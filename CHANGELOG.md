# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- **Noise Model**: Comprehensive Kraus-operator noise simulation system (`NoiseModel.hpp/cpp`).
  - Depolarizing noise channel — single-qubit (4 Kraus operators: I, X, Y, Z) and two-qubit (full 16-operator Pauli tensor product {I,X,Y,Z}⊗{I,X,Y,Z}).
  - Amplitude damping channel (T1) — models energy relaxation of |1⟩ → |0⟩.
  - Phase damping channel (T2) — models dephasing / loss of coherence.
  - Readout error — per-qubit confusion matrix with configurable P(0|1) and P(1|0).
- **Automatic noise injection**: Noise channels are applied automatically after every gate operation when a `NoiseModel` is attached to a `QuantumRegister` via `setNoiseModel()`.
- **Convenience builders**: `NoiseModel::Depolarizing(p1q, p2q)` and `NoiseModel::Realistic(p1q, p2q, t1, t2, readout)` for one-line noise model creation.
- **Protobuf**: Added `AMPLITUDE_DAMPING` and `PHASE_DAMPING` gate types, `noise_gamma` field, and `NoiseConfig` message to `quantum.proto`.
- **Python bindings**: Full pybind11 exposure of `NoiseModel`, `ReadoutError`, `NoiseChannel1Q/2Q`, and channel factory functions.
- **Tests**: 17 new GTest cases covering Kraus completeness (Σ K†K = I), channel configuration, invalid input rejection, CpuBackend functional tests, and QuantumRegister integration.

### Changed
- **IQuantumBackend**: Extended interface with `applyNoiseChannel1Q()`, `applyNoiseChannel2Q()` (pure virtual), and `measureWithReadoutError()` (default implementation).
- **CpuBackend**: Implemented stochastic Kraus operator selection and application with post-application renormalization for sub-unitary operators.
- **QuantumRegister**: All gate methods now call `applyPostGateNoise1Q/2Q()` when a noise model is active. `measure()` routes through `measureWithReadoutError()` when readout error is configured.
- **Backend stubs**: MPS, CUDA, Metal, Cloud, and Stabilizer backends received stub implementations for the new virtual noise methods.
### Fixed

## [0.2.5] - 2026-04-12

### Fixed
- **Dashboard UI**: Resolved bug where the "Execute" button was invisible until user interaction by making entrance animations more robust.
- **macOS Build**: Fixed `libomp` dependency linkage and dynamic Homebrew path discovery on macOS runners.
- **CI/CD**: Corrected Python package distribution by relocating build configuration to the repository root.
- **CI/CD**: Synchronized project-wide versioning to ensure clean PyPI publication.

## [0.2.0] - 2026-04-11

### Added
- **Test Coverage**: Added `MetalBackendTests.cpp` guarded by macOS directives to ensure GPU kernel correctness.
- **Test Coverage**: Vitest unit tests added for Next.js web components (`CircuitDiagram.test.tsx`, `TopologyGraph.test.tsx`, `WavefunctionChart.test.tsx`).
- **Observability**: Added `prometheus` and `grafana` directly into `docker-compose.yaml` with a default `prometheus.yml`.
- **Doxygen Docs**: Created Doxygen API comments for `IQuantumBackend.hpp`, `QuantumJIT.hpp`, and `QuantumDifferentiator.hpp`.

### Changed
- **CUDA Support**: Expanded `CMAKE_CUDA_ARCHITECTURES` from just Volta (`75`) to support `70;75;80;86;89;90`, enabling use on modern GPUs like Hopper.
- **Service Discovery**: Updated Go Scheduler `grpc.Dial` dialer to use the `dns:///` scheme and `round_robin` load balancing policy for internal service mesh.
- **Security**: Hardened JWT handling to `panic()` if `QUBIT_ENGINE_JWT_SECRET` is unset in production environments.
- **Security**: Restricted gRPC-Web CORS in the scheduler to be configurable via the `ALLOWED_ORIGINS` environment variable.
- **Security**: The Scheduler redis client now properly reads `REDIS_PASSWORD` from the environment instead of hardcoding empty credentials.

### Fixed
- **JIT Compiler Hash Collision Risk**: Added explicit parameter and qubit delimiters (`_`, `,`, `;`) to `QuantumJIT::compute_hash` stringification to prevent collisions between ambiguous gate instructions.
- **Metal Backend Consistency**: Deleted the unfinished stochastic depolarizing noise shader; the engine correctly relies on the CPU-driven dispatch trajectory model implemented in `MetalBackend.mm`.
