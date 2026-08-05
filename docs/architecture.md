# QubitEngine Architecture Guide

## System Overview

QubitEngine is a multi-language quantum simulation platform with five major layers:

```
┌───────────────────────────┬─────────────────────────────┐
│    Next.js Web Client     │      Rust TUI (Ratatui)     │
│  (React/TypeScript UI)    │   (Terminal dashboard)      │
└─────────────┬─────────────┴──────────────┬──────────────┘
              │ gRPC-Web over Envoy        │ gRPC Streaming
┌────────────────────────▼────────────────────────────────┐
│             Go Application Mesh (gRPC)                  │
│  ┌──────────┐  ┌──────────┐  ┌───────┐                 │
│  │Scheduler │  │ Registry │  │ Cache │                  │
│  │  (Redis) │  │(Postgres)│  │(Redis)│                  │
│  └──────────┘  └──────────┘  └───────┘                  │
└────────────────────────┬────────────────────────────────┘
                         │ gRPC
┌────────────────────────▼────────────────────────────────┐
│              C++ Physics Kernel (C++20)                  │
│  ┌────────────────────────────────────────────────────┐ │
│  │ QuantumRegister → IQuantumBackend (polymorphic)    │ │
│  │  ├─ CpuBackend   (AVX2/NEON + OpenMP)              │ │
│  │  ├─ CudaBackend  (NVIDIA GPU)                      │ │
│  │  ├─ MetalBackend (Apple GPU)                       │ │
│  │  ├─ MPSBackend   (Tensor Networks)                 │ │
│  │  ├─ StabilizerBackend (Clifford QEC)               │ │
│  │  └─ CloudBackend (remote execution)                │ │
│  ├─ NoiseModel      (Kraus-operator noise channels)   │ │
│  ├─ QuantumJIT     (gate fusion compiler)             │ │
│  ├─ QuantumDifferentiator (parameter-shift + adjoint) │ │
│  ├─ OpenQASM       (parser/exporter 2.0 & 3.0)       │ │
│  └─ CircuitOptimizer (gate peephole optimization)     │ │
│  ┌────────────────────────────────────────────────────┐ │
│  │ Python Bindings (pybind11) → qubit_engine module   │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

## Backend Selection

`QuantumRegister` acts as a proxy that creates the appropriate backend at construction time. The selection logic in `BackendFactory.cpp` follows this priority chain:

1. **Cloud** — Remote execution offloading stub (`CloudBackend`).
2. **MPS (Tensor Network)** — Simulates 30+ qubits (and up to 50+ qubits for weakly entangled 1D circuits) using SVD truncation with configurable bond dimension (default $\chi=64$).
3. **CUDA** — GPU acceleration. Multi-GPU execution via `CudaBackend::applyGateDistributed` replicates the full $2^N$ state vector across GPUs via `ncclAllGather` (each GPU must possess sufficient VRAM for the full state vector).
4. **CPU** — Default fallback with explicit AVX2 (`__m256d`) and ARM NEON (`float64x2_t`) vector intrinsics for `applyHadamard`, combined with OpenMP thread parallelization and scalar loops for remaining gates.

> **Note on Metal (Apple Silicon)**: `MetalBackend` is not part of the automatic `BackendFactory::create` selection chain. It requires explicit instantiation / dependency injection via `QuantumRegister(n, std::move(metal_backend))`.

The `force_local` constructor parameter bypasses distributed (MPI) execution, useful for gradient calculations where each parameter evaluation needs an independent register.

## JIT Compiler Optimization Tiers

The `QuantumJIT` compiler pass (`QuantumJIT::compile`) operates on `CircuitIR` (intermediate representation) and utilizes an in-memory LRU hashing layer (`ir_cache_map_` protected by `cache_mutex_`) to cache topologically identical circuit optimization passes.

In the gRPC service layer (`CircuitService::RunCircuit`), gate operations are processed in 1,000-gate chunks. Compilation of upcoming chunks is dispatched asynchronously to background threads via `std::async(std::launch::async)`, pipelining JIT compilation concurrently ahead of active gate execution.

It features five optimization levels:

| Level | Name | Strategy |
|-------|------|----------|
| **O0** | None | Pass-through; builds `CompiledGate` structs only |
| **O1** | Cancel | Adjacent inverse gate cancellation (X·X = I, H·H = I) |
| **O2** | Fuse | Consecutive single-qubit gates on the same qubit are fused via 2×2 matrix multiplication |
| **O3** | Aggressive | Reorders independent gates + applies O2 fusion again + swaps linear mappings for 1D topology (MPS) |
| **O4** | KAK | Two-qubit adjacent fusion via KAK-style decomposition + tensor network contraction |

Gate matrices are stored as `std::array<Complex, 4>` (2×2) or `std::array<Complex, 16>` (4×4).

## Noise Simulation

`NoiseModel` provides a composable noise configuration system based on Kraus operators. Rather than simulating density matrices (which would require O(4^n) memory), noise is applied **stochastically** — for each channel, one Kraus operator is randomly selected based on probabilities and applied as a matrix transformation, followed by state renormalization. This gives statistically correct results when averaged over many shots, which VQE already does.

### Noise Application Pipeline

When a `NoiseModel` is attached to a `QuantumRegister` via `setNoiseModel()`, noise is injected automatically:

1. **After every 1Q gate**: All configured single-qubit channels are applied to the target qubit
2. **After every 2Q gate**: Single-qubit channels are applied to both qubits involved, then all two-qubit channels are applied to the pair
3. **During measurement**: Readout error (confusion matrix) is applied to flip the classical result with configured probabilities

### Supported Noise Channels

| Channel | Parameters | Kraus Operators | Description |
|---------|-----------|-----------------|-------------|
| **Depolarizing (1Q)** | p ∈ [0,1] | K₀ = √(1-p)·I, K₁ = √(p/3)·X, K₂ = √(p/3)·Y, K₃ = √(p/3)·Z | Random Pauli error with probability p |
| **Depolarizing (2Q)** | p ∈ [0,1] | 16 operators: √(1-p)·I⊗I + 15× √(p/15)·σₐ⊗σ_b | Full two-qubit Pauli tensor product |
| **Amplitude Damping** | γ ∈ [0,1] | K₀ = diag(1, √(1-γ)), K₁ = [[0,√γ],[0,0]] | T1 energy relaxation (\|1⟩ → \|0⟩) |
| **Phase Damping** | γ ∈ [0,1] | K₀ = diag(1, √(1-γ)), K₁ = [[0,0],[0,√γ]] | T2 dephasing (coherence loss) |
| **Readout Error** | P(0\|1), P(1\|0) | Confusion matrix | Classical bit-flip during measurement |

### Convenience Constructors

```cpp
// Simple depolarizing (typical NISQ error rates)
auto model = NoiseModel::Depolarizing(0.001, 0.01);  // 1Q: 0.1%, 2Q: 1%

