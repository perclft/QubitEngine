# QubitEngine API Reference

This document provides a detailed reference for the QubitEngine gRPC API definitions found in `api/proto/quantum.proto`.

## Services

### `QuantumCompute`
The primary service for executing quantum circuits and variational algorithms.

| Method | Request Type | Response Type | Description |
| :--- | :--- | :--- | :--- |
| `RunCircuit` | `CircuitRequest` | `StateResponse` | Synchronously executes a batch of operations. |
| `StreamGates` | `stream GateOperation` | `stream StateResponse` (bidirectional) | Executes gates in real-time as they arrive and returns state updates. |
| `VisualizeCircuit` | `CircuitRequest` | `stream StateResponse` (server-streaming) | Executes a circuit and streams back the state after *every* gate. |
| `RunVQE` | `VQERequest` | `stream VQEResponse` (server-streaming) | Runs a Variational Quantum Eigensolver optimization loop. |
| `GetHardwareTopology` | `HardwareTopologyRequest` | `HardwareTopologyResponse` | Retrieves the physical/mock qubit connectivity graph. |

---

## Message Definitions

### `CircuitRequest`
Defines a quantum circuit and execution parameters.

- `num_qubits` (int32): Total qubits in the register.
- `operations` (repeated `GateOperation`): The gate sequence to execute.
- `noise_probability` (double): Error rate for the depolarizing noise model.
- `execution_backend` (Enum): Choice of simulator or hardware.
- `measurement_strategy` (Enum): How to return results (Full state vs. expectation values).
- `noise_config` (`NoiseConfig`): Structured noise model configuration. Overrides `noise_probability` when set.

### `GateOperation`
A single quantum instruction.

- `type` (GateType Enum): The gate to apply (HADAMARD, CNOT, etc.).
- `target_qubit` (uint32): Primary qubit index.
- `control_qubit` (uint32): Control qubit (for CNOT/CZ).
- `angle` (double): Rotation angle in radians (for RX/RY/RZ).
- `noise_probability` (double): Local error rate override (for DEPOLARIZING_NOISE).
- `noise_gamma` (double): Decay/dephasing rate in [0, 1] (for AMPLITUDE_DAMPING / PHASE_DAMPING).

### `StateResponse`
The results of a circuit execution or step.

- `state_vector` (repeated `ComplexNumber`): The full 2^N amplitude vector.
- `classical_results` (map<uint32, bool>): Measured bit values.
- `server_id` (string): Metadata identifying the processing node.
- `shm_descriptor` (string): Shared memory key for zero-copy IPC.

### `VQERequest`
Parameters for variational optimization.

- `max_iterations` (int32): Shutdown threshold for optimization.
- `learning_rate` (double): Step size for gradient-based methods.
- `optimizer_type` (Enum): Choice of SPSA or Gradient Descent (Parameter Shift).
- `observables` (repeated `PauliTerm`): The Hamiltonian to minimize.

---

## Enums

### `GateType`
- `HADAMARD`, `PAULI_X`, `PAULI_Y`, `PAULI_Z`
- `CNOT`, `TOFFOLI`, `SWAP`, `CZ`
- `ROTATION_X`, `ROTATION_Y`, `ROTATION_Z`
- `PHASE_S`, `PHASE_T`
- `MEASURE`, `DEPOLARIZING_NOISE`
- `AMPLITUDE_DAMPING`, `PHASE_DAMPING`

### `ExecutionBackend`
- `SIMULATOR` (0)
- `MOCK_HARDWARE` (1)
- `REAL_IBM_Q` (2)

---

## Noise Configuration

### `NoiseConfig`
Structured noise model configuration for realistic simulations. Attach to `CircuitRequest` to apply noise automatically.

- `depolarizing_1q` (double): Single-qubit depolarizing error probability (typ. ~0.001)
- `depolarizing_2q` (double): Two-qubit depolarizing error probability (typ. ~0.01)
- `amplitude_damping` (double): T1 amplitude damping gamma, γ = 1 - exp(-t/T1)
- `phase_damping` (double): T2 phase damping gamma, γ = 1 - exp(-t/T2)
- `readout_p0_given_1` (double): Readout error probability P(measure 0 | true state is |1⟩)
- `readout_p1_given_0` (double): Readout error probability P(measure 1 | true state is |0⟩)
