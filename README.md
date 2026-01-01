# QubitEngine

<p align="center">
<img src="docs/images/logo.png" alt="QubitEngine Logo" width="120" />
</p>

<p align="center">
<strong>⚛️ Unified, Distributed, Cross-Platform Quantum Experimentation Platform</strong>
</p>

<p align="center">
<a href="#architecture">Architecture</a> •
<a href="#modules">Application Modules</a> •
<a href="#performance">Benchmarks</a> •
<a href="#quick-start">Quick Start</a>
</p>

---

## 🌟 Overview

**QubitEngine** is a high-performance quantum simulation platform designed to bridge the gap between low-level hardware acceleration and cloud-native application design. It features a unified C++20 physics kernel with **automatic hardware detection**, enabling seamless execution across **Linux (CUDA)**, **macOS (Metal)**, and **Windows (AVX2)**.

## 🏗️ Architecture Design

### 1. Unified Physics Kernel (C++20)

The engine utilizes a polymorphic `IQuantumBackend` interface to abstract hardware-specific complexities:

* **Auto-Hardware Detection**: Dynamically selects between **CUDA**, **Metal**, or **AVX2/NEON** at runtime based on the host environment.
* **Native Differentiability**: Implements the **Parameter Shift Rule** in C++ for analytical gradient calculations, bypassing the overhead of numerical differentiation.

### 2. Distributed Simulation Layer (MPI)

For systems exceeding local memory limits (30+ qubits), the engine shards the state vector across a Kubernetes cluster using **MPI**.

### 3. Application Mesh (Go & gRPC)

A modular microservices architecture handles high-level logic and orchestration:

* **Scheduler**: A priority-aware job queue backed by **Redis**.
* **Registry**: Persistent circuit storage using **PostgreSQL**.

## 🧩 Application Modules

* **Quantum Music Composer**: Generates melodic sequences and MIDI files using quantum superposition and entanglement probabilities.
* **Quantum Cryptography**: A sandbox for testing **BB84 QKD** protocols with real-time eavesdropping detection.
* **Quantum Education**: Interactive learning platform with a circuit library, gamified quizzes, and step-by-step state snapshots.

## ⚡ Performance Benchmarks

QubitEngine is optimized for maximum memory bandwidth, essential for large-scale state vector manipulation.

| Backend | Platform | Qubits | Bandwidth |
| --- | --- | --- | --- |
| **Metal** | macOS (M3 Air) | 25 | **19.03 GB/s** |
| **CUDA** | Linux/Windows | 25 | **VRAM Optimized** |
| **AVX2** | Windows | 25 | **6.64 GB/s** |
| **MPI (n=2)** | Distributed | 22 | **3.15 GB/s** |

## 🚀 Quick Start

```bash
# Start the full microservices mesh with Docker Compose
docker compose -f deploy/docker/docker-compose.yaml up --build

```

Access the **Visual Dashboard** at `http://localhost:5173`.

## 📄 License

This project is licensed under the MIT License.