// Full realistic model
auto model = NoiseModel::Realistic(
    0.001,  // 1Q depolarizing
    0.01,   // 2Q depolarizing
    0.005,  // T1 amplitude damping gamma
    0.01,   // T2 phase damping gamma
    {0.02, 0.01}  // readout error: P(0|1)=2%, P(1|0)=1%
);

qreg.setNoiseModel(model);  // All subsequent gates automatically apply noise
```

## gRPC API Surface

Defined in `api/proto/quantum.proto`:

| RPC | Type | Description |
|-----|------|-------------|
| `RunCircuit` | Unary | Synchronous circuit execution |
| `StreamGates` | Bidi Stream | Send gates, receive state vectors |
| `VisualizeCircuit` | Server Stream | Execute circuit, stream state after each step |
| `RunVQE` | Server Stream | Run VQE optimization, stream energy per iteration |

> **Note**: POSIX/Windows shared memory mapping utilities (`ipc::SharedMemory`) exist in C++ with RAII handle tracking, but RPC paths between Go services and the C++ engine use standard gRPC serialization over network interfaces.

Defined in `api/proto/scheduler.proto`:

| RPC | Type | Description |
|-----|------|-------------|
| `SubmitJob` | Unary | Submit circuit to Redis priority queue |
| `GetJobStatus` | Unary | Poll job state |
| `CancelJob` | Unary | Cancel queued or running job |
| `StreamJobResults` | Server Stream | Stream results as engine produces them |
| `ListJobs` | Unary | List jobs by user with pagination |

## Hardware & Distributed Scaling

- **Autoscaling & Metrics Integration**: The Go `Scheduler` service mounts a `:2112/metrics` endpoint exposing `quantum_job_queue_depth` metrics directly into Prometheus, allowing Kubernetes HorizontalPodAutoscalers (HPA) to scale backend deployments based on active queue workload.
- **Python Capsule Buffer Handoff**: The `core.get_state_vector()` and `get_probabilities()` C++ endpoints bind using `pybind11::capsule` memory management. While `QuantumRegister::getStateVector()` returns by value from the backend, moving the vector onto the heap and wrapping it in a `py::capsule` avoids duplicate copies during the Python C++ boundary handoff to NumPy.

## Differentiator Methods

`QuantumDifferentiator` provides two gradient computation strategies:

- **Parameter Shift Rule** (`calculateGradients`): O(2P) circuit evaluations for P parameters. Supports MPI distribution across ranks. Exact for gates of the form exp(-iθP/2).
- **Adjoint Differentiation** (`calculateGradientsAdjoint`): O(1) forward pass + O(L) backward pass over L gates. Records a circuit tape, then walks it in reverse computing ⟨λ|dU/dθ|ψ⟩ contributions.

## Deployment

- **Docker Compose** (`deploy/docker/docker-compose.yaml`): Full local stack (engine, scheduler, registry, cache, Redis, PostgreSQL, Envoy proxy)
- **Helm Chart** (`deploy/helm/`): Kubernetes deployment with configurable replicas, resource limits, and service discovery
- **Kubernetes Manifests** (`deploy/k8s/`): Raw manifests for namespace, services, deployments, and PVCs
- **Envoy** (`deploy/docker/envoy.yaml`): gRPC-Web transcoding for browser clients

## 🔮 Planned / Roadmap Items

The following features represent architectural roadmap targets for future releases:

1. **Memory-Sharded Multi-GPU Execution**: Replacing `ncclAllGather` full-state replication with a point-to-point (P2P) boundary exchange model to shard large state vectors across GPU VRAM.
2. **Zero-Copy gRPC Shared Memory Integration**: Wiring C++ `ipc::SharedMemory` descriptors into active Go gRPC sidecar RPC payloads.
3. **QPU Cloud Backend Payload Serialization**: Translating circuit IR into native Quil (Rigetti) and IonQ JSON payloads for active hardware execution.
