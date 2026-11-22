use crate::api::GateOperation;
use crate::api::quantum_compute_client::QuantumComputeClient;
use crate::api::ShmAckRequest;
use crate::framework::AppEvent;
use serde::Deserialize;
use std::cmp::Reverse;
use std::collections::BinaryHeap;
use std::fs;
use tokio::sync::mpsc;

#[derive(Deserialize, Debug)]
#[allow(dead_code)]
pub struct ShmDescriptor {
    pub format_version: u32,
    pub segment_name: String,
    pub size_bytes: u64,
    pub num_qubits: u32,
    pub data_type: String,
    pub created_at_ms: i64,
    pub ack_token: String,
}

#[cfg(windows)]
pub fn read_shm_amplitudes(segment_name: &str, size_bytes: usize) -> Result<Vec<(f64, f64)>, String> {
    use std::ffi::CString;
    use windows_sys::Win32::Foundation::CloseHandle;
    use windows_sys::Win32::System::Memory::{
        FILE_MAP_READ, MapViewOfFile, OpenFileMappingA, UnmapViewOfFile,
    };

    let c_name = CString::new(segment_name).map_err(|e| format!("CString error: {}", e))?;
    let handle = unsafe { OpenFileMappingA(FILE_MAP_READ, 0, c_name.as_ptr() as *const u8) };
    if handle.is_null() {
        return Err(format!("OpenFileMappingA failed for {}", segment_name));
    }

    let ptr = unsafe { MapViewOfFile(handle, FILE_MAP_READ, 0, 0, size_bytes) };
    if ptr.Value.is_null() {
        unsafe { CloseHandle(handle) };
        return Err(format!("MapViewOfFile failed for {}", segment_name));
    }

    let num_elements = size_bytes / 16;
    let slice: &[(f64, f64)] = unsafe {
        std::slice::from_raw_parts(ptr.Value as *const (f64, f64), num_elements)
    };
    let data = slice.to_vec();

    unsafe {
        UnmapViewOfFile(ptr);
        CloseHandle(handle);
    }
    Ok(data)
}

#[cfg(unix)]
pub fn read_shm_amplitudes(segment_name: &str, size_bytes: usize) -> Result<Vec<(f64, f64)>, String> {
    use std::ffi::CString;

    let c_name = CString::new(segment_name).map_err(|e| format!("CString error: {}", e))?;
    let fd = unsafe { libc::shm_open(c_name.as_ptr(), libc::O_RDONLY, 0o600) };
    if fd == -1 {
        return Err(format!("shm_open failed for {}", segment_name));
    }

    let ptr = unsafe {
        libc::mmap(
            std::ptr::null_mut(),
            size_bytes,
            libc::PROT_READ,
            libc::MAP_SHARED,
            fd,
            0,
        )
    };
    unsafe { libc::close(fd) };

    if ptr == libc::MAP_FAILED {
        return Err(format!("mmap failed for {}", segment_name));
    }

    let num_elements = size_bytes / 16;
    let slice: &[(f64, f64)] = unsafe {
        std::slice::from_raw_parts(ptr as *const (f64, f64), num_elements)
    };
    let data = slice.to_vec();

    unsafe { libc::munmap(ptr, size_bytes) };
    Ok(data)
}

#[allow(clippy::collapsible_if)]
pub fn process_state_response(
    state_res: &crate::api::StateResponse,
    heap: &mut BinaryHeap<Reverse<(u64, usize)>>,
    k: usize,
    client: &mut QuantumComputeClient<tonic::transport::Channel>,
    token: &str,
) {
    let mut processed_via_shm = false;
    if !state_res.shm_descriptor.is_empty() {
        if let Ok(desc) = serde_json::from_str::<ShmDescriptor>(&state_res.shm_descriptor) {
            let now_ms = chrono::Utc::now().timestamp_millis();
            let age = (now_ms - desc.created_at_ms).abs();
            if desc.format_version == 1 && age <= 3000 {
                if let Ok(amplitudes) = read_shm_amplitudes(&desc.segment_name, desc.size_bytes as usize) {
                    for (i, (real, imag)) in amplitudes.iter().enumerate() {
                        let prob = real * real + imag * imag;
                        if prob < 1e-8 {
                            continue;
                        }
                        heap.push(Reverse((prob.to_bits(), i)));
                        if heap.len() > k {
                            heap.pop();
                        }
                    }
                    processed_via_shm = true;

                    if !desc.ack_token.is_empty() {
                        let mut ack_client = client.clone();
                        let token_owned = token.to_string();
                        let ack_req = ShmAckRequest {
                            ack_token: desc.ack_token,
                        };
                        tokio::spawn(async move {
                            let mut req = tonic::Request::new(ack_req);
                            if let Ok(meta_value) = format!("Bearer {}", token_owned).parse() {
                                req.metadata_mut().insert("authorization", meta_value);
                            }
                            let _ = ack_client.acknowledge_shm_read(req).await;
                        });
                    }
                }
            }
        }
    }

    if !processed_via_shm {
        for (i, c) in state_res.state_vector.iter().enumerate() {
            let prob = c.real * c.real + c.imag * c.imag;
            if prob < 1e-8 {
                continue;
            }
            heap.push(Reverse((prob.to_bits(), i)));
            if heap.len() > k {
                heap.pop();
            }
        }
    }
}

