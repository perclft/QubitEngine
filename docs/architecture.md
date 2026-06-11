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

`QuantumRegister` acts as a proxy that creates the appropriate backend at construction time. The selection logic in `QuantumRegister.cpp` follows this priority chain:

1. **Cloud** — Remote execution offloading
2. **MPS (Tensor Network)** — Simulates > 50 qubits for weakly entangled logic using SVD Truncation
3. **CUDA** — Multi-GPU Tensor Sharding & Async Streams via NCCL
4. **CPU** — Default fallback with AVX2/NEON + OpenMP

The `force_local` constructor parameter bypasses distributed (MPI) execution, useful for gradient calculations where each parameter evaluation needs an independent register.

## JIT Compiler Optimization Tiers

The `QuantumJIT` compiler operates on `CircuitIR` (intermediate representation) and implements an LRU hashing layer to cache topological identical circuit passes entirely. It also operates on an independent thread, fusing arrays concurrently alongside active GPU hardware executions.

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

> **Note**: `StateResponse` objects support `shm_descriptor` string mappings, allowing Go sidecars and Python processes to map the raw zero-copy `2^N` Floats natively from OS paging memory rather than serializing arrays over protobuf sockets.

Defined in `api/proto/scheduler.proto`:

| RPC | Type | Description |
|-----|------|-------------|
| `SubmitJob` | Unary | Submit circuit to Redis priority queue |
| `GetJobStatus` | Unary | Poll job state |
| `CancelJob` | Unary | Cancel queued or running job |
| `StreamJobResults` | Server Stream | Stream results as engine produces them |
| `ListJobs` | Unary | List jobs by user with pagination |

## Hardware & Distributed Scaling

- **Predictive Autoscaling Mesh**: The Go `Scheduler` mounts a `:2112/metrics` endpoint exposing `queue:jobs` depth mappings directly into Prometheus. The K8s auto-scaler uses this to dynamically scale backend deployments globally ahead of congestion.
- **Python Zero-Copy Buffers**: The `core.get_state_vector()` and `get_probabilities()` C++ endpoints bind utilizing `pybind11::buffer_info`, anchoring C++ RAM allocation lifecycles actively inside NumPy preventing memory duplications.

## Differentiator Methods

`QuantumDifferentiator` provides two gradient computation strategies:

- **Parameter Shift Rule** (`calculateGradients`): O(2P) circuit evaluations for P parameters. Supports MPI distribution across ranks. Exact for gates of the form exp(-iθP/2).
- **Adjoint Differentiation** (`calculateGradientsAdjoint`): O(1) forward pass + O(L) backward pass over L gates. Records a circuit tape, then walks it in reverse computing ⟨λ|dU/dθ|ψ⟩ contributions.

## Deployment

- **Docker Compose** (`deploy/docker/docker-compose.yaml`): Full local stack (engine, scheduler, registry, cache, Redis, PostgreSQL, Envoy proxy)
- **Helm Chart** (`deploy/helm/`): Kubernetes deployment with configurable replicas, resource limits, and service discovery
- **Kubernetes Manifests** (`deploy/k8s/`): Raw manifests for namespace, services, deployments, and PVCs
- **Envoy** (`deploy/docker/envoy.yaml`): gRPC-Web transcoding for browser clients
