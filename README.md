# QubitEngine

<p align="center">
<img src="docs/images/logo.png" alt="QubitEngine Logo" width="120" />
</p>

<p align="center">
<strong>⚛️ Unified, Distributed, Cross-Platform Quantum Experimentation Platform</strong>
</p>

<p align="center">
<a href="#overview">Overview</a> •
<a href="#architecture">Architecture</a> •
<a href="#prerequisites">Prerequisites</a> •
<a href="#building">Building</a> •
<a href="#running">Running</a> •
<a href="#testing">Testing</a> •
<a href="#deployment">Deployment</a>
</p>

---

## 🌟 Overview

**QubitEngine** is a high-performance quantum simulation platform built as a robust R&D tool for researchers. It features a unified C++20 physics kernel with **automatic hardware detection**, enabling seamless execution across **Linux (CUDA)**, **macOS (Metal)**, and **Windows (AVX2)**.

Key capabilities:

- **Multi-backend simulation** — Hardware-accelerated execution on CUDA (single-GPU; multi-GPU with NCCL is designed, not integrated), Metal (macOS), and AVX2/NEON (CPU) with MPI distributed multi-node state-vector simulation.
- **Noise simulation** — Kraus-operator noise model with depolarizing (1Q/2Q), amplitude damping (T1), phase damping (T2), and readout error channels
- **Tensor Networks (MPS)** — Matrix Product State (MPS) 1D chain simulation with SVD truncation (configurable bond dimension $\chi$). Observable extraction is limited to single-node contraction: state vectors up to $N \le 30$ and probabilities up to $N \le 25$ (expectation value contraction is designed, not integrated).
- **Clifford Stabilizer QEC** — Gottesman-Knill simulation for thousands of qubits in polynomial time
- **Shared-Memory IPC** — Zero-copy shared-memory IPC (`ipc::SharedMemory` in C++, `pkg/ipc` in Go, `cli-rs` in Rust) with authenticated token lifecycle, backpressure management, and automatic protobuf fallback.
- **Native differentiability** — Analytical gradient computation via the Parameter Shift Rule and Adjoint differentiation (supporting parameterized rotation gates $R_X, R_Y, R_Z$ interleaved with Clifford and phase gates across multi-qubit circuits) on single-node execution.
- **JIT circuit optimization** — In-memory LRU caching and multi-tier gate optimization passes (O0 pass-through, O1 cancellation, O2 single-qubit fusion, O3 reordering & fusion, O4 two-qubit adjacent fusion with KAK decomposition).
- **Workload-Based Autoscaling** — Kubernetes HPA manifests utilizing Prometheus custom metrics (`quantum_job_queue_depth`) exported by the Go scheduler.
- **OpenQASM 2.0/3.0** — Import and export circuits for interoperability with Qiskit and other frameworks
- **Interactive TUI** — Rust-based terminal dashboard with live probability streaming via gRPC
- **Python bindings** — pybind11 module equipped with zero-copy NumPy buffers and PyTorch integration