#[derive(Deserialize, Debug)]
struct CircuitFile {
    name: String,
    qubits: i32,
    ops: Vec<OperationDefinition>,
}

#[derive(Deserialize, Debug)]
struct OperationDefinition {
    gate: String,
    target: u32,
    #[serde(default)]
    control: u32,
    #[serde(default)]
    control2: u32,
    #[serde(default)]
    angle: f64,
    #[serde(default)]
    classical_reg: u32,
}

pub enum GrpcEvent {
    Log(String),
    Wavefunction(Vec<(String, u64)>), // pre-sorted top-K probabilities
    VqeUpdate(i32, f64, bool),        // iteration, energy, converged
    Completed(String),
    Error(String),
    Topology(Vec<(f64, f64)>, Vec<(usize, usize)>), // nodes(x,y), edges(n1, n2)
}

pub async fn run_circuit(server_addr: String, circuit_path: String, tx: mpsc::Sender<AppEvent>) {
    let _ = tx
        .send(AppEvent::Grpc(GrpcEvent::Log(format!(
            "Connecting to {}",
            server_addr
        ))))
        .await;

    // Connect
    let client_res = QuantumComputeClient::connect(server_addr).await;
    let mut client = match client_res {
        Ok(c) => c,
        Err(e) => {
            let _ = tx
                .send(AppEvent::Grpc(GrpcEvent::Error(format!(
                    "Connection failed: {}",
                    e
                ))))
                .await;
            return;
        }
    };

    let _ = tx
        .send(AppEvent::Grpc(GrpcEvent::Log(
            "Parsing circuit JSON...".to_string(),
        )))
        .await;
    let data = match fs::read_to_string(&circuit_path) {
        Ok(data) => data,
        Err(e) => {
            let _ = tx
                .send(AppEvent::Grpc(GrpcEvent::Error(format!("IO Error: {}", e))))
                .await;
            return;
        }
    };

    let circuit: CircuitFile = match serde_json::from_str(&data) {
        Ok(cir) => cir,
        Err(e) => {
            let _ = tx
                .send(AppEvent::Grpc(GrpcEvent::Error(format!(
                    "Invalid JSON: {}",
                    e
                ))))
                .await;
            return;
        }
    };

    let _ = tx
        .send(AppEvent::Grpc(GrpcEvent::Log(format!(
            "Parsed '{}' ({} qubits)",
            circuit.name, circuit.qubits
        ))))
        .await;

    let mut ops = Vec::new();
    for op in circuit.ops {
        let op_type = match op.gate.to_uppercase().as_str() {
            "H" => 0,
            "X" => 1,
            "CNOT" => 2,
            "M" => 3,
            "TOFFOLI" | "CCNOT" => 4,
            "S" => 5,
            "T" => 6,
            "RY" => 7,
            "RZ" => 8,
            _ => 1, // Default PauliX if unknown
        };

        ops.push(GateOperation {
            r#type: op_type,
            target_qubit: op.target,
            control_qubit: op.control,
            second_control_qubit: op.control2,
            second_target_qubit: 0,
            angle: op.angle,
            noise_probability: 0.0,
            noise_gamma: 0.0,
            classical_register: op.classical_reg,
        });
    }

    let mut req = tonic::Request::new(crate::api::CircuitRequest {
        num_qubits: circuit.qubits,
        operations: ops,
        noise_probability: 0.0,
        noise_config: None,
        execution_backend: 0,
        measurement_strategy: 0,
        use_shm: circuit.qubits >= 12,
        version: "1.0".to_string(),
    });

    let token = std::env::var("QUBIT_ENGINE_AUTH_TOKEN").unwrap_or_else(|_| "default-secret-token".to_string());
    if let Ok(meta_value) = format!("Bearer {}", token).parse() {
        req.metadata_mut().insert("authorization", meta_value);
    }

    let _ = tx
        .send(AppEvent::Grpc(GrpcEvent::Log(String::from(
            "Requesting Visualization Stream...",
        ))))
        .await;

    let res = client.visualize_circuit(req).await;
    let mut stream = match res {
        Ok(s) => s.into_inner(),
        Err(e) => {
            let _ = tx
                .send(AppEvent::Grpc(GrpcEvent::Error(format!(
                    "Stream request failed: {}",
                    e
                ))))
                .await;
            return;
        }
    };

    // Hoist heap allocation outside the stream loop to reuse capacity
    let k = 10;
    let mut heap: BinaryHeap<Reverse<(u64, usize)>> = BinaryHeap::with_capacity(k + 1);
    let mut step = 1;

    while let Ok(Some(state_res)) = stream.message().await {
        let _ = tx.try_send(AppEvent::Grpc(GrpcEvent::Log(format!(
            "Received step {} wavefunction.",
            step
        ))));

        heap.clear(); // O(1) clear, retains allocated capacity

        process_state_response(&state_res, &mut heap, k, &mut client, &token);

        // Drain and perform exactly K string allocations (deferred formatting)
        let num_qubits = circuit.qubits as usize;
        let mut probs: Vec<(String, u64)> = heap
            .drain()
            .map(|Reverse((bits, index))| {
                (
                    format_basis_state(index, num_qubits),
                    (f64::from_bits(bits) * 100.0) as u64,
                )
            })
            .collect();
        probs.sort_unstable_by_key(|b| std::cmp::Reverse(b.1));

        let _ = tx.try_send(AppEvent::Grpc(GrpcEvent::Wavefunction(probs)));
        step += 1;
    }

    let _ = tx
        .send(AppEvent::Grpc(GrpcEvent::Completed(format!(
            "Simulation Completed ({} steps)",
            step - 1
        ))))
        .await;
}

