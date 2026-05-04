# QubitEngine Python API Reference

QubitEngine exposes its high-performance C++20 simulation kernel to Python via `pybind11`. All gate operations, noise simulation, gradient computation, and optimization execute natively in compiled C++ — the Python layer is a zero-overhead interface, not a reimplementation.

## Installation

```bash
cd backend
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build -j$(nproc)

# The compiled module `core.cpython-*.so` will be in python/build/
# Add it to your PYTHONPATH or install via pip:
pip install -e .
```

```python
import qubit_engine
```

---

## Core Classes

### `QuantumRegister`

The primary simulation object. Wraps the C++ `QuantumRegister`, which automatically selects the optimal backend (CUDA → Metal → MPI → CPU) at construction time.

```python
qreg = qubit_engine.QuantumRegister(num_qubits, force_local=False)
```

| Parameter | Type | Default | Description |
|---|---|---|---|
| `num_qubits` | `int` | *required* | Number of qubits in the register |
| `force_local` | `bool` | `False` | If `True`, forces single-process CPU execution (bypasses MPI). Required for gradient calculations where each parameter evaluation needs an independent register. |

#### Gate Operations

All gates mutate the register's internal state vector in-place.

**Single-Qubit Gates**

| Method | Parameters | Description |
|---|---|---|
| `applyHadamard(target)` | `target: int` | Apply Hadamard gate |
| `applyX(target)` | `target: int` | Apply Pauli-X (NOT) gate |
| `applyY(target)` | `target: int` | Apply Pauli-Y gate |
| `applyZ(target)` | `target: int` | Apply Pauli-Z gate |
| `applyRotationX(target, angle)` | `target: int, angle: float` | Apply Rx(θ) rotation |
| `applyRotationY(target, angle)` | `target: int, angle: float` | Apply Ry(θ) rotation |
| `applyRotationZ(target, angle)` | `target: int, angle: float` | Apply Rz(θ) rotation |
| `applyPhaseS(target)` | `target: int` | Apply S gate (π/2 phase) |
| `applyPhaseT(target)` | `target: int` | Apply T gate (π/4 phase) |

**Multi-Qubit Gates**

| Method | Parameters | Description |
|---|---|---|
| `applyCNOT(control, target)` | `control: int, target: int` | Controlled-NOT gate |
| `applyCZ(control, target)` | `control: int, target: int` | Controlled-Z gate |
| `applySWAP(qubit1, qubit2)` | `qubit1: int, qubit2: int` | SWAP gate |
| `applyToffoli(c1, c2, target)` | `c1: int, c2: int, target: int` | Toffoli (CCX) gate |

#### Measurement & Observation

| Method | Returns | Description |
|---|---|---|
| `measure(target)` | `int` | Projective measurement on a single qubit. Returns 0 or 1 and collapses the state. |
| `getProbabilities()` | `list[float]` | Returns the probability distribution `|α_i|²` for all 2^N basis states. |
| `expectationValue(pauli_string)` | `float` | Compute ⟨ψ\|P\|ψ⟩ for a Pauli string. Example: `"ZZI"` measures Z⊗Z⊗I. Supports `I`, `X`, `Y`, `Z` characters. |
| `getStateVector()` | `numpy.ndarray[complex128]` | Returns the full state vector as a NumPy array. Zero-copy when possible via `pybind11::buffer_info`. |

#### State Inspection

| Method | Returns | Description |
|---|---|---|
| `getRank()` | `int` | MPI rank of this process (0 if not using MPI) |
| `getSize()` | `int` | Total MPI world size (1 if not using MPI) |

#### Noise

| Method | Parameters | Description |
|---|---|---|
| `setNoiseModel(model)` | `model: NoiseModel` | Attach a noise model. All subsequent gate operations will automatically inject noise. |
| `getNoiseModel()` | — | Returns the currently attached `NoiseModel` or `None`. |

#### Example: Bell State

```python
import qubit_engine
import numpy as np

qreg = qubit_engine.QuantumRegister(2)
qreg.applyHadamard(0)
qreg.applyCNOT(0, 1)

state = qreg.getStateVector()
print(state)  # [0.707+0j, 0+0j, 0+0j, 0.707+0j]

print(qreg.expectationValue("ZZ"))  # 1.0  (perfectly correlated)
print(qreg.expectationValue("XX"))  # 1.0
```

---

### `GPUQuantumRegister`

A GPU-accelerated register (Metal on macOS, CUDA on Linux/Windows). Provides a subset of the `QuantumRegister` API for direct GPU control.

```python
qreg = qubit_engine.GPUQuantumRegister(num_qubits)
```

