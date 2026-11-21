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
│  │  ├─ CpuBackend   (AVX2/NEON + OpenMP, single-node) │ │
│  │  ├─ CudaBackend  (NVIDIA GPU, single-device)       │ │
│  │  ├─ MetalBackend (Apple GPU)                       │ │
│  │  ├─ MPSBackend   (1D Tensor Networks, ≤ 30Q)       │ │
│  │  ├─ StabilizerBackend (Clifford QEC)               │ │
│  │  └─ CloudBackend (Mock remote execution stubs)     │ │
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

1. **Cloud** — Mock remote execution offloading stub (`CloudBackend`). Returns simulated job IDs; no live QPU hardware connectivity.
2. **MPS (Tensor Network)** — Simulates 1D circuits with SVD truncation up to configurable bond dimension ($\chi$, default 64 per `ConfigManager.cpp:49` and `MPSBackend.hpp:40`). Observable extraction is limited to single-node contraction: full state vector extraction supports up to $N \le 30$ and probability extraction supports up to $N \le 25$. Expectation value calculation throws `FeatureNotSupportedException` (general tensor-network contraction is designed, not integrated).
3. **CUDA / Metal** — Hardware GPU acceleration (single-device). Automatically selects `CudaBackend` on NVIDIA platforms (`#ifdef ENABLE_CUDA`) or `MetalBackend` on macOS Apple Silicon (`#ifdef ENABLE_METAL`). Multi-GPU CUDA execution via NCCL is designed, not integrated: NCCL communicator initialization is uncoordinated across independent server processes.
4. **CPU** — Default fallback with explicit AVX2 (`__m256d`) and ARM NEON (`float64x2_t`) vector intrinsics for `applyHadamard`, combined with OpenMP thread parallelization and scalar loops for remaining gates. Supports both single-node and multi-node MPI distributed state-vector execution (SPMD via `MPI_COMM_WORLD`). When running under MPI with `world_size > 1`, `BackendFactory` automatically selects `CpuBackend` as the sole collective-aware backend.

The `force_local` constructor parameter explicitly forces single-node local execution (e.g. for standalone sub-routines or tests), while omitting it or setting `force_local=false` allows `CpuBackend` to distribute the state vector across all available MPI ranks.

## JIT Compiler Optimization Tiers

The `QuantumJIT` compiler pass (`QuantumJIT::compile`) operates on `CircuitIR` (intermediate representation) and utilizes an in-memory LRU hashing layer (`ir_cache_map_` protected by `cache_mutex_`) to cache topologically identical circuit optimization passes.

In the gRPC service layer (`CircuitService::RunCircuit`), gate operations are processed in 1,000-gate chunks. Compilation of upcoming chunks is dispatched asynchronously to background threads via `std::async(std::launch::async)`, pipelining JIT compilation concurrently ahead of active gate execution.

It features five optimization levels:

| Level | Name | Strategy |
|-------|------|----------|
| **O0** | None | Pass-through; builds `CompiledGate` structs only |
| **O1** | Cancel | Adjacent inverse gate cancellation (X·X = I, H·H = I) |
| **O2** | Fuse | Consecutive single-qubit gates on the same qubit are fused via 2×2 matrix multiplication |
| **O3** | Aggressive | Groups and reorders independent single-qubit gates per qubit, then fuses them via 2×2 matrix multiplication |
| **O4** | KAK | Two-qubit adjacent gate fusion via 4×4 matrix multiplication and KAK decomposition into canonical single-qubit rotations and CNOT gates |

Gate matrices are stored as `std::array<Complex, 4>` (2×2) or `std::array<Complex, 16>` (4×4).

## Noise Simulation

`NoiseModel` provides a composable noise configuration system based on Kraus operators. Rather than simulating density matrices (which would require O(4^n) memory), noise is applied **stochastically** — for each channel, one Kraus operator is randomly selected based on probabilities and applied as a matrix transformation, followed by state renormalization. This gives statistically correct results when averaged over many shots, which VQE already does.

### Noise Application Pipeline

When a `NoiseModel` is attached to a `QuantumRegister` via `setNoiseModel()`, noise is injected automatically:

1. **After every 1Q gate**: All configured single-qubit channels are applied to the target qubit
2. **After every 2Q gate**: Single-qubit channels are applied to both qubits involved, then all two-qubit channels are applied to the pair
3. **During measurement**: Readout error (confusion matrix) is applied to flip the classical result with configured probabilities

> **Stabilizer Backend Noise**: On `StabilizerBackend`, general Kraus non-unitary operators are restricted to Clifford/Pauli channels. Single-qubit and two-qubit depolarizing noise channels are stochastically sampled and applied as random Pauli $X, Y, Z$ and $\sigma_a \otimes \sigma_b$ gate insertions directly into the stabilizer tableau, preserving Gottesman-Knill simulation efficiency while accurately modeling physical error rates.

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
| `GetHardwareTopology` | Unary | Fetch hardware topology (nodes & coupler edges) for visualization |
| `AcknowledgeShmRead` | Unary | Acknowledge completion of reading a Shared Memory segment |