pub async fn run_vqe(server_addr: String, tx: mpsc::Sender<AppEvent>) {
    let _ = tx
        .send(AppEvent::Grpc(GrpcEvent::Log(format!(
            "Connecting to {} for VQE...",
            server_addr
        ))))
        .await;

    // Connect
    let client_res =
        crate::api::quantum_compute_client::QuantumComputeClient::connect(server_addr).await;
    let mut client = match client_res {
        Ok(c) => c,
        Err(e) => {
            let _ = tx
                .send(AppEvent::Grpc(GrpcEvent::Error(format!(
                    "Connection failed: {}",
                    e
                ))))
                .await;
            return;
        }
    };

    let _ = tx
        .send(AppEvent::Grpc(GrpcEvent::Log(
            "Configuring Hydrogen (H2) VQE via Parameter Shift...".to_string(),
        )))
        .await;

    #[allow(deprecated)]
    let mut req = tonic::Request::new(crate::api::VqeRequest {
        molecule: 0, // H2
        max_iterations: 100,
        learning_rate: 0.2,  // Faster convergence
        optimizer_type: 1,   // GRADIENT_DESCENT
        observables: vec![], // Added missing observables
    });

    let token = std::env::var("QUBIT_ENGINE_AUTH_TOKEN").unwrap_or_else(|_| "default-secret-token".to_string());
    if let Ok(meta_value) = format!("Bearer {}", token).parse() {
        req.metadata_mut().insert("authorization", meta_value);
    }

    let res = client.run_vqe(req).await;
    let mut stream = match res {
        Ok(s) => s.into_inner(),
        Err(e) => {
            let _ = tx
                .send(AppEvent::Grpc(GrpcEvent::Error(format!(
                    "VQE stream request failed: {}",
                    e
                ))))
                .await;
            return;
        }
    };

    while let Ok(Some(update)) = stream.message().await {
        let _ = tx
            .send(AppEvent::Grpc(GrpcEvent::VqeUpdate(
                update.iteration,
                update.energy,
                update.converged,
            )))
            .await;
    }

    let _ = tx
        .send(AppEvent::Grpc(GrpcEvent::Completed(String::from(
            "VQE Optimization Completed!",
        ))))
        .await;
}

