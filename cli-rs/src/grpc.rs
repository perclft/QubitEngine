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
            classical_register: op.classical_reg,
        });
    }

    let req = tonic::Request::new(crate::api::CircuitRequest {
        num_qubits: circuit.qubits,
        operations: ops,
        noise_probability: 0.0,
        execution_backend: 0,
        measurement_strategy: 0,
        use_shm: false,
    });

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

    let mut step = 1;
    while let Ok(Some(state_res)) = stream.message().await {
        let _ = tx
            .send(AppEvent::Grpc(GrpcEvent::Log(format!(
                "Received step {} wavefunction.",
                step
            ))))
            .await;

        // Extract top-10 probabilities using a min-heap (O(N log K) instead of O(N log N))
        let k = 10;
        // Heap of (prob_bits, label) — to_bits() preserves f64 ordering for positive values
        // and satisfies Ord since it's a u64. We reconstruct the f64 on extraction.
        let mut heap: BinaryHeap<Reverse<(u64, String)>> = BinaryHeap::with_capacity(k + 1);

        for (i, c) in state_res.state_vector.iter().enumerate() {
            let prob = c.real * c.real + c.imag * c.imag;
            if prob < 1e-8 {
                // Configurable noise floor — skip truly negligible amplitudes
                continue;
            }
            // to_bits() gives zero-cost Ord-compliant integer without precision loss
            heap.push(Reverse((prob.to_bits(), format!("|{}>", i))));
            if heap.len() > k {
                heap.pop(); // evict the smallest
            }
        }

        // Drain, reconstruct f64 from bits, convert to percentage, sort highest first
        let mut probs: Vec<(String, u64)> = heap
            .into_sorted_vec()
            .into_iter()
            .rev()
            .map(|Reverse((bits, label))| (label, (f64::from_bits(bits) * 100.0) as u64))
            .collect();
        probs.sort_by(|a, b| b.1.cmp(&a.1));

        // try_send: don't block the gRPC receiver if the UI channel is full
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

    let req = tonic::Request::new(crate::api::VqeRequest {
        molecule: 0, // H2
        max_iterations: 100,
        learning_rate: 0.2,  // Faster convergence
        optimizer_type: 1,   // GRADIENT_DESCENT
        observables: vec![], // Added missing observables
    });

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

    let req = tonic::Request::new(crate::api::HardwareTopologyRequest {});

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
