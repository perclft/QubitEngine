QubitEngine: Distributed Quantum Circuit Simulator
QubitEngine is a high-performance, distributed quantum computing simulator designed to emulate quantum circuits on classical hardware. It utilizes a microservices architecture, separating the control plane (Go CLI) from the compute engine (C++20) via a strict gRPC/Protobuf contract.

This system is engineered for high-throughput state vector simulation, leveraging OpenMP for parallelization and hardware-aware memory management to ensure stability in constrained environments.

Project Overview
Architecture: Distributed Microservices grpc
Compute Engine: C++20(OpenMP Parallelized)
Control Plane: Go (Golang)
Protocol: HTTP/2 over TCP (Protobuf)
Infrastructure: Dockerized (Alpine Linux)
License: MIT
System Architecture:
The system decouples the user intent (Circuit Definition) from the heavy computation (State Vector Evolution).
The Client (Go): A lightweight CLI that accepts user input, validates circuit parameters, and serializes the request into Protocol Buffers.
The Network Layer (gRPC): Transmits binary-encoded circuit data over HTTP/2. This ensures low-latency communication and strict type safety between the polyglot components.
The Engine (C++): A multi-threaded linear algebra worker. It allocates the Hilbert Space (State Vector), applies quantum logic gates (Hadamard, CNOT, Pauli-X), and returns the measurement probabilities.
Key Features:
High-Performance Compute (HPC)
Parallel Execution: Utilizes OpenMP #pragma omp parallel for to parallelize matrix operations across all available CPU cores.
Memory Optimization: Uses contiguous memory allocation (std::vector<std::complex<double>>) to minimize cache misses during stride loops.
Systems Reliability & SafetyHardware-Aware Memory Guards: The engine queries the kernel (sys/sysinfo.h) before allocation. It rejects requests that exceed available physical RAM, preventing OOM (Out-of-Memory) kills and system instability.
Strict Input Validation: Enforces hard limits on qubit count (N < 29) to prevent exponential resource exhaustion.
Cloud-Native Deployment Containerization: Fully dockerized using a streamlined build process that bundles all shared libraries (abseil, grpc, protobuf).
Orchestration Ready: Stateless architecture allows for horizontal scaling in Kubernetes environments.
PrerequisitesBuild System: CMake (3.15+), MakeCompilers: GCC 11+ (Support for C++20), Go 1.20+
Dependencies: gRPC, Protobuf, Abseil-cpp, OpenMPInstallation
BuildLocal Development (Linux/Arch)
Clone the repository:Bashgit clone https://github.com/username/QubitEngine.git
cd QubitEngine
Build the project:This command compiles the C++ backend, generates the Go gRPC code, and builds the CLI binary.Bashmake
Run Tests:Executes GoogleTest unit tests for the quantum math logic.Bashmake test
Container DeploymentThe application is designed to run as a Docker container.Build the Image:Bashdocker build -t qubit-engine:latest -f deploy/docker/Dockerfile.engine .
Run the Container:Runs the engine in the background, exposing port 50051.Bashdocker run -d -p 50051:50051 --name quantum-engine qubit-engine:latest
UsageRunning a SimulationUse the compiled Go CLI (qctl) to send jobs to the running server.Example: Bell State SimulationCreates a 2-qubit circuit, applies a Hadamard gate to Qubit 0, and controls Qubit 1 (Entanglement).Bash./bin/qctl --server localhost:50051 --qubits 2
Expected Output:PlaintextSending circuit: 2 Qubits...
Result State Vector:
|00>: (0.707 + 0.000i)  [50% Probability]
|11>: (0.707 + 0.000i)  [50% Probability]
Project StructurePlaintextQubitEngine/
├── api/
│   └── proto/          # Protocol Buffer definitions (.proto)
├── backend/            # C++ Compute Engine
│   ├── src/            # Source code (main.cpp, QuantumRegister.cpp)
│   ├── include/        # Header files
│   └── tests/          # GoogleTest unit tests
├── cli/                # Go Control Plane
│   ├── cmd/            # Entry point
│   └── internal/       # Generated gRPC code
├── deploy/
│   └── docker/         # Dockerfiles for Engine and CLI
└── Makefile            # Build automation
Technical Challenges & Solutions
Cross-Language Linking: Solved "Dependency Hell" by manually configuring CMake toolchains to link abseil and grpc shared objects correctly across Alpine Linux environments.
Exponential Scaling: Addressed the $2^N$ memory requirement of state vectors by implementing a Pre-Flight System Check that validates available RAM against the requested circuit depth.