| Method | Description |
|---|---|
| `applyHadamard(target)` | Hadamard gate on GPU |
| `applyX(target)` | Pauli-X gate on GPU |
| `applyY(target)` | Pauli-Y gate on GPU |
| `applyZ(target)` | Pauli-Z gate on GPU |
| `applyRotationY(target, angle)` | Ry rotation on GPU |
| `getStateVector()` | Copy state vector from GPU → CPU as NumPy array |

> **Note**: For most use cases, prefer `QuantumRegister`, which auto-selects the GPU backend when available. Use `GPUQuantumRegister` only when you need explicit GPU resource control.

---

### `StabilizerBackend`

Efficient Clifford-circuit simulator using the Gottesman–Knill tableau representation. Simulates thousands of qubits in polynomial time, but only supports Clifford gates (H, X, Y, Z, CNOT, S, CZ, SWAP).

```python
stab = qubit_engine.StabilizerBackend(num_qubits=100)
```

| Method | Parameters | Description |
|---|---|---|
| `applyHadamard(target)` | `target: int` | Hadamard gate |
| `applyX(target)` | `target: int` | Pauli-X gate |
| `applyY(target)` | `target: int` | Pauli-Y gate |
| `applyZ(target)` | `target: int` | Pauli-Z gate |
| `applyCNOT(control, target)` | `control: int, target: int` | CNOT gate |
| `applyPhaseS(target)` | `target: int` | S gate |
| `applyPhaseT(target)` | `target: int` | **Raises exception** — T is non-Clifford |
| `applyCZ(control, target)` | `control: int, target: int` | CZ gate |
| `applySWAP(qubit1, qubit2)` | `qubit1: int, qubit2: int` | SWAP gate |
| `measure(target)` | `target: int` | Projective measurement → 0 or 1 |
| `get_state_vector()` | — | Returns full state vector as NumPy array. **Warning**: O(2^N) memory — only feasible for small N. |
| `num_qubits()` | — | Returns the number of qubits |

---

## Noise Modeling

QubitEngine implements stochastic noise simulation using Kraus operators. Noise is injected automatically after every gate when a `NoiseModel` is attached to a register.

### `NoiseModel`

```python
model = qubit_engine.NoiseModel()
```

#### Convenience Constructors

```python
# Simple depolarizing noise (typical NISQ error rates)
model = qubit_engine.NoiseModel.Depolarizing(p1q=0.001, p2q=0.01)

# Full realistic model with T1, T2, and readout errors
model = qubit_engine.NoiseModel.Realistic(
    p1q=0.001,       # 1Q depolarizing probability
    p2q=0.01,        # 2Q depolarizing probability
    t1_gamma=0.005,  # Amplitude damping γ = 1 - exp(-t/T1)
    t2_gamma=0.01,   # Phase damping γ = 1 - exp(-t/T2)
    readout=qubit_engine.ReadoutError(p0_given_1=0.02, p1_given_0=0.01)
)
```

#### Configuration Methods

| Method | Parameters | Description |
|---|---|---|
| `add_single_qubit_noise(channel)` | `channel: NoiseChannel1Q` | Add a custom single-qubit noise channel |
| `add_two_qubit_noise(channel)` | `channel: NoiseChannel2Q` | Add a custom two-qubit noise channel |
| `set_readout_error(qubit, error)` | `qubit: int, error: ReadoutError` | Set readout error for a specific qubit |
| `set_readout_error_all(error)` | `error: ReadoutError` | Set default readout error for all qubits |
| `set_enabled(enabled)` | `enabled: bool` | Enable/disable noise injection |
| `is_enabled()` | — | Returns `True` if noise is active |
| `set_coherent_error(gate_type, epsilon)` | `gate_type: int, epsilon: float` | Add systematic rotation bias for a gate type |
| `get_coherent_error(gate_type)` | `gate_type: int` | Get the rotation bias for a gate type (0.0 if not set) |

### `ReadoutError`

Per-qubit measurement confusion matrix.

```python
readout = qubit_engine.ReadoutError(p0_given_1=0.02, p1_given_0=0.01)
```

| Field | Type | Description |
|---|---|---|
| `p0_given_1` | `float` | Probability of reading 0 when the true state is \|1⟩ |
| `p1_given_0` | `float` | Probability of reading 1 when the true state is \|0⟩ |

### Channel Factory Functions

Create individual noise channels for fine-grained noise model composition.

