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

- **Multi-backend simulation** — CUDA (multi-GPU with NCCL), Metal, AVX2/NEON, and MPI distributed execution
- **Distributed Tensor Networks** — Matrix Product State (MPS) backend scaling beyond 32 qubits with SVD truncation
- **Clifford Stabilizer QEC** — Gottesman-Knill simulation for thousands of qubits in polynomial time
- **Zero-Copy IPC** — Direct POSIX shared memory mapped state vector transfers mitigating gRPC overhead
- **Native differentiability** — Parameter Shift Rule and Adjoint differentiation for variational algorithms (VQE)
- **JIT circuit optimization** — LRU caching & multi-level gate fusion compiler (O0–O3) utilizing background multi-threading
- **Predictive Autoscaling Mesh** — Integrated Prometheus metrics querying Redis queue lengths mapping into Kubernetes HPA
- **OpenQASM 2.0/3.0** — Import and export circuits for interoperability with Qiskit and other frameworks
- **Interactive TUI** — Rust-based terminal dashboard with live probability streaming via gRPC
- **Python bindings** — pybind11 module equipped with zero-copy NumPy buffers and PyTorch integration

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Rust TUI (Ratatui)                    │
│              Interactive terminal dashboard              │
└────────────────────────┬────────────────────────────────┘
                         │ gRPC Streaming
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
│    ├─ CpuBackend   (AVX2/NEON + OpenMP)                 │
│    ├─ CudaBackend  (NVIDIA multi-GPU + NCCL)            │
│    ├─ MetalBackend (Apple GPU + Async Cmd Queues)       │
│    ├─ MPSBackend   (Tensor Networks + SVD Truncation)   │
│    ├─ StabilizerBackend (Clifford QEC)                  │
│    └─ CloudBackend (remote execution)                    │
│  QuantumJIT (LRU Caching) · Differentiator · Optimizer  │
│  Python Bindings (pybind11 zero-copy NumPy arrays)       │
└─────────────────────────────────────────────────────────┘
```

> See [`docs/architecture.md`](docs/architecture.md) for a detailed breakdown of backend selection, JIT tiers, gRPC API surface, and differentiator methods.

## ⚡ Performance Benchmarks

| Backend | Platform | Qubits | Bandwidth |
| --- | --- | --- | --- |
| **Metal** | macOS (M3 Air) | 25 | **19.03 GB/s** |
| **CUDA** | Linux/Windows | 25 | **VRAM Optimized** |
| **AVX2** | Windows | 25 | **6.64 GB/s** |
| **MPI (n=2)** | Distributed | 22 | **3.15 GB/s** |

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

Each service builds independently:

```bash
cd services/scheduler && go build -o scheduler .
cd services/registry && go build -o registry .
cd services/cache && go build -o cache .
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

This starts: engine (port 50051), PostgreSQL (5432), registry (50052), Redis (6379), scheduler (50053), and cache (50054).

## 🧪 Testing

### C++ Unit Tests

```bash
cd backend/build
ctest --output-on-failure
```

The test suite covers: quantum gates, JIT compiler, OpenQASM round-trip, differentiator (parameter-shift + adjoint), circuit optimizer, CPU backend, mock hardware backend, and gRPC service handlers.

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

## 📄 License

This project is licensed under the MIT License.
