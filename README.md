<p align="center">
  <img src="docs/images/logo.png" alt="QubitEngine Logo" width="120" />
</p>

<h1 align="center">QubitEngine</h1>

<p align="center">
  <strong>⚛️ A Distributed, Cloud-Native Quantum Physics Engine</strong>
</p>

<p align="center">
  <a href="#quick-start">Quick Start</a> •
  <a href="#features">Features</a> •
  <a href="#architecture">Architecture</a> •
  <a href="#domain-modules">Domain Modules</a> •
  <a href="#deployment">Deployment</a> •
  <a href="#api-reference">API Reference</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C++-20-blue?logo=cplusplus" alt="C++20" />
  <img src="https://img.shields.io/badge/Go-1.23-00ADD8?logo=go" alt="Go 1.23" />
  <img src="https://img.shields.io/badge/TypeScript-5.0-3178C6?logo=typescript" alt="TypeScript" />
  <img src="https://img.shields.io/badge/gRPC-Protocol%20Buffers-4285F4?logo=google" alt="gRPC" />
  <img src="https://img.shields.io/badge/Kubernetes-Ready-326CE5?logo=kubernetes" alt="Kubernetes" />
  <img src="https://img.shields.io/badge/License-MIT-green" alt="MIT License" />
</p>

---

## Overview

**QubitEngine** is a production-ready, distributed quantum circuit simulator designed for both research and real-world applications. It combines a high-performance C++20 physics kernel with cloud-native microservices, providing:

- **State Vector Simulation** up to 30 qubits with OpenMP parallelization
- **Real-Time 3D Visualization** with Bloch sphere, particle effects, and audio synthesis
- **Domain-Specific Modules** for physics, gaming, music, cryptography, finance, and education
- **Multi-Backend Support** for IBM Quantum, Rigetti, IonQ, and local simulation
- **Enterprise Features** including job scheduling, result caching, and multi-cluster federation

---

## 🎬 Demo

<p align="center">
  <img src="docs/videos/art_studio_demo.webp" alt="Quantum Art Studio Demo" width="800" />
</p>

<p align="center">
  <em>Quantum Art Studio — Create stunning visualizations from quantum computations</em>
</p>

---

## Quick Start

### Prerequisites

- Docker 20.10+
- Docker Compose v2
- Git

### One-Command Setup

```bash
# Clone and start all services
git clone https://github.com/perclft/QubitEngine.git
cd QubitEngine
docker compose -f deploy/docker/docker-compose.yaml up --build

# Open the dashboard
open http://localhost:5173
```

### What's Running

| Service | Port | Description |
|---------|------|-------------|
| **Web Dashboard** | 5173 | React + Three.js visualization |
| **Envoy Proxy** | 8080 | gRPC-Web gateway |
| **Quantum Engine** | 50051 | C++ state vector simulator |
| **Circuit Registry** | 50052 | PostgreSQL circuit storage |
| **Job Scheduler** | 50053 | Redis-backed job queue |
| **Result Cache** | 50054 | Redis execution cache |
| **PostgreSQL** | 5432 | Persistent storage |
| **Redis** | 6379 | Queue + cache backend |

---

## Features

### 🌀 Quantum Simulation Engine

The core engine provides accurate state vector simulation with the following capabilities:

| Gate | Description | Matrix |
|------|-------------|--------|
| **H** | Hadamard | Creates superposition |
| **X, Y, Z** | Pauli gates | Bit/phase flips |
| **CNOT** | Controlled-NOT | Two-qubit entanglement |
| **CZ** | Controlled-Z | Phase entanglement |
| **SWAP** | Swap | Exchange qubit states |
| **RX, RY, RZ** | Rotation gates | Parametric rotations |
| **S, T** | Phase gates | π/2 and π/4 phase |
| **Toffoli** | CCX | Three-qubit gate |

**Performance:**
- Memory: 16 × 2^N bytes (e.g., 30 qubits = 16 GB)
- Parallelization: OpenMP SIMD across all CPU cores
- Throughput: ~10M gate operations/second (depends on qubit count)

### 🎨 Real-Time Visualization