## 🏗️ Architecture

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
│  QuantumRegister → IQuantumBackend (polymorphic)        │
│    ├─ CpuBackend   (AVX2/NEON + OpenMP; MPI SPMD multi) │
│    ├─ CudaBackend  (NVIDIA single-GPU; NCCL not integr.)│
│    ├─ MetalBackend (Apple GPU + Async Cmd Queues)       │
│    ├─ MPSBackend   (1D Tensor Chains; state ext. ≤ 30Q) │
│    ├─ StabilizerBackend (Clifford QEC)                  │
│    └─ CloudBackend (Mock remote execution stubs)        │
│  QuantumJIT (LRU Caching) · Differentiator · Optimizer  │
│  NoiseModel (Depolarizing · T1 · T2 · Readout Error)     │
│  Python Bindings (pybind11 zero-copy NumPy arrays)       │
└─────────────────────────────────────────────────────────┘
```

> See [`docs/architecture.md`](docs/architecture.md) for a detailed breakdown of backend selection, JIT tiers, gRPC API surface, and differentiator methods.

## ⚡ Performance Benchmarks

The following measurements are strictly traceable to runnable benchmarks in the repository:

### Physics Kernel Memory Bandwidth (`applyHadamard`)
| Kernel / SIMD | Platform / Hardware | Qubits | Memory Bandwidth | Benchmark Source & Function |
| --- | --- | --- | --- | --- |
| **ARM NEON (`CpuBackend`)** | macOS (Apple M3 Air) | 25 (512 MB) | **19.03 GB/s** | `benchmarks/AntigravityMemoryBenchmark.cpp` (`BM_Antigravity_ApplyHadamard`) |
| **AVX2 (`CpuBackend`)** | Windows 11 (x86_64) | 25 (512 MB) | **6.64 GB/s** | `benchmarks/MemoryWallBenchmark.cpp` (`BM_ApplyHadamard`) |

*(Notes on Deleted / Corrected Claims:
- The shared-memory speedup benchmark table has been removed per audit: in-process `memcpy` comparisons against protobuf heap allocation do not measure end-to-end multi-process IPC and cannot be presented as IPC benchmarks.
- The distributed MPI bandwidth claim (3.15 GB/s) has been deleted because `benchmarks/run_mpi_benchmark.sh` historically only called `sleep 5` without measuring throughput (see Finding NEW-05 in `FINDINGS_NEW.md`). MPI SPMD distributed execution itself has now been integrated and verified across multiple processes in Phase 4.
- Metal bandwidth was previously misattributed to the Metal GPU backend; the measured 19.03 GB/s run was produced by the CPU NEON SIMD vector kernel on Apple Silicon unified memory.)*

## 📁 Project Structure

```
QubitEngine/
├── api/proto/          # Protobuf service definitions (quantum, scheduler, registry, cache)
├── backend/            # C++20 physics kernel
│   ├── src/            # Core source (QuantumRegister, JIT, Differentiator, OpenQASM, gRPC service)
│   ├── src/backends/   # IQuantumBackend implementations (CPU, CUDA, Metal, Cloud, Mock)
│   └── tests/          # GTest unit tests
├── benchmarks/         # Performance benchmarks
├── cli-rs/             # Rust TUI client (Ratatui + tonic)
├── deploy/             # Deployment configs
│   ├── docker/         # Docker Compose + Dockerfiles
│   ├── helm/           # Helm chart for Kubernetes
│   └── k8s/            # Raw Kubernetes manifests
├── python/             # Python bindings (pybind11) + PyTorch integration
├── services/           # Go microservices
│   ├── scheduler/      # Priority job queue (Redis)
│   ├── registry/       # Circuit storage (PostgreSQL)
│   └── cache/          # Result cache (Redis)
├── web/                # Next.js web frontend (TypeScript, gRPC-Web)
└── docs/               # Architecture documentation
```

## 📋 Prerequisites

### C++ Engine (required)

| Tool | Version | Notes |
|------|---------|-------|
| **CMake** | ≥ 3.15 | Build system |
| **vcpkg** | Latest | C++ package manager |
| **C++ compiler** | C++20 support | MSVC 2022, GCC 11+, or Clang 14+ |

vcpkg will install: `grpc`, `protobuf`, `abseil`, `gtest`, `prometheus-cpp`, `gflags`, `glog`, `pybind11`

### Optional components

| Tool | Version | For |
|------|---------|-----|
| **Node.js** | ≥ 18.x | Web frontend (`web`) |
| **Rust** | ≥ 1.85 | TUI client (`cli-rs`) |
| **Go** | ≥ 1.24 | Microservices (scheduler, registry, cache) |
| **CUDA Toolkit** | ≥ 11.0 | GPU acceleration (NVIDIA) |
| **Docker + Compose** | Latest | Full-stack deployment |

## 🔨 Building

### C++ Engine

#### Windows (MSVC + vcpkg)

```powershell
# One-time: set VCPKG_ROOT environment variable
$env:VCPKG_ROOT = "C:\path\to\vcpkg"

# Configure and build
cd backend
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

#### macOS / Linux

```bash
cd backend
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build -j$(nproc)
```

#### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_CUDA` | `ON` | Enable CUDA GPU acceleration (auto-detects toolkit) |
| `ENABLE_HW_ACCEL` | `ON` | Enable AVX2/NEON SIMD acceleration |
| `SKIP_METAL_SHADERS` | `OFF` | Skip Metal shader compilation on macOS |

### Rust TUI

```bash
cd cli-rs
cargo build --release
```

### Go Microservices

All microservices can be built together from the repository root using the Go workspace (`go.work`):

```bash
go build ./...
```

Or each service can be built independently:

```bash
cd services/scheduler && go build -o scheduler ./cmd/scheduler
cd services/registry && go build -o registry .
cd services/cache && go build -o cache .
```

### Web Frontend

```bash
cd web
npm install
npm run build
```

## 🚀 Running

### 1. Start the Engine

```bash
# Native
./backend/build/qubit_engine        # Linux/macOS
.\backend\build\Release\qubit_engine.exe  # Windows

# The engine listens on 0.0.0.0:50051 (gRPC)
```

### 2. Launch the TUI

```bash
cd cli-rs
cargo run
# Use ↑/↓ to select circuits, Enter to execute, 'v' for VQE, 'q' to quit
```

### 3. Full Stack (Docker Compose)