pub async fn get_topology(server_addr: String, tx: mpsc::Sender<AppEvent>) {
    let client_res =
        crate::api::quantum_compute_client::QuantumComputeClient::connect(server_addr.clone())
            .await;
    let mut client = match client_res {
        Ok(c) => c,
        Err(e) => {
            let _ = tx
                .send(AppEvent::Grpc(GrpcEvent::Error(format!(
                    "Topology connect error: {}",
                    e
                ))))
                .await;
            return;
        }
    };

    let mut req = tonic::Request::new(crate::api::HardwareTopologyRequest {});

    let token = std::env::var("QUBIT_ENGINE_AUTH_TOKEN").unwrap_or_else(|_| "default-secret-token".to_string());
    if let Ok(meta_value) = format!("Bearer {}", token).parse() {
        req.metadata_mut().insert("authorization", meta_value);
    }

    match client.get_hardware_topology(req).await {
        Ok(res) => {
            let topo = res.into_inner();
            let mut nodes = Vec::new();
            let mut edges = Vec::new();

            for n in topo.nodes {
                nodes.push((n.x, n.y));
            }
            for e in topo.edges {
                edges.push((e.node1 as usize, e.node2 as usize));
            }

            let _ = tx
                .send(AppEvent::Grpc(GrpcEvent::Topology(nodes, edges)))
                .await;
        }
        Err(e) => {
            let _ = tx
                .send(AppEvent::Grpc(GrpcEvent::Error(format!(
                    "Topology fetch error: {}",
                    e
                ))))
                .await;
        }
    }
}

pub async fn run_circuit_headless(server_addr: String, circuit_path: String) -> Result<Vec<(String, u64)>, Box<dyn std::error::Error>> {
    let mut client = QuantumComputeClient::connect(server_addr).await?;
    let data = fs::read_to_string(&circuit_path)?;
    let circuit: CircuitFile = serde_json::from_str(&data)?;

    let mut ops = Vec::new();
    for op in circuit.ops {
        let op_type = match op.gate.to_uppercase().as_str() {
            "H" => 0,
            "X" => 1,
            "CNOT" => 2,
            "M" => 3,
            "TOFFOLI" | "CCNOT" => 4,
            "S" => 5,
            "T" => 6,
            "RY" => 7,
            "RZ" => 8,
            _ => 1,
        };
        ops.push(GateOperation {
            r#type: op_type,
            target_qubit: op.target,
            control_qubit: op.control,
            second_control_qubit: op.control2,
            second_target_qubit: 0,
            angle: op.angle,
            noise_probability: 0.0,
            noise_gamma: 0.0,
            classical_register: op.classical_reg,
        });
    }

    let mut req = tonic::Request::new(crate::api::CircuitRequest {
        num_qubits: circuit.qubits,
        operations: ops,
        noise_probability: 0.0,
        noise_config: None,
        execution_backend: 0,
        measurement_strategy: 0,
        use_shm: circuit.qubits >= 12,
        version: "1.0".to_string(),
    });

    let token = std::env::var("QUBIT_ENGINE_AUTH_TOKEN").unwrap_or_else(|_| "default-secret-token".to_string());
    if let Ok(meta_value) = format!("Bearer {}", token).parse() {
        req.metadata_mut().insert("authorization", meta_value);
    }

    let res = client.run_circuit(req).await?;
    let state_res = res.into_inner();

    let k = 10;
    let mut heap: BinaryHeap<Reverse<(u64, usize)>> = BinaryHeap::with_capacity(k + 1);
    process_state_response(&state_res, &mut heap, k, &mut client, &token);

    let num_qubits = circuit.qubits as usize;
    let mut probs: Vec<(String, u64)> = heap
        .drain()
        .map(|Reverse((bits, index))| {
            (
                format_basis_state(index, num_qubits),
                (f64::from_bits(bits) * 100.0) as u64,
            )
        })
        .collect();
    probs.sort_unstable_by_key(|b| std::cmp::Reverse(b.1));

    Ok(probs)
}

pub fn format_basis_state(index: usize, num_qubits: usize) -> String {
    format!("|{:0width$b}>", index, width = num_qubits)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_format_basis_state() {
        assert_eq!(format_basis_state(3, 3), "|011>");
        assert_eq!(format_basis_state(0, 2), "|00>");
        assert_eq!(format_basis_state(7, 3), "|111>");
    }
}

