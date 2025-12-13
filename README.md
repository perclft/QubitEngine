<p align="center">
  <img src="docs/images/logo.png" alt="QubitEngine Logo" width="120" />
</p>

<h1 align="center">QubitEngine</h1>

<p align="center">
  <strong>⚛️ High-Performance, Distributed, Differentiable Quantum Simulator</strong>
</p>

<p align="center">
  <a href="#quick-start">Quick Start</a> •
  <a href="#architecture">Architecture</a> •
  <a href="#applications">Applications</a> •
  <a href="#python-bindings">Python SDK</a> •
  <a href="#deployment">Deployment</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C++-20-blue?logo=cplusplus" alt="C++20" />
  <img src="https://img.shields.io/badge/Python-3.8+-3776AB?logo=python" alt="Python" />
  <img src="https://img.shields.io/badge/gRPC-Protobuf-4285F4?logo=google" alt="gRPC" />
  <img src="https://img.shields.io/badge/MPI-OpenMPI-orange" alt="MPI" />
  <img src="https://img.shields.io/badge/License-MIT-green" alt="MIT License" />
</p>

---

## Overview

**QubitEngine** is a high-performance, cloud-native quantum simulator engineered for **scale** and **precision**. It bridges the gap between high-performance computing (HPC) and modern cloud architectures, enabling simulation of large quantum systems that exceed the memory limits of a single machine.

Unlike traditional academic simulators, QubitEngine is designed as a **Microservices Mesh**, where the core physics engine runs as a highly optimized C++ service, accessible via gRPC or native Python bindings.

## 🏗️ Architecture Design

QubitEngine employs a hybrid architecture to maximize performance while maintaining ease of use.

### 1. Core Physics Kernel (C++20)

At the heart of the engine is a rigorously optimized C++ kernel.

- **Vectorization**: Hand-tuned AVX2/AVX-512 intrinsics for state vector manipulation.
- **Parallelism**: OpenMP for multi-threading on multi-core processors.
- **Differentiability**: Native implementation of the **Parameter Shift Rule**, allowing for analytical gradient calculations directly in C++, bypassing the overhead of numerical differentiation.

### 2. Distributed Simulation Layer (MPI)

For systems exceeding local RAM (approx. 30+ qubits), QubitEngine shards the state vector across a Kubernetes cluster.

- **MPI Integration**: Uses Message Passing Interface to synchronize state updates across nodes.
- **Zero-Copy**: Optimized memory management to minimize data movement between shards.

### 3. Interface Layer

- **gRPC API**: Language-agnostic interface defined in `quantum.proto`, allowing clients (Go, Rust, Node.js) to request simulations, stream results, and control jobs.
- **Python Bindings (`pybind11`)**: Direct access to the C++ kernel from Python, enabling seamless integration with the scientific Python ecosystem (NumPy, SciPy).

---

## 🚀 Applications

### 🧪 Scientific Research & QML

QubitEngine is optimized for **Variational Quantum Eigensolvers (VQE)** and **Quantum Machine Learning (QML)**.

- **Fast Gradients**: Native differentiable simulation accelerates training of parameterized quantum circuits (PQC).
- **Molecular Simulation**: Built-in support for molecular Hamiltonians (e.g., H2, LiH).

### 🎮 Quantum Game Development

Designed to power the next generation of games with real quantum mechanics.

- **True Randomness**: Generate entropy from actual quantum superposition measurements.
- **Entanglement Mechanics**: Create game states where objects are probabilistically linked across the map.

---

## 🐍 Python Bindings

Experience the performance of C++ with the ease of Python.

```bash
# Build the Python module
cd backend && mkdir build && cd build
cmake .. && make
```

**Usage Example:**

```python
import qubit_engine
import math

# Initialize a register
q = qubit_engine.QuantumRegister(2)

# Create Bell State |00> + |11>
q.applyHadamard(0)
q.applyCNOT(0, 1)

# Measure
probs = q.getProbabilities()
print(f"Probabilities: {probs}")
```

---

## ⚡ Quick Start (Service Mode)

Run the full stack (Engine + API + Dashboard) locally using Docker Compose.

```bash
# Clone the repository
git clone https://github.com/perclft/QubitEngine.git
cd QubitEngine

# Start services
docker compose -f deploy/docker/docker-compose.yaml up --build
```

Access the **Visual Dashboard** at `http://localhost:5173`.

---

## ☁️ Deployment

QubitEngine is "Kubernetes Native". Check the `deploy/k8s` directory for Helm charts and manifests to deploy a distributed simulation cluster.

```bash
kubectl apply -f deploy/k8s/mpi-cluster.yaml
```

---

## 🤝 Contributing

Contributions are welcome! Please read `CONTRIBUTING.md` for details on our code of conduct and the process for submitting pull requests.

## 📄 License

This project is licensed under the MIT License - see the `LICENSE` file for details.
