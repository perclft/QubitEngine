use crate::api::GateOperation;
use crate::api::quantum_compute_client::QuantumComputeClient;
use crate::framework::AppEvent;
use serde::Deserialize;
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
    Wavefunction(Vec<(usize, f64, f64)>), // index, real, imag
    VqeUpdate(i32, f64, bool),            // iteration, energy, converged
    Completed(String),
    Error(String),
}

pub async fn run_circuit(
    server_addr: String,
    circuit_path: String,
    tx: mpsc::UnboundedSender<AppEvent>,
) {
    let _ = tx.send(AppEvent::Grpc(GrpcEvent::Log(format!(
        "Connecting to {}",
        server_addr
    ))));

    // Connect
    let client_res = QuantumComputeClient::connect(server_addr).await;
    let mut client = match client_res {
        Ok(c) => c,
        Err(e) => {
            let _ = tx.send(AppEvent::Grpc(GrpcEvent::Error(format!(
                "Connection failed: {}",
                e
            ))));
            return;
        }
    };

    let _ = tx.send(AppEvent::Grpc(GrpcEvent::Log(
        "Parsing circuit JSON...".to_string(),
    )));
    let data = match fs::read_to_string(&circuit_path) {
        Ok(data) => data,
        Err(e) => {
            let _ = tx.send(AppEvent::Grpc(GrpcEvent::Error(format!("IO Error: {}", e))));
            return;
        }
    };

    let circuit: CircuitFile = match serde_json::from_str(&data) {
        Ok(cir) => cir,
        Err(e) => {
            let _ = tx.send(AppEvent::Grpc(GrpcEvent::Error(format!(
                "Invalid JSON: {}",
                e
            ))));
            return;
        }
    };

    let _ = tx.send(AppEvent::Grpc(GrpcEvent::Log(format!(
        "Parsed '{}' ({} qubits)",
        circuit.name, circuit.qubits
    ))));

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

    let _ = tx.send(AppEvent::Grpc(GrpcEvent::Log(String::from(
        "Requesting Visualization Stream...",
    ))));

    let res = client.visualize_circuit(req).await;
    let mut stream = match res {
        Ok(s) => s.into_inner(),
        Err(e) => {
            let _ = tx.send(AppEvent::Grpc(GrpcEvent::Error(format!(
                "Stream request failed: {}",
                e
            ))));
            return;
        }
    };

    let mut step = 1;
    while let Ok(Some(state_res)) = stream.message().await {
        let _ = tx.send(AppEvent::Grpc(GrpcEvent::Log(format!(
            "Received step {} wavefunction.",
            step
        ))));

        let mut non_zero_amps = Vec::new();
        for (i, c) in state_res.state_vector.iter().enumerate() {
            let mag = c.real * c.real + c.imag * c.imag;
            if mag > 0.0001 {
                non_zero_amps.push((i, c.real, c.imag));
            }
        }

        let _ = tx.send(AppEvent::Grpc(GrpcEvent::Wavefunction(non_zero_amps)));
        step += 1;
    }

    let _ = tx.send(AppEvent::Grpc(GrpcEvent::Completed(format!(
        "Simulation Completed ({} steps)",
        step - 1
    ))));
}

pub async fn run_vqe(server_addr: String, tx: mpsc::UnboundedSender<AppEvent>) {
    let _ = tx.send(AppEvent::Grpc(GrpcEvent::Log(format!(
        "Connecting to {} for VQE...",
        server_addr
    ))));

    // Connect
    let client_res =
        crate::api::quantum_compute_client::QuantumComputeClient::connect(server_addr).await;
    let mut client = match client_res {
        Ok(c) => c,
        Err(e) => {
            let _ = tx.send(AppEvent::Grpc(GrpcEvent::Error(format!(
                "Connection failed: {}",
                e
            ))));
            return;
        }
    };

    let _ = tx.send(AppEvent::Grpc(GrpcEvent::Log(
        "Configuring Hydrogen (H2) VQE via Parameter Shift...".to_string(),
    )));

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
            let _ = tx.send(AppEvent::Grpc(GrpcEvent::Error(format!(
                "VQE stream request failed: {}",
                e
            ))));
            return;
        }
    };

    while let Ok(Some(update)) = stream.message().await {
        let _ = tx.send(AppEvent::Grpc(GrpcEvent::VqeUpdate(
            update.iteration,
            update.energy,
            update.converged,
        )));
    }

    let _ = tx.send(AppEvent::Grpc(GrpcEvent::Completed(String::from(
        "VQE Optimization Completed!",
    ))));
}