The web dashboard provides immersive quantum state visualization:

- **3D Bloch Sphere** with neon trails and phase-based HSL coloring
- **Generative Particle Field** responding to quantum amplitude bursts
- **Probability Histograms** for multi-qubit measurement outcomes
- **Visual Overdrive Mode** with Bloom, Chromatic Aberration, Noise, and Vignette

### 🔊 Quantum Audio Synthesis

Sonify quantum states in real-time:

- **Amplitude → Volume**: Higher probability = louder tone
- **Phase → Pitch**: Phase rotation shifts frequency (440Hz ± 500Hz)
- **Waveform**: Sine oscillator with smooth envelope
- **Spatial Audio**: Panning based on qubit index

### 📡 Depolarizing Noise Model

Simulate realistic quantum decoherence:

- Configurable error rate: 0.0 (pure) to 0.5 (maximum)
- Random Pauli errors applied after each gate
- Visualize decoherence on the Bloch sphere

---

## Architecture

```
┌────────────────────────────────────────────────────────────────────────────┐
│                           QubitEngine Platform                              │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────┐    ┌─────────────┐    ┌─────────────┐    ┌────────────┐ │
│  │ React + Three │    │   Envoy     │    │  Quantum    │    │ Hardware   │ │
│  │   Dashboard   │◄──►│   Proxy     │◄──►│   Engine    │◄──►│  Backends  │ │
│  └───────────────┘    └─────────────┘    └─────────────┘    └────────────┘ │
│        :5173              :8080              :50051         IBM/Rigetti/IonQ│
│                                                                             │
│  ┌───────────────┐    ┌─────────────┐    ┌─────────────┐    ┌────────────┐ │
│  │   Circuit     │    │    Job      │    │   Result    │    │ Domain     │ │
│  │   Registry    │    │  Scheduler  │    │   Cache     │    │ Modules    │ │
│  └───────────────┘    └─────────────┘    └─────────────┘    └────────────┘ │
│        :50052             :50053             :50054        VQE/RNG/Music/... │
│           │                  │                  │                           │
│           ▼                  ▼                  ▼                           │
│  ┌───────────────────────────────────────────────────────────────────────┐ │
│  │                    PostgreSQL      │        Redis                      │ │
│  │                      :5432         │        :6379                      │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
│                                                                             │
└────────────────────────────────────────────────────────────────────────────┘
```

### Core Services

| Service | Language | Purpose |
|---------|----------|---------|
| **Quantum Engine** | C++20 | State vector simulation, gate operations, measurement |
| **Circuit Registry** | Go | CRUD for circuits, versioning, forking, analytics |
| **Job Scheduler** | Go | Priority queue, async execution, job cancellation |
| **Result Cache** | Go | TTL-based caching, hit statistics, invalidation |
| **Envoy Proxy** | YAML | gRPC-Web bridge, load balancing, rate limiting |
| **Web Dashboard** | TypeScript | React + Three.js visualization |

---

## Domain Modules

QubitEngine includes specialized modules for real-world applications:

### ⚛️ Physics: VQE Quantum Chemistry

```protobuf
service VQESolver {
    rpc FindGroundState(VQERequest) returns (stream VQEIteration);
    rpc GetMoleculeLibrary(Empty) returns (MoleculeLibrary);
    rpc BuildHamiltonian(MoleculeConfig) returns (Hamiltonian);
}
```

**Features:**
- Molecular Hamiltonian construction (Jordan-Wigner transform)
- Pre-built molecule library: H2, HeH+, LiH
- Ansatz options: UCCSD, Hardware-Efficient, RY layers
- Optimizers: COBYLA, SPSA, Adam, Gradient Descent

### 🎮 Gaming: Quantum RNG

```protobuf
service QuantumGaming {
    rpc GenerateRandom(RandomRequest) returns (RandomResponse);
    rpc QuantumCoinFlip(CoinFlipRequest) returns (CoinFlipResult);
    rpc QuantumDiceRoll(DiceRequest) returns (DiceResult);
    rpc ShuffleDeck(ShuffleRequest) returns (ShuffledDeck);
    rpc CreateSuperposition(SuperpositionRequest) returns (SuperpositionState);
}
```