> **Note on Shared-Memory IPC**:
> `ipc::SharedMemory` in C++, `pkg/ipc` in Go, and `cli-rs` in Rust provide high-performance, zero-copy state-vector transport for co-located clients on the same host. The architecture features authenticated JWT token lifecycle management, client read-acknowledgment RPCs (`AcknowledgeShmRead`), backpressure limiting (up to 3 concurrent active segments), automatic protobuf fallback under load or unauthorized requests, and a 5-second asynchronous safety reaper to prevent kernel segment leaks on reader crashes.

Defined in `api/proto/scheduler.proto`:

| RPC | Type | Description |
|-----|------|-------------|
| `SubmitJob` | Unary | Submit circuit to Redis priority queue |
| `GetJobStatus` | Unary | Poll job state |
| `CancelJob` | Unary | Cancel queued or running job |
| `StreamJobResults` | Server Stream | Stream results as engine produces them |
| `ListJobs` | Unary | List jobs by user with pagination |
| `GetClusterMetrics` | Unary | Get real-time cluster metrics for visualization |
| `StreamClusterMetrics` | Server Stream | Stream real-time cluster metrics for visualization |

## Hardware & Distributed Scaling

- **Autoscaling & Metrics Integration**: The Go `Scheduler` service mounts a `:2112/metrics` endpoint exposing `quantum_job_queue_depth` metrics directly into Prometheus, allowing standard Kubernetes HorizontalPodAutoscalers (HPA) to scale backend deployments based on active queue workload.
- **Python Capsule Buffer Handoff**: The `core.get_state_vector()` and `get_probabilities()` C++ endpoints bind using `pybind11::capsule` memory management. While `QuantumRegister::getStateVector()` returns by value from the backend, moving the vector onto the heap and wrapping it in a `py::capsule` avoids duplicate copies during the Python C++ boundary handoff to NumPy.
- **MPI Distributed State Simulation**: Distributed SPMD circuit execution coordinates Rank 0 (gRPC Ingress and coordinator) with worker ranks via `MPI_Bcast` command dispatch, lockstep gate execution, `MPI_Sendrecv` for global-stride partner exchanges, and `MPI_Gather` for state retrieval. The cluster execution lock serializes runs with configurable timeouts (returning `RESOURCE_EXHAUSTED` under contention), and dynamic watchdog timeouts ($T = 15\text{s} + \lceil \text{bytes} / 100\text{ MB/s} \rceil$) trigger `MPI_Abort` on worker hangs.

## Differentiator Methods

`QuantumDifferentiator` provides two gradient computation strategies:

- **Parameter Shift Rule** (`calculateGradients`): O(2P) circuit evaluations for P parameters. Exact for gates of the form exp(-iθP/2). Single-node execution.
- **Adjoint Differentiation** (`calculateGradientsAdjoint`): O(1) forward pass + O(L) backward pass over L gates. Analytically computes gradients for single-qubit parameterized rotations ($R_X, R_Y, R_Z$) interleaved with Clifford and phase gates ($H, X, Y, Z, S, T, \text{CNOT}$) across multi-qubit circuits on single-node execution.

## Deployment

- **Docker Compose** (`deploy/docker/docker-compose.yaml`): Full local stack (engine, scheduler, registry, cache, Redis, PostgreSQL, Envoy proxy)
- **Helm Chart** (`deploy/helm/`): Kubernetes deployment running standalone single-node engine replicas with configurable replica count, resource limits, and service discovery, or distributed multi-node simulation via Kubeflow `MPIJob` (`templates/mpijob.yaml`)
- **Kubernetes Manifests** (`deploy/k8s/`): Raw manifests deploying independent single-node engine pods behind a Service, with headless service and MPI cluster definitions
- **Envoy** (`deploy/docker/envoy.yaml`): gRPC-Web transcoding for browser clients

## 🔮 Planned / Roadmap Items

The following features represent architectural roadmap targets for future releases:

1. **Distributed Multi-Node VQE Optimization**: Extending gradient evaluation and VQE optimization loops to participate in MPI collective broadcasts across worker ranks (currently scoped to single-node per Finding NEW-06).
2. **Linux/POSIX Multi-Process SHM CI Test Matrix**: Expanding automated multi-process SHM integration test coverage into Linux container CI environments (Finding NEW-07).
3. **MPS Tensor Network Observable Contraction**: Implementing general tensor-network contraction for expectation values and probabilities beyond 25–30 qubits without full state vector reconstruction.
4. **Coordinated Multi-GPU Scaling**: Coordinating NCCL communicator initialization across processes and replacing full-state replication with a point-to-point (P2P) boundary exchange model.
5. **Live QPU Cloud Backend Submissions**: Translating circuit IR into native Quil (Rigetti) and IonQ JSON payloads for active hardware execution.