```bash
docker compose -f deploy/docker/docker-compose.yaml up --build
```

This starts: engine (port 50051), PostgreSQL (5432), registry (50052), Redis (6379), scheduler (50053), and cache (50054), plus Envoy proxy and Web frontend.

### 4. Web Frontend (Local Development)

```bash
cd web
npm run dev
# The web interface will be available at http://localhost:3000
```

## 🧪 Testing

### C++ Unit Tests

```bash
cd backend/build
ctest --output-on-failure
```

The test suite covers: quantum gates, JIT compiler, OpenQASM round-trip, differentiator (parameter-shift + adjoint), circuit optimizer, noise model (Kraus completeness, channel application, readout errors), CPU backend, mock hardware backend, and gRPC service handlers.

### Python Binding Verification

```bash
cd backend/tests
python verify_bindings.py
```

## 🐳 Deployment

### Docker Compose (Local)

```bash
# Copy and configure environment
cp .env.example .env

# Build and start all services
docker compose -f deploy/docker/docker-compose.yaml up --build
```

### Kubernetes (Helm)

```bash
helm install qubit-engine deploy/helm/ \
  --namespace qubit-engine \
  --create-namespace
```

### Kubernetes (Raw Manifests)

```bash
kubectl apply -f deploy/k8s/namespace.yaml
kubectl apply -f deploy/k8s/
```

## ⚠️ Known Limitations & Architectural Roadmap

The following status accurately reflects verified capabilities vs. planned roadmap items:

- **Hardware Acceleration Coverage**: Explicit AVX2 (`__m256d`) and ARM NEON (`float64x2_t`) vector intrinsics are implemented in `applyHadamard` ([CpuBackend.cpp](backend/src/backends/CpuBackend.cpp)). Other gate kernels rely on scalar loops combined with OpenMP thread parallelization and compiler auto-vectorization.
- **Metal Backend Selection**: `MetalBackend` (Apple Silicon GPU) is integrated into `BackendFactory::create` on macOS (`#ifdef ENABLE_METAL`), automatically selecting Apple Silicon GPU acceleration when available ahead of CPU fallback.
- **CUDA Single-GPU vs Multi-GPU**: `CudaBackend` executes on a single NVIDIA GPU. Multi-GPU execution via NCCL (`CudaBackend::applyGateDistributed`) is designed, not integrated: NCCL communicator initialization is uncoordinated across independent server processes.
- **MPI Distributed State-Vector Simulation**: Point-to-point state buffer swap routines exist in `CpuBackend`, but distributed SPMD simulation across multi-node clusters is designed, not integrated. The engine runs strictly single-node in production; Kubernetes manifests and Helm charts deploy standalone, independent engine pods.
- **Tensor Networks (MPS) Observable Extraction**: `MPSBackend` maintains 1D matrix product states with SVD truncation up to configurable bond dimension ($\chi$, default 64 per `ConfigManager.cpp:49` and `MPSBackend.hpp:40`). Observable extraction is limited to single-node contraction: state vectors are supported up to $N \le 30$ and probabilities up to $N \le 25$. Expectation value calculation throws `FeatureNotSupportedException` (general tensor network contraction is designed, not integrated).
- **Same-Host Shared-Memory IPC**: `ipc::SharedMemory` in C++ and `pkg/ipc` in Go implement shared memory segment creation and mapping. However, end-to-end integration into the application mesh is designed, not integrated: Go microservices and the Rust TUI client currently exchange state vectors exclusively via standard gRPC protobuf serialization.
- **Python Buffer Transfer**: `QuantumRegister::getStateVector()` returns `std::vector<Complex>` by value from the C++ backend. The Python bindings move this vector onto the heap and pass it via `py::capsule` to avoid duplicate copies during the C++-to-Python handoff to NumPy.
- **Cloud Backend Submissions**: `RigettiBackend` and `IonQBackend` in the Go backend package are mock execution stubs that return simulated job IDs without connecting to physical QPU hardware APIs.
- **Autoscaling Metrics**: The Go scheduler exposes queue depth metrics (`quantum_job_queue_depth`) on `:2112/metrics` for reactive Kubernetes HorizontalPodAutoscalers; predictive autoscaling is not implemented.
- **gRPC API Connection Standard**: All Go microservices, integration tests, and client connection helpers are fully migrated to `grpc.NewClient` (gRPC 1.0.4+).
- **Cache Microservice Resilience**: `services/cache` updates hit counts asynchronously in Redis with fallback logging to prevent read requests from failing on transient storage write errors.
- **TUI State Representation**: `cli-rs` streams basis state probabilities as binary strings (`|0...0>`) matched to the target circuit qubit width.

## 📄 License

This project is licensed under the MIT License.

