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
<a href="#performance">Benchmarks</a> •
<a href="#quick-start">Quick Start</a>
</p>

---

## 🌟 Overview

**QubitEngine** is a high-performance quantum simulation platform built specifically as a robust R&D tool for researchers. It features a unified C++20 physics kernel with **automatic hardware detection**, enabling seamless execution across **Linux (CUDA)**, **macOS (Metal)**, and **Windows (AVX2)**.

To easily explore and trace internal wavefunctions, QubitEngine provides a native interactive **Terminal User Interface (TUI)** written in **Rust** (Ratatui), connecting directly to the core simulator over gRPC streaming.

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
* **TUI Dashboard**: A Rust Ratatui client that streams live probability matrix data for immediate graphical analysis.

## ⚡ Performance Benchmarks

QubitEngine is optimized for maximum memory bandwidth, essential for large-scale state vector manipulation.

| Backend | Platform | Qubits | Bandwidth |
| --- | --- | --- | --- |
| **Metal** | macOS (M3 Air) | 25 | **19.03 GB/s** |
| **CUDA** | Linux/Windows | 25 | **VRAM Optimized** |
| **AVX2** | Windows | 25 | **6.64 GB/s** |
| **MPI (n=2)** | Distributed | 22 | **3.15 GB/s** |

## 🚀 Quick Start

1. Start the distributed backend engine natively or via Makefile:

```bash
make build-engine
./build/qubit_engine
```

1. Boot the Rust terminal user interface to stream executions interactively:

```bash
cd cli-rs
cargo run
```

## 📄 License

This project is licensed under the MIT License.
