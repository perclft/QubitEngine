# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
### Changed
### Fixed

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
