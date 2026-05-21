# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- **Pythonic Circuit-Builder API**: Introduced fluent `Circuit` builder (`circuit.py`) with support for basic quantum gates, `Result` metadata wrapping, high-level noise wrapper, and VQE execution.
- **Circuit Presets (Web)**: Integrated standard quantum algorithm presets (Bell, GHZ, QFT, Teleportation) for quick loading in the Circuit Lab.
- **Hardware-Specific Noise Presets**: Added calibration configurations for `IBM Brisbane` and `Google Sycamore` with customized per-qubit and per-edge noise channel application.
- **MPS Backend Noise & Scaling**: Implemented top-k sampling for `getProbabilities()` and full stochastic Kraus operator support for applying noise channels (`applyNoiseChannel1Q`, `applyNoiseChannel2Q`, `applyDepolarizingNoise`).
- **OpenQASM 3.0 Tier 1**: Expanded `QASMParser` to support control flow (`if`/`else`), custom `gate` declarations, `barrier`, and `reset` commands. Updated exporter to round-trip these features.
- **Validation Suite**: Added `AlgorithmValidation.cpp` and `MirrorCircuitTests.cpp` for large-scale cross-simulator correctness checking.
- **CI Golden Generation**: Integrated Qiskit into CI to automatically generate and compare golden reference vectors on-the-fly (`generate_golden_vectors.py`).
- **O4 Fusion Optimization**: Enhanced `QuantumJIT` to support single-to-two qubit gate fusion and identity pruning, significantly reducing gate count and bandwidth for large circuits.
- **OpenMP Parallelization**: Implemented multi-threading for `CpuBackend::expectationValue`, `QuantumDifferentiator::evaluateEnergy`, and adjoint state update loops.
- **Surface Code Finalization**: Implemented logical measurement ($Z_0 Z_3 Z_6$) and MWPM-matched Pauli correction logic for distance-3 rotated surface codes.
- **OpenQASM 3.0 Parser**: New robust AST-based implementation with full statement lookahead and support for complex assignments.
- **Security Hardening**: Added strict JWT audience validation (`qubit-engine-api`) in `AuthInterceptor`.

### Changed
- **Circuit Lab Visualizer Polish**: Refactored `CircuitDiagram.tsx` to support native SVG `fill-` classes, rendering correct vibrant gate colors. Added standard circle-with-plus target representation for CNOT (`CX`) and Toffoli (`CCX`) gates.
- **Go Toolchain Security Hardening**: Upgraded Go compiler version to `1.25.10` in all Go services and the root workspace to fix high-severity vulnerabilities (`CVE-2026-33811`, `CVE-2026-33814`, `CVE-2026-39820`, `CVE-2026-39836`, `CVE-2026-42499`).
- **Debian Engine Image Security Hardening**: Added `apt-get upgrade -y` to the engine container runtime stage, resolving critical vulnerabilities in `libgnutls30` (`CVE-2026-33845`, `CVE-2026-42010`, `CVE-2026-33846`, `CVE-2026-3833`, `CVE-2026-42009`).
- **Surface Code Layout**: Switched to a standard distance-3 rotated layout to ensure stabilizer commutativity.
- **Simulation Flow**: Updated the QEC simulation loop to treat the first cycle as a projection-only round, preventing false corrections.
- **Tokenizer Logic**: Refactored tokenization to prioritize punctuation over numeric literals (fixing `->` parsing).

### Fixed
- **QASM Roundtrip**: Resolved failures in measurement assignment parsing and roundtrip fidelity.
- **JIT Property Flakiness**: Fixed race conditions and stability issues in JIT property-based tests.
- **Stabilizer Noise Consistency**: Corrected noise threshold assertions in `StabilizerBackend` tests.

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