| Function | Parameters | Returns | Description |
|---|---|---|---|
| `make_depolarizing_channel_1q(p)` | `p: float` | `NoiseChannel1Q` | Single-qubit depolarizing channel with error probability p |
| `make_depolarizing_channel_2q(p)` | `p: float` | `NoiseChannel2Q` | Two-qubit depolarizing channel (full 16-operator Pauli tensor product) |
| `make_amplitude_damping_channel(gamma)` | `gamma: float` | `NoiseChannel1Q` | T1 amplitude damping (energy relaxation) |
| `make_phase_damping_channel(gamma)` | `gamma: float` | `NoiseChannel1Q` | T2 phase damping (dephasing / coherence loss) |
| `make_thermal_relaxation_channel(t1, t2, gate_time)` | `t1, t2, gate_time: float` | `NoiseChannel1Q` | Combined T1/T2 thermal relaxation. Requires T2 ≤ 2·T1. |

#### Example: Noisy Simulation

```python
import qubit_engine

model = qubit_engine.NoiseModel.Realistic(
    p1q=0.001, p2q=0.01,
    t1_gamma=0.005, t2_gamma=0.01,
    readout=qubit_engine.ReadoutError(0.02, 0.01)
)

qreg = qubit_engine.QuantumRegister(2)
qreg.setNoiseModel(model)

# All subsequent gates now include automatic noise injection
qreg.applyHadamard(0)
qreg.applyCNOT(0, 1)

result = qreg.measure(0)  # Subject to readout error
```

---

## Gradient Computation

QubitEngine provides two gradient methods, both executing entirely in C++ for maximum performance. Gradients are used for variational quantum algorithm (VQA) optimization.

### Ansatz Functions

All gradient and optimization functions accept an **ansatz function** — a Python callable that builds a parameterized quantum circuit:

```python
def my_ansatz(params: list[float], qreg: qubit_engine.QuantumRegister) -> None:
    qreg.applyRotationY(0, params[0])
    qreg.applyRotationY(1, params[1])
    qreg.applyCNOT(0, 1)
    qreg.applyRotationY(0, params[2])
    qreg.applyRotationY(1, params[3])
```

### Hamiltonian Format

Hamiltonians are specified as a list of `(coefficient, pauli_string)` tuples:

```python
# H2 molecule Hamiltonian
hamiltonian = [
    (-1.0524, "II"),
    ( 0.3979, "IZ"),
    (-0.3979, "ZI"),
    (-0.0113, "ZZ"),
    ( 0.1809, "XX"),
    ( 0.1809, "YY"),
]
```

### `calculate_gradients`

Compute analytical gradients via the **Parameter Shift Rule**: `∂E/∂θ = [E(θ+π/2) − E(θ−π/2)] / 2`. Requires 2P circuit evaluations for P parameters. Supports MPI distribution.

```python
grads = qubit_engine.calculate_gradients(
    num_qubits,    # int: number of qubits
    params,        # list[float]: current parameter values
    ansatz_func,   # callable(params, qreg)
    hamiltonian    # list[tuple[float, str]]
)
# Returns: list[float] of length len(params)
```

### `calculate_gradients_adjoint`

Compute gradients via the **Adjoint Differentiation** method. Uses a single forward pass + reverse tape walk. More efficient than parameter shift for large circuits (O(1) forward + O(L) backward vs. O(2P) evaluations).

```python
grads = qubit_engine.calculate_gradients_adjoint(
    num_qubits, params, ansatz_func, hamiltonian
)
```

### `calculate_gradients_adjoint_gpu`

Same as `calculate_gradients_adjoint` but executes on GPU (CUDA). Falls back to CPU automatically if CUDA is not available.

```python
grads = qubit_engine.calculate_gradients_adjoint_gpu(
    num_qubits, params, ansatz_func, hamiltonian
)
```

### `get_expectation_value`

Compute the expectation value ⟨H⟩ for a single forward pass. Useful for energy evaluation during optimization or as a PyTorch forward pass.

```python
energy = qubit_engine.get_expectation_value(
    num_qubits, params, ansatz_func, hamiltonian
)
# Returns: float
```

### `get_gradients`

Alias for `calculate_gradients`, designed for use in PyTorch backward passes.

```python
grads = qubit_engine.get_gradients(
    num_qubits, params, ansatz_func, hamiltonian
)
```

---

## Optimizers

Native C++ optimizers that run the full optimization loop without Python overhead. Each iteration evaluates gradients, updates parameters, and checks convergence — all in compiled code.

### `AdamOptimizer`

Adaptive Moment Estimation optimizer. Default configuration: `lr=0.1, β₁=0.9, β₂=0.999, ε=1e-8, max_iter=100, tol=1e-6`.

```python
optimizer = qubit_engine.AdamOptimizer()
optimal_params = optimizer.minimize(
    num_qubits,      # int
    ansatz_func,     # callable(params, qreg)
    hamiltonian,     # list[tuple[float, str]]
    initial_params   # list[float]
)
# Returns: list[float] — optimized parameters
```

