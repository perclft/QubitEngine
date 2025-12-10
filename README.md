# QubitEngine

Infrastructure for the Quantum-Ready Future. I built it to solve the deployment, scaling, and memory-management challenges that will exist when real quantum hardware becomes accessible via API.

is a high-performance, distributed quantum circuit simulator designed for state vector evolution on classical hardware. It implements a decoupled microservices architecture, separating the compute-intensive physics kernel (C++20) from the control plane (Go) via a strict gRPC contract.
This system is engineered for horizontal scalability, utilizing OpenMP for shared-memory parallelization within nodes and Kubernetes for distributed load balancing across nodes.

# System Architecture
The project follows a cloud-native design pattern:
# Compute Engine (Backend):
A C++20 application that manages the Hilbert space state vector. It handles unitary gate applications (Hadamard, Pauli-X, CNOT) and non-unitary measurement operations (wavefunction collapse).

# Control Plane (CLI):
A Go-based command-line interface (qctl) that serializes user intent into Protocol Buffers and transmits them to the engine.

# Communication:
gRPC (HTTP/2) is used for low-latency, strongly typed communication between components.

# Infrastructure:
The system is containerized (Docker) and designed to run as a stateless ReplicaSet on Kubernetes.

# Key Features

Parallel Execution: Gate operations are parallelized across CPU cores using OpenMP SIMD instructions.
Non-Unitary Simulation: Supports probabilistic measurement and true wavefunction collapse logic (Born rule implementation).
Memory Safety: Implements hardware-aware pre-flight checks to reject circuits that exceed available physical RAM, preventing OOM crashes.
Fault Tolerance: Designed with Kubernetes Liveness and Readiness probes to ensure automatic recovery from deadlocks or failures.

# Prerequisites
Docker (20.10+)Kubernetes Cluster (Minikube, Kind, or Docker Desktop)Go (1.23+)MakeBuild

# Instructions 
The project uses a Makefile to automate the build pipeline.1. Build Docker ImagesThis command compiles the C++ engine and Go CLI into optimized, multi-stage Docker images (based on Alpine Linux)

make docker-build

# 2. Local Development Build (Optional)If you wish to build binaries locally without Docker:Bash# Generate Protobuf code
make proto

# Build C++ Engine (Requires CMake & GCC 11+)
make build-cpp

# Build Go CLI
make build-go

# Usage
Running Locally (Docker)
Start the Engine
Run the simulation engine in a background container.

docker network create qubit-net
docker run -d --net qubit-net --name qubit-engine -p 50051:50051 qubit-engine:latest

# Run a Circuit. Use the qctl client to submit a job. The client connects to the engine, sends the circuit definition, and prints the resulting state vector and measurement bits.

docker run --rm --net qubit-net qctl:latest -server qubit-engine:50051

Expected Output:PlaintextSending circuit: Bell State + Measurement...

--- Classical Measurement Results ---
Qubit 0 collapsed to: |1>
Qubit 1 collapsed to: |1>

--- Final Wavefunction (Collapsed) ---
State |3>: (1.000 + 0.000i)


Kubernetes DeploymentFor production-grade deployment, use the provided manifests to deploy a load-balanced cluster.1. Deploy the ClusterThis creates a Namespace, a Deployment (3 Replicas), and a LoadBalancer Service.

kubectl apply -f deploy/k8s/namespace.yaml
kubectl apply -f deploy/k8s/engine-deployment.yaml
kubectl apply -f deploy/k8s/engine-service.yaml

# 2. Verify StatusEnsure all 3 replicas are Running.

kubectl get pods -n qubit-system

# 3. Send Distributed RequestsPoint the local client to the Kubernetes Service. Requests will be round-robin load balanced across the available engines.

./bin/qctl -server localhost:50051

# Project StructurePlaintext.
├── api
│   └── proto            # Protocol Buffer definitions (gRPC contract)
├── backend              # C++ Compute Engine
│   ├── src              # Source code (QuantumRegister, ServiceImpl)
│   ├── include          # Header files
│   └── tests            # GoogleTest unit tests
├── cli                  # Go Control Plane
│   ├── cmd              # Entry point for qctl
│   └── internal         # Generated gRPC client code
├── deploy
│   ├── docker           # Multi-stage Dockerfiles
│   └── k8s              # Kubernetes Manifests (Deployment, Service)
└── makefile             # Automation scripts

# Performance & Limitations

State Vector Size: The memory requirement scales exponentially ($16 \times 2^N$ bytes).20 Qubits: ~16 MB28 Qubits: ~4 GB30 Qubits: ~16 GB

# Hard Limits: The engine enforces a hard limit of 30 qubits to protect the host system.

# LicenseThis project is licensed under the MIT License. See the LICENSE file for details.
