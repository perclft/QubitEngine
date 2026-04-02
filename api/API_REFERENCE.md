# QubitEngine API Reference

This document provides a comprehensive guide to the gRPC API endpoints for the QubitEngine backend.

## Service: `QuantumCompute`

The primary service for executing quantum circuits and algorithms.

### `RunCircuit`
Executes a fixed circuit and returns the final state vector. Best for small to medium-sized simulations.

- **Request**: `CircuitRequest`
- **Response**: `StateResponse`
- **Behavior**: Synchronous

### `StreamGates`
A bidirectional streaming endpoint. The client sends individual gates and receives the updated state vector after each operation.

- **Request**: `stream GateStreamRequest`
- **Response**: `stream StateResponse`
- **Protocol**: 
  1. Client sends `GateStreamInit` (with `num_qubits`).
  2. Client sends one or more `GateOperation`.
  3. Server returns `StateResponse` for each operation.

### `VisualizeCircuit`
Server-side streaming. Executes a full circuit and streams back the state after **every** gate. Optimized for web-based circuit visualizers (e.g., gRPC-Web).

- **Request**: `CircuitRequest`
- **Response**: `stream StateResponse`

### `RunVQE`
Executes a Variational Quantum Eigensolver (VQE) algorithm for quantum chemistry simulations. Streams intermediate results (iteration, energy) as the optimizer converges.

- **Request**: `VQERequest`
- **Response**: `stream VQEResponse`

### `GetHardwareTopology`
Returns the physical qubit connectivity and layout of the backend.

- **Request**: `HardwareTopologyRequest`
- **Response**: `HardwareTopologyResponse`

---

## Data Structures

### `CircuitRequest`
| Field | Type | Description |
| :--- | :--- | :--- |
| `num_qubits` | `int32` | Total qubits in the simulation. |
| `operations` | `repeated GateOperation` | List of gates to execute. |
| `noise_probability` | `double` | Depolarizing noise level (0.0 to 1.0). |
| `execution_backend` | `enum` | `SIMULATOR`, `MOCK_HARDWARE`. |

### `GateOperation`
| Field | Type | Description |
| :--- | :--- | :--- |
| `type` | `GateType` | `HADAMARD`, `PAULI_X`, `CNOT`, `ROTATION_Y`, etc. |
| `target_qubit` | `uint32` | Index of the target qubit. |
| `control_qubit` | `uint32` | Index of the control qubit (for 2-qubit gates). |
| `angle` | `double` | Rotation angle in radians (for R gates). |

### `StateResponse`
| Field | Type | Description |
| :--- | :--- | :--- |
| `state_vector` | `repeated ComplexNumber` | The full complex state vector. |
| `classical_results` | `map<uint32, bool>` | Measured classical bits. |
| `server_id` | `string` | ID of the worker that processed the job. |
| `shm_descriptor` | `string` | Shared memory segment name (for zero-copy IPC). |
