# QubitEngine

**Infrastructure for the Quantum-Ready Future.** A high-performance, distributed quantum circuit simulator designed for state vector evolution on classical hardware.

![QubitEngine Dashboard](docs/images/dashboard.png)

## Overview

QubitEngine implements a decoupled microservices architecture, separating the compute-intensive physics kernel (C++20) from the control plane (Go) via a strict gRPC contract. This system is engineered for horizontal scalability, utilizing OpenMP for shared-memory parallelization within nodes and Kubernetes for distributed load balancing across nodes.

### Key Features

| Feature | Description |
|---------|-------------|
| **Live Visualization** | Real-time 3D Bloch Sphere visualization with streaming state updates |
| **Quantum Audio Synthesis** | Sonification of quantum states (phase → pitch, amplitude → volume) |
| **Depolarizing Noise** | Configurable quantum noise simulation for realistic decoherence |
| **Visual Overdrive Mode** | Post-processing effects (Bloom, Chromatic Aberration, Vignette) |
| **Parallel Execution** | Gate operations parallelized using OpenMP SIMD instructions |
| **Cluster Scalability** | Horizontal scaling with Kubernetes (3+ replicas) |
| **Security Hardening** | Non-root containers, resource limits, health probes |

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         QubitEngine System                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌─────────────┐    gRPC-Web    ┌─────────────┐    gRPC    ┌──────┐│
│  │ React + 3D  │◄──────────────►│   Envoy     │◄──────────►│Engine││
│  │  Dashboard  │     :8080      │   Proxy     │    :50051  │ (C++)││
│  └─────────────┘                └─────────────┘            └──────┘│
│       :5173                                                        │
│                                                                     │
│  Features:                      Features:                Features: │
│  • Bloch Sphere (Three.js)      • gRPC-Web Bridge        • State   │
│  • Generative Canvas            • Load Balancing           Vector  │
│  • Quantum Audio                                         • Gates   │
│  • Visual Overdrive                                      • Noise   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Components

- **Compute Engine (C++20)**: Manages Hilbert space state vectors. Handles unitary gate applications (H, X, Y, Z, CNOT, Toffoli, Phase, Rotation) and non-unitary measurement operations.
- **Control Plane (Go CLI)**: `qctl` serializes user intent into Protocol Buffers for transmission to the engine.
- **Web Dashboard (React + Three.js)**: Real-time 3D visualization with audio synthesis and post-processing effects.
- **Envoy Proxy**: Bridges gRPC-Web (browser) to native gRPC (backend).

---

## Quick Start

### Prerequisites

- Docker (20.10+)
- Docker Compose v2

### Running with Docker Compose

```bash
# Clone the repository
git clone https://github.com/yourusername/QubitEngine.git
cd QubitEngine

# Build and start all services
docker compose -f deploy/docker/docker-compose.yaml up --build

# Access the dashboard
open http://localhost:5173
```

### Services

| Service | Port | Description |
|---------|------|-------------|
| Web Dashboard | 5173 | React frontend with 3D visualization |
| Envoy Proxy | 8080 | gRPC-Web to gRPC bridge |
| Quantum Engine | 50051 | C++ compute backend |

---

## Features in Detail

### 🌀 Live Bloch Sphere Visualization

The dashboard displays a real-time 3D Bloch sphere that animates as the quantum state evolves. The state vector is rendered with:
- **Neon trails** showing the path of quantum evolution
- **Dynamic coloring** based on quantum phase
- **Smooth interpolation** between states

### 🔊 Quantum Audio Synthesis

Enable the "Quantum Audio" checkbox to hear the quantum state:
- **Amplitude → Volume**: Higher probability = louder tone
- **Phase → Pitch**: Phase rotation creates a sci-fi "warble" effect
- **Test Button**: Verify browser audio is working

### 🎨 Visual Overdrive Mode

Toggle advanced post-processing effects:
- **Bloom**: Glowing neon aesthetic
- **Chromatic Aberration**: Prismatic edge effects
- **Noise**: Film grain overlay
- **Vignette**: Cinematic darkened edges

> ⚠️ **Warning**: Visual Overdrive is GPU-intensive. Disable if experiencing lag.

### 📡 Depolarizing Noise

Simulate quantum decoherence with the "Entropy" slider:
- Range: 0.0 (pure) to 0.5 (maximum noise)
- Applies random Pauli errors (X, Y, Z) after each gate
- Watch the Bloch vector jitter erratically under noise

---

## CLI Usage

### Using JSON Circuit Files

```bash
# Build the CLI
make build-go

# Run a circuit from JSON
./bin/qctl -file circuits/bell.json
```

### Example Circuit (bell.json)

```json
{
  "name": "Bell State Entanglement",
  "qubits": 2,
  "ops": [
    { "gate": "H", "target": 0 },
    { "gate": "CNOT", "control": 0, "target": 1 },
    { "gate": "M", "target": 0 },
    { "gate": "M", "target": 1 }
  ]
}
```

### Streaming Mode

```bash
./bin/qctl -file circuits/stream_test.json -stream
```

---

## Kubernetes Deployment

### Deploy to Cluster

```bash
# Create namespace and deploy
kubectl apply -f deploy/k8s/namespace.yaml
kubectl apply -f deploy/k8s/engine-deployment.yaml
kubectl apply -f deploy/k8s/engine-service.yaml

# Verify pods (3 replicas)
kubectl get pods -n qubit-system

# Example output:
# NAME                           READY   STATUS    RESTARTS   AGE
# qubit-engine-5fdcfcfc96-8kmnm   1/1     Running   0          5m
# qubit-engine-5fdcfcfc96-9mr9w   1/1     Running   0          5m
# qubit-engine-5fdcfcfc96-chqkn   1/1     Running   0          5m
```

### Cluster Scalability

The dashboard displays "Computed By: [pod-name]" to show which replica processed each simulation. Requests are round-robin load balanced across engines.

---

## Project Structure

```
.
├── api/
│   └── proto/               # Protocol Buffer definitions (gRPC contract)
├── backend/
│   ├── src/                 # C++ source (QuantumRegister, ServiceImpl)
│   └── include/             # Header files
├── cli/
│   └── cmd/qctl/            # Go CLI entry point
├── web/
│   ├── src/
│   │   ├── components/      # React components (BlochSphere, QuantumAudio)
│   │   └── generated/       # TypeScript protobuf clients
│   └── package.json
├── deploy/
│   ├── docker/              # Dockerfiles, docker-compose.yaml, envoy.yaml
│   └── k8s/                 # Kubernetes manifests
├── circuits/                # Example circuit JSON files
└── Makefile                 # Build automation
```

---

## Performance & Limitations

### State Vector Memory

The memory requirement scales exponentially: **16 × 2^N bytes**

| Qubits | Memory Required |
|--------|-----------------|
| 20     | ~16 MB          |
| 25     | ~512 MB         |
| 28     | ~4 GB           |
| 30     | ~16 GB          |

### Hard Limits

- **Maximum Qubits**: 30 (enforced to protect host system)
- **Thread Safety**: Uses `thread_local` RNG for concurrent measurement operations

---

## Development

### Local Build (Without Docker)

```bash
# Generate Protobuf code
make proto

# Build C++ Engine (Requires CMake & GCC 11+)
make build-cpp

# Build Go CLI
make build-go
```

### Web Development

```bash
cd web
npm install --legacy-peer-deps
npm run dev
```

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