**Features:**
- True quantum random number generation
- Superposition-based game mechanics (Schrödinger's loot box)
- Verifiable deck shuffling with proof hashes
- Configurable bias for fair/unfair coins

### 🎵 Music: Quantum Composer

```protobuf
service QuantumComposer {
    rpc GenerateMelody(MelodyRequest) returns (Melody);
    rpc GenerateChordProgression(ChordRequest) returns (ChordProgression);
    rpc ComposeTrack(CompositionRequest) returns (stream CompositionEvent);
    rpc ExportMIDI(ExportRequest) returns (MIDIFile);
}
```

**Features:**
- Scale-aware melody generation (Major, Minor, Pentatonic, Blues, Dorian)
- Chord progressions (I-V-vi-IV, I-IV-V-V, etc.)
- MIDI export for DAW integration
- Mood-based composition (Happy, Sad, Mysterious, Epic)

### 🔐 Crypto: BB84 QKD Protocol

```protobuf
service QuantumCrypto {
    rpc StartBB84Alice(BB84AliceRequest) returns (BB84AliceState);
    rpc StartBB84Bob(BB84BobRequest) returns (BB84BobState);
    rpc ReconcileBB84(ReconcileRequest) returns (BB84Key);
    rpc DetectEavesdropping(EavesdropRequest) returns (EavesdropResult);
}
```

**Features:**
- Full BB84 quantum key distribution simulation
- Basis reconciliation and key sifting
- Eavesdropper detection via error rate analysis
- One-time pad encryption with quantum keys

### 💰 Finance: Monte Carlo Pricing

```protobuf
service QuantumFinance {
    rpc PriceEuropeanOption(OptionRequest) returns (OptionPrice);
    rpc OptimizePortfolio(PortfolioRequest) returns (OptimalPortfolio);
    rpc CalculateVaR(VaRRequest) returns (VaRResult);
    rpc SimulatePricePaths(SimulationRequest) returns (stream PricePath);
}
```

**Features:**
- European/American option pricing
- Greeks calculation (Delta, Gamma, Theta, Vega, Rho)
- Black-Scholes comparison
- Value at Risk (VaR) and Conditional VaR
- Portfolio optimization with Sharpe ratio

### 📚 Education: Interactive Learning

```protobuf
service QuantumEducation {
    rpc GetLesson(LessonRequest) returns (Lesson);
    rpc StartQuiz(QuizRequest) returns (Quiz);
    rpc GetCircuit(CircuitRequest) returns (LibraryCircuit);
    rpc SimulateCircuit(SimulateRequest) returns (SimulationResult);
}
```

**Features:**
- Structured lessons on superposition, entanglement, algorithms
- Interactive quizzes with explanations
- Circuit library: Bell state, GHZ state, QFT, Grover
- Step-by-step simulation with state evolution

---

## Deployment

### Docker Compose (Development)

```bash
# Start all services
docker compose -f deploy/docker/docker-compose.yaml up --build

# Scale the engine
docker compose -f deploy/docker/docker-compose.yaml up --scale engine=3
```

### Kubernetes (Production)

```bash
# Create namespace
kubectl apply -f deploy/k8s/namespace.yaml

# Deploy core services
kubectl apply -f deploy/k8s/engine-deployment.yaml
kubectl apply -f deploy/k8s/registry-deployment.yaml
kubectl apply -f deploy/k8s/scheduler-deployment.yaml

# Deploy observability stack
kubectl apply -f deploy/k8s/prometheus.yaml
kubectl apply -f deploy/k8s/grafana.yaml
kubectl apply -f deploy/k8s/jaeger.yaml

# Enable autoscaling
kubectl apply -f deploy/k8s/autoscaling.yaml

# Configure API Gateway
kubectl apply -f deploy/k8s/api-gateway-ingress.yaml
```

### Kubernetes Manifests

| Manifest | Description |
|----------|-------------|
| `engine-deployment.yaml` | Quantum Engine pods (CPU-optimized) |
| `registry-deployment.yaml` | Circuit Registry + PostgreSQL |
| `scheduler-deployment.yaml` | Job Scheduler + Redis |
| `autoscaling.yaml` | HPAs with custom metrics |
| `prometheus.yaml` | Metrics collection + alerting |
| `grafana.yaml` | Pre-built dashboards |
| `jaeger.yaml` | Distributed tracing |
| `multi-cluster.yaml` | Federation across regions |

### Multi-Cluster Federation

QubitEngine supports spanning multiple Kubernetes clusters:

```yaml
clusters:
  - name: us-west-1 (primary)
  - name: us-east-1 (secondary)
  - name: eu-central-1 (secondary)
  - name: ap-south-1 (secondary)
```

---

## API Reference

### gRPC Services

| Service | Package | Port |
|---------|---------|------|
| QuantumEngine | `qubit_engine` | 50051 |
| CircuitRegistry | `qubit_engine.registry` | 50052 |
| QuantumScheduler | `qubit_engine.scheduler` | 50053 |
| ResultCache | `qubit_engine.cache` | 50054 |

### Core RPCs

```protobuf
// Quantum Engine
rpc InitializeRegister(RegisterConfig) returns (RegisterState);
rpc ApplyGate(GateOperation) returns (StateResponse);
rpc Measure(MeasurementRequest) returns (MeasurementResult);
rpc StreamState(StreamRequest) returns (stream StateResponse);
rpc VisualizeCircuit(CircuitRequest) returns (CircuitVisualization);

// Circuit Registry
rpc SaveCircuit(SaveCircuitRequest) returns (CircuitHandle);
rpc LoadCircuit(CircuitHandle) returns (Circuit);
rpc ListCircuits(ListCircuitsRequest) returns (CircuitList);
rpc ForkCircuit(ForkRequest) returns (CircuitHandle);

// Job Scheduler
rpc SubmitJob(JobRequest) returns (JobHandle);
rpc GetJobStatus(JobHandle) returns (JobStatus);
rpc CancelJob(JobHandle) returns (CancelResponse);
rpc StreamJobResults(JobHandle) returns (stream JobResult);
```

### REST API (via Envoy)

```bash
# Initialize a 2-qubit register
curl -X POST http://localhost:8080/v1/register \
  -H "Content-Type: application/json" \
  -d '{"num_qubits": 2}'

# Apply Hadamard gate
curl -X POST http://localhost:8080/v1/gate \
  -H "Content-Type: application/json" \
  -d '{"gate": "HADAMARD", "target_qubit": 0}'
```

---

## Advanced Features

### JIT Gate Fusion

QubitEngine includes a quantum JIT compiler that optimizes circuits:

```cpp
// Optimization levels
O0: No optimization
O1: Adjacent gate cancellation (X·X = I)
O2: Single-qubit gate fusion
O3: Aggressive reordering + multi-qubit fusion
```

### OpenQASM Support

Import and export circuits in OpenQASM 2.0/3.0 format:

```qasm
OPENQASM 3.0;
include "stdgates.inc";

qubit[2] q;
bit[2] c;

h q[0];
cx q[0], q[1];
c = measure q;
```

### Hardware Backends

Connect to real quantum hardware:

| Provider | Backend | Max Qubits |
|----------|---------|------------|
| IBM Quantum | ibmq_manila, ibm_osaka | 127 |
| Rigetti | Aspen-M-3 | 80 |
| IonQ | Harmony, Aria-1 | 32 |
| Local | QubitEngine Simulator | 30 |

---

## Observability

### Prometheus Metrics

QubitEngine exports the following metrics:

| Metric | Type | Description |
|--------|------|-------------|
| `quantum_job_queue_depth` | Gauge | Pending jobs in queue |
| `quantum_cache_hit_rate` | Gauge | Cache efficiency |
| `grpc_server_handled_total` | Counter | RPC call counts |
| `grpc_server_handling_seconds` | Histogram | RPC latency |

### Grafana Dashboards

Pre-built dashboards include:
- **Quantum Engine Overview**: RPC rates, latency percentiles, error rates
- **Job Queue Metrics**: Queue depth, processing time, worker utilization
- **Cache Performance**: Hit/miss ratio, memory usage, evictions

### Distributed Tracing

Jaeger provides end-to-end tracing:
- Request flow across services
- Latency breakdown by component
- Error correlation and root cause analysis

---

## Project Structure

```
QubitEngine/
├── api/proto/                    # Protocol Buffer definitions
│   ├── quantum.proto             # Core quantum operations
│   ├── registry.proto            # Circuit storage
│   ├── scheduler.proto           # Job queue
│   ├── cache.proto               # Result caching
│   ├── physics/vqe.proto         # VQE chemistry
│   ├── gaming/gaming.proto       # Quantum RNG
│   ├── music/music.proto         # Music generation
│   ├── crypto/crypto.proto       # BB84 QKD
│   ├── finance/finance.proto     # Option pricing
│   └── education/education.proto # Learning platform
│
├── backend/
│   ├── src/                      # C++ quantum engine
│   │   ├── QuantumRegister.cpp   # State vector operations
│   │   ├── ServiceImpl.cpp       # gRPC server
│   │   ├── QuantumJIT.hpp        # Gate fusion optimizer
│   │   └── OpenQASM.hpp          # QASM parser/exporter
│   └── backends/                 # Hardware abstractions
│       └── backends.go           # IBM/Rigetti/IonQ clients
│
├── services/
│   ├── registry/                 # Circuit registry (Go)
│   ├── scheduler/                # Job scheduler (Go)
│   └── cache/                    # Result cache (Go)
│
├── modules/
│   ├── physics/                  # VQE solver
│   ├── gaming/                   # Quantum RNG
│   ├── music/                    # Music composer
│   ├── crypto/                   # BB84 QKD
│   ├── finance/                  # Monte Carlo
│   └── education/                # Learning platform
│
├── web/
│   ├── src/
│   │   ├── components/           # React components
│   │   │   ├── BlochSphere.tsx   # 3D visualization
│   │   │   ├── QuantumAudio.tsx  # Audio synthesis
│   │   │   └── GenerativeCanvas/ # Particle effects
│   │   └── gpu/
│   │       └── WebGPURenderer.ts # GPU acceleration
│   └── package.json
│
├── deploy/
│   ├── docker/
│   │   ├── docker-compose.yaml   # Local development
│   │   ├── Dockerfile.engine     # C++ build
│   │   └── Dockerfile.web        # React build
│   └── k8s/
│       ├── engine-deployment.yaml
│       ├── autoscaling.yaml      # HPAs
│       ├── prometheus.yaml       # Monitoring
│       ├── grafana.yaml          # Dashboards
│       ├── jaeger.yaml           # Tracing
│       └── multi-cluster.yaml    # Federation
│
├── cli/cmd/qctl/                 # Go CLI
├── circuits/                     # Example circuits
└── Makefile
```

---

## Performance

### State Vector Memory Requirements

| Qubits | Memory | Simulation Time (1000 gates) |
|--------|--------|------------------------------|
| 10 | 16 KB | ~1 ms |
| 15 | 512 KB | ~10 ms |
| 20 | 16 MB | ~100 ms |
| 25 | 512 MB | ~3 s |
| 28 | 4 GB | ~30 s |
| 30 | 16 GB | ~2 min |

### Cluster Scalability

With Kubernetes HPA, QubitEngine scales automatically:
- **Min replicas**: 2 (for high availability)
- **Max replicas**: 20 (configurable)
- **Scale triggers**: CPU > 70%, queue depth > 10 jobs

---

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup

```bash
# Generate protobuf code
make proto

# Build C++ engine
make build-cpp

# Build Go services
make build-go

# Run tests
make test
```

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

<p align="center">
  <strong>Built with ❤️ by the QubitEngine Team</strong>
</p>

<p align="center">
  <a href="https://github.com/perclft/QubitEngine">GitHub</a> •
  <a href="https://qubitengine.io/docs">Documentation</a> •
  <a href="https://discord.gg/qubitengine">Discord</a>
</p>
