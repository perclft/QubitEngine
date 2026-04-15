use crate::api::GateOperation;
use crate::api::quantum_compute_client::QuantumComputeClient;
use crate::framework::AppEvent;
use serde::Deserialize;
use std::cmp::Reverse;
use std::collections::BinaryHeap;
use std::fs;
use tokio::sync::mpsc;

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
        use_shm: false,
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

        // Hot loop: strictly stack variables, zero heap allocations
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

        // Drain and perform exactly K string allocations (deferred formatting)
        let mut probs: Vec<(String, u64)> = heap
            .drain()
            .map(|Reverse((bits, index))| {
                (
                    format!("|{}>", index),
                    (f64::from_bits(bits) * 100.0) as u64,
                )
            })
            .collect();
        probs.sort_unstable_by(|a, b| b.1.cmp(&a.1));

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
