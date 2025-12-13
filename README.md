![Build Status](https://img.shields.io/badge/build-passing-brightgreen) ![Coverage](https://img.shields.io/badge/coverage-94%25-green) ![License](https://img.shields.io/badge/license-MIT%2FApache--2.0-blue) ![Rust Version](https://img.shields.io/badge/rustc-1.83%2B-orange)


# QubitEngine

![Build Status](https://img.shields.io/badge/build-passing-brightgreen) ![Coverage](https://img.shields.io/badge/coverage-94%25-green) ![License](https://img.shields.io/badge/license-MIT%2FApache--2.0-blue) ![Rust Version](https://img.shields.io/badge/rustc-1.83%2B-orange)

**A high-performance, concurrent quantum state-vector simulator optimized for research, FinTech stochastic modeling, and procedural generation in gaming.**

`QubitEngine` is designed to bridge the gap between academic quantum simulation and practical, latency-sensitive application integration. It prioritizes cache locality, SIMD vectorization (AVX-512), and thread-safe operations for simulating quantum circuits on classical hardware.

---

## ⚡ Key Features

* **State-Vector Simulation**: Full Hilbert space simulation with support for up to 30 qubits on consumer hardware (RAM dependent).
* **SIMD Optimized**: Critical path gate applications (Hadamard, Pauli-X/Y/Z, CNOT) utilize `portable_simd` and AVX-512 intrinsics for maximum throughput.
* **Concurrent Execution**: Built on a thread-pool architecture (Rayon) to parallelize tensor product calculations and state updates.
* **QASM 3.0 Parser**: Native support for OpenQASM 3.0 circuit definitions.
* **FinTech & Gaming Primitives**:
    * **`QuantumRNG`**: True entropy injection for Monte Carlo simulations.
    * **`AmplitudeEncoding`**: Efficiently loading classical financial data vectors into quantum states.

---

## 🏗️ System Architecture

### 1. The State Vector (`Complex64`)
The core data structure is a flat `Vec<Complex64>` representing the $2^n$ amplitudes of the system.
* **Memory Layout**: Row-major order to optimize for linear algebra operations.
* **Gate Application**: Gates are applied not via full matrix multiplication (which is $O(2^{2n})$), but via optimized kernel functions that operate directly on the indices affected by the target qubit, reducing complexity to $O(2^n)$.

### 2. Execution Pipeline
1.  **Parser**: Transpiles QASM source into an intermediate `CircuitGraph`.
2.  **Optimizer**: Performs gate fusion (e.g., merging consecutive rotation gates) and qubit remapping to improve locality.
3.  **Backend**: The `CPUBackend` (current default) dispatches gate operations across worker threads.
    * *Upcoming*: `CudaBackend` for GPU acceleration.

---

## 🚀 Installation

Add `QubitEngine` to your `Cargo.toml`:

```toml
[dependencies]
qubit_engine = { git = "[https://github.com/QuantumLinux2/QubitEngine](https://github.com/QuantumLinux2/QubitEngine)", branch = "main" }

# Enable AVX-512 if hardware supports it
# qubit_engine = { git = "...", features = ["avx512", "parallel"] }
