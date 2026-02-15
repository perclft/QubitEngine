# QubitEngine Architecture

This document provides a high-level overview of the QubitEngine architecture, illustrating the data flow from the user interface down to the hardware execution layers.

## System Overview

The system is composed of three main layers:
1.  **Frontend**: A React-based web interface for users to design circuits and view results.
2.  **Orchestrator**: A Go-based microservice that schedules jobs and manages state.
3.  **Engine**: A high-performance C++ application that executes quantum simulations.

## Data Flow Diagram

```mermaid
graph TD
    User([User / Frontend]) -->|HTTP/gRPC| GoScheduler[Go Scheduler Service]
    GoScheduler -->|gRPC| CppEngine[C++ Qubit Engine]
    
    subgraph "C++ Execution Engine"
        CppEngine -->|Job Request| QuantumRegister[QuantumRegister Facade]
        QuantumRegister -->|Select Backend| BackendSelector{Backend Selector}
        
        BackendSelector -->|CPU| CpuBackend[CpuBackend]
        BackendSelector -->|GPU| CudaBackend[CudaBackend]
        
        CpuBackend -->|OpenMP/AVX| CpuExecution[CPU Execution]
        CudaBackend -->|CUDA Runtime| GateKernels[CUDA Kernels (.cu)]
        
        GateKernels -->|nVidia Driver| GPU[GPU Hardware]
    end
```

## Component Details

### 1. Go Scheduler
- **Role**: Acts as the API gateway and job queue.
- **Communication**: Receives JSON/gRPC requests from the frontend.
- **Responsibility**: Dispatches simulation tasks to available C++ Engine instances.

### 2. C++ Qubit Engine
- **Role**: The core computational unit.
- **QuantumRegister**: The main entry point for quantum operations. It abstracts the underlying backend hardware.
- **Backend Selection**:
    - Automatic detection based on build flags (`ENABLE_CUDA`) and hardware availability.
    - Fallback to CPU if GPU is unavailable.

### 3. Backends
- **CpuBackend**:
    - Optimized with OpenMP for multi-threading.
    - Uses AVX2/NEON intrinsics for vectorization.
- **CudaBackend**:
    - leverages NVIDIA GPUs for massive parallelism.
    - Implemented using CUDA C++ kernels (`gate_kernels.cu`).
    - Explicit memory management between Host (CPU) and Device (GPU).

## Build System
The project uses **CMake** for build configuration, handling dependencies like `gRPC`, `Protobuf`, and `NVIDIA CUDA Toolkit`.