### `SPSAOptimizer`

Simultaneous Perturbation Stochastic Approximation. Requires only 2 circuit evaluations per iteration regardless of parameter count — useful for high-dimensional circuits.

```python
optimizer = qubit_engine.SPSAOptimizer()
optimal_params = optimizer.minimize(
    num_qubits,
    ansatz_func,
    hamiltonian,
    initial_params
)
```

---

## Framework Integrations

### PyTorch (`torch_quantum.py`)

QubitEngine provides a `torch.autograd.Function` for seamless integration with PyTorch's automatic differentiation.

```python
from qubit_engine.torch_quantum import QuantumLayer

# Define ansatz
def ansatz(params, qreg):
    qreg.applyRotationY(0, params[0])
    qreg.applyRotationY(1, params[1])
    qreg.applyCNOT(0, 1)

# Hamiltonian as list of (coeff, pauli_string)
hamiltonian = [(-1.05, "II"), (0.40, "IZ"), (-0.40, "ZI"), (0.18, "XX")]

# Create a differentiable quantum layer
layer = QuantumLayer(num_qubits=2, num_params=2, hamiltonian=hamiltonian, ansatz_func=ansatz)

# Use in a standard PyTorch training loop
optimizer = torch.optim.Adam(layer.parameters(), lr=0.1)
for epoch in range(100):
    energy = layer(None)   # Forward pass → C++ expectation value
    energy.backward()      # Backward pass → C++ parameter shift gradients
    optimizer.step()
    optimizer.zero_grad()
```

The `QuantumFunction` autograd function handles:
- **Forward**: Calls `get_expectation_value()` in C++
- **Backward**: Calls `get_gradients()` in C++ (parameter shift rule)
- **Chain rule**: Properly propagates `grad_output` through the quantum layer

### PennyLane (`qubit_engine.pennylane`)

QubitEngine registers as a PennyLane device for use with PennyLane's circuit API and optimizers.

```python
import pennylane as qml

dev = qml.device("qubit_engine.simulator", wires=2)

@qml.qnode(dev)
def circuit(params):
    qml.RY(params[0], wires=0)
    qml.RY(params[1], wires=1)
    qml.CNOT(wires=[0, 1])
    return qml.expval(qml.PauliZ(0) @ qml.PauliZ(1))

result = circuit([0.5, 1.2])
```

**Supported PennyLane Gates**: `Hadamard`, `PauliX`, `PauliY`, `PauliZ`, `CNOT`, `RX`, `RY`, `RZ`, `SWAP`, `CZ`, `Toffoli`, `S`, `T`

**Supported Observables**: `PauliX`, `PauliY`, `PauliZ`, `Identity`, `Hamiltonian`

---

## Complete VQE Example

Find the ground state energy of the H₂ molecule:

```python
import qubit_engine
import math
import random

# 1. Define Hamiltonian (H2 at 0.7414 Å bond distance)
hamiltonian = [
    (-1.052373, "II"),
    ( 0.397937, "IZ"),
    (-0.397937, "ZI"),
    (-0.011280, "ZZ"),
    ( 0.180931, "XX"),
    ( 0.180931, "YY"),
]
num_qubits = 2

# 2. Define hardware-efficient ansatz
def ansatz(params, qreg):
    qreg.applyRotationY(0, params[0])
    qreg.applyRotationY(1, params[1])
    qreg.applyCNOT(0, 1)
    qreg.applyRotationY(0, params[2])
    qreg.applyRotationY(1, params[3])

# 3. Optimize with Adam (entirely in C++)
params = [random.uniform(0, 2 * math.pi) for _ in range(4)]
optimizer = qubit_engine.AdamOptimizer()
optimal_params = optimizer.minimize(num_qubits, ansatz, hamiltonian, params)

# 4. Evaluate final energy
energy = qubit_engine.get_expectation_value(num_qubits, optimal_params, ansatz, hamiltonian)
print(f"Ground State Energy: {energy:.6f} Ha")  # Expected: ~-1.137 Ha
```

---

## MPI Distributed Execution

When built with `-DMPI_ENABLED=ON`, state vectors are automatically distributed across MPI ranks. The Python bindings handle MPI lifecycle automatically (init on import, finalize on exit via `atexit`).

```bash
mpirun -np 4 python my_simulation.py
```

```python
qreg = qubit_engine.QuantumRegister(30)  # 2^30 amplitudes split across 4 ranks
print(f"Rank {qreg.getRank()} of {qreg.getSize()}")

# All gate operations are MPI-aware — no code changes needed
qreg.applyHadamard(0)
qreg.applyCNOT(0, 29)  # Global qubit — automatically uses MPI_Sendrecv
```
