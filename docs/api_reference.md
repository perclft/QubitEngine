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

### `GateOperation`
A single quantum instruction.

- `type` (GateType Enum): The gate to apply (HADAMARD, CNOT, etc.).
- `target_qubit` (uint32): Primary qubit index.
- `control_qubit` (uint32): Control qubit (for CNOT/CZ).
- `angle` (double): Rotation angle in radians (for RX/RY/RZ).
- `noise_probability` (double): Local error rate override.

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

### `ExecutionBackend`
- `SIMULATOR` (0)
- `MOCK_HARDWARE` (1)
- `REAL_IBM_Q` (2)
