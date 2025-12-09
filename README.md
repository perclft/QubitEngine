QubitEngine
A Distributed, Memory-Safe Quantum Circuit Simulator.
QubitEngine is a high-performance compute microservice designed to simulate quantum circuits. Unlike standard Python-based simulators, this engine is built for Cloud-Native Infrastructure, treating quantum state vectors as distributed jobs scheduled across a Kubernetes cluster.
It features hardware-aware memory safeguards to prevent OOM (Out of Memory) kills, OpenMP parallelization for multi-core processing, and a gRPC/Protobuf interface for strict contract enforcement between the C++ Engine and the Go Control Plane.
🚀 Key Engineering Features
 * Hardware-Aware Safety: Implements Linux system calls (sys/sysinfo.h) to verify physical RAM availability before attempting exponential state vector allocations.
 * High-Performance Compute: core math engine written in C++20, utilizing #pragma omp parallel for multi-threading and AVX2 instructions for vectorized complex number arithmetic.
 * Cloud-Native Protocol: Abandons REST for gRPC (HTTP/2), reducing serialization overhead and enforcing strong typing across microservices.
 * Systems-Level Optimization: Uses cache-friendly memory strides (Block/Stride pattern) for gate application, minimizing cache misses during large matrix operations.
 * Polyglot Architecture: Decouples the "Compute Plane" (C++) from the "Control Plane" (Go CLI), demonstrating modern distributed system design.
🛠️ Tech Stack
 * Core Engine: C++20, OpenMP, Abseil
 * Communication: gRPC, Protocol Buffers
 * Infrastructure: Docker (Multi-Stage Alpine Builds), Kubernetes
 * Testing: GoogleTest (GTest)
 * Build System: CMake, Make, vcpkg
📂 Project Structure
QubitEngine/
├── api/proto/            # The Contract (Protobuf Definitions)
├── backend/              # The Compute Engine (C++20)
│   ├── src/              # Source Code (Service & Math Logic)
│   ├── tests/            # GoogleTest Unit Suites
│   └── CMakeLists.txt    # Build Configuration (Auto-generates Protos)
├── cli/                  # The Control Plane (Go)
│   └── cmd/qctl/         # CLI Entrypoint
├── deploy/               # Infrastructure as Code
│   ├── docker/           # Multi-stage Dockerfiles (Alpine)
│   └── k8s/              # Kubernetes Manifests (Deployments/Services)
└── Makefile              # Automation Entrypoint

⚡ Getting Started (Local Dev)
Prerequisites (Arch Linux)
This project uses vcpkg for reproducible C++ dependency management.
# 1. Install System Build Tools
sudo pacman -Syu base-devel git cmake

# 2. Install & Bootstrap vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh

# 3. Clone Repository
git clone https://github.com/perclft/QubitEngine.git
cd QubitEngine

Build & Run
The project includes a Makefile to orchestrate the polyglot build process.
# 1. Build C++ Engine and Go CLI
# (First run will take ~5-10 mins to compile gRPC dependencies via vcpkg)
make

# 2. Run Unit Tests (Validates Math Logic)
make test
# Output: [100%] Built target qubit_tests... 4/4 Test #1: QuantumTest.BellState ... Passed

# 3. Run the Engine Locally
./backend/build/qubit_engine
# Output: QubitEngine (C++) listening on 0.0.0.0:50051

Usage (CLI)
In a separate terminal, use the Go CLI (qctl) to submit a circuit job.
# Submit a Bell State Circuit (Hadamard + CNOT)
./cli/bin/qctl --server localhost:50051

☁️ Deployment (Kubernetes)
This project is designed for Rancher/K3s clusters. It uses highly optimized Alpine Linux images (~15MB runtime size).
1. Build Docker Images
make docker-build

2. Deploy to Cluster
make deploy

This applies the manifests in deploy/k8s/, creating:
 * Namespace: qubit-system
 * Deployment: 2 Replicas of the C++ Engine (with resource limits).
 * Service: ClusterIP for internal gRPC communication.
3. Verify Deployment
kubectl get pods -n qubit-system
kubectl logs -l app=qubit-engine -n qubit-system

🔬 Technical Deep Dive
Memory Safety Guard
To prevent the Kubernetes OOM Killer from terminating pods during high-qubit simulations, the engine performs a pre-flight check:
// backend/src/ServiceImpl.cpp
bool hasEnoughMemory(int num_qubits) {
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    size_t required = 1ULL << num_qubits * sizeof(complex<double>);
    return memInfo.freeram > (required + OVERHEAD_BUFFER);
}

Parallel Gate Application
Quantum gates are applied using a Stride Pattern rather than full matrix multiplication, allowing O(N) complexity instead of O(N^2). This loop is parallelized via OpenMP:
// backend/src/QuantumRegister.cpp
#pragma omp parallel for
for (size_t i = 0; i < limit; i += 2 * stride) {
    // Apply gate logic to pairs in parallel
}

License
MIT
