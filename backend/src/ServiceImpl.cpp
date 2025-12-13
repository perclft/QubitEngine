#include "ServiceImpl.hpp"
#include "QuantumRegister.hpp"
#include "backends/CloudBackend.hpp"
#include "backends/MockHardwareBackend.hpp"
#include "backends/SimulatorBackend.hpp"
#include <cmath>
#include <cstdint> // FIX: Added for uint32_t
#include <iostream>
#include <sys/sysinfo.h>
#include <unistd.h> // For gethostname

using grpc::ServerContext;
using grpc::Status;
using qubit_engine::CircuitRequest;
using qubit_engine::GateOperation;
using qubit_engine::StateResponse;

// HELPER: Check if the server has enough free RAM for the requested qubits
bool hasEnoughMemory(int num_qubits) {
  struct sysinfo memInfo;
  sysinfo(&memInfo);

  long long available_ram = memInfo.freeram * memInfo.mem_unit;

  // Memory needed = 2^N * sizeof(complex<double>) (16 bytes)
  // 1ULL ensures we do 64-bit arithmetic
  size_t required_elements = 1ULL << num_qubits;
  size_t required_bytes = required_elements * sizeof(std::complex<double>);

  // Add 5% buffer for OS/Process overhead
  size_t overhead = required_bytes / 20;

  return available_ram > (required_bytes + overhead);
}

// Helper to map GateOperation to QuantumRegister calls
void QubitEngineServiceImpl::applyGate(QuantumRegister &qreg,
                                       const qubit_engine::GateOperation &op,
                                       qubit_engine::StateResponse *response) {
  switch (op.type()) {
  case qubit_engine::GateOperation::HADAMARD:
    qreg.applyHadamard(op.target_qubit());
    break;
  case qubit_engine::GateOperation::PAULI_X:
    qreg.applyX(op.target_qubit());
    break;
  case qubit_engine::GateOperation::CNOT:
    qreg.applyCNOT(op.control_qubit(), op.target_qubit());
    break;
  case qubit_engine::GateOperation::MEASURE: {
    bool result = qreg.measure(op.target_qubit());
    uint32_t reg_id = (op.classical_register() > 0) ? op.classical_register()
                                                    : op.target_qubit();
    (*response->mutable_classical_results())[reg_id] = result;
    break;
  }
  // Phase 3: New Gates
  case qubit_engine::GateOperation::TOFFOLI:
    qreg.applyToffoli(op.control_qubit(), op.second_control_qubit(),
                      op.target_qubit());
    break;
  case qubit_engine::GateOperation::PHASE_S:
    qreg.applyPhaseS(op.target_qubit());
    break;
  case qubit_engine::GateOperation::PHASE_T:
    qreg.applyPhaseT(op.target_qubit());
    break;
  case qubit_engine::GateOperation::ROTATION_Y:
    qreg.applyRotationY(op.target_qubit(), op.angle());
    break;
  case qubit_engine::GateOperation::ROTATION_Z:
    qreg.applyRotationZ(op.target_qubit(), op.angle());
    break;
  default:
    throw std::invalid_argument("Unknown Gate Type");
  }
}

// Helper to serialize state vector
void QubitEngineServiceImpl::serializeState(
    const QuantumRegister &qreg, qubit_engine::StateResponse *response) {
  response->clear_state_vector();
  const auto &state = qreg.getStateVector();
  for (const auto &amp : state) {
    auto *c = response->add_state_vector();
    c->set_real(amp.real());
    c->set_imag(amp.imag());
  }

  // Populate Server ID (Pod Hostname)
  char hostname[1024];
  std::string id_str = "unknown-host";
  if (gethostname(hostname, 1024) == 0) {
    id_str = std::string(hostname);
  }

  // Phase 23: Distributed Info
  if (qreg.getSize() > 1) {
    id_str += " (MPI Rank " + std::to_string(qreg.getRank()) + "/" +
              std::to_string(qreg.getSize()) + ")";
  }

  response->set_server_id(id_str);
}

// Factory Helper
std::unique_ptr<IQuantumBackend>
createBackend(qubit_engine::CircuitRequest::ExecutionBackend type,
              int num_qubits) {
  switch (type) {
  case qubit_engine::CircuitRequest::MOCK_HARDWARE:
    std::cout << "Using Mock Hardware Backend" << std::endl;
    return std::make_unique<MockHardwareBackend>(num_qubits);
#include "backends/CloudBackend.hpp"

    // ... inside switch ...
  case qubit_engine::CircuitRequest::REAL_IBM_Q:
    std::cout << "Using Cloud Quantum Backend" << std::endl;
    return std::make_unique<CloudBackend>(num_qubits);
  case qubit_engine::CircuitRequest::SIMULATOR:
  default:
    std::cout << "Using Local Simulator Backend" << std::endl;
    return std::make_unique<SimulatorBackend>(num_qubits);
  }
}

grpc::Status
QubitEngineServiceImpl::RunCircuit(grpc::ServerContext *context,
                                   const qubit_engine::CircuitRequest *request,
                                   qubit_engine::StateResponse *response) {

  std::cout << "DEBUG: RunCircuit method invoked!" << std::endl;

  int n = request->num_qubits();

  // 1. HARD LIMIT CHECK
  if (n <= 0 || n > 30) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Qubits must be between 1 and 30");
  }

  // 2. DYNAMIC MEMORY CHECK (Only for Simulator really, but good safety)
  if (!hasEnoughMemory(n)) {
    return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED,
                        "Insufficient Server Memory for " + std::to_string(n) +
                            " qubits.");
  }

  try {
    // Instantiate Backend
    auto backend = createBackend(request->execution_backend(), n);

    // Apply Gates
    for (const auto &op : request->operations()) {
      try {
        backend->applyGate(op);
      } catch (const std::exception &e) {
        // If backend fails on a gate (e.g. out of bounds)
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e.what());
      }
    }

    // Get Result
    backend->getResult(response);

  } catch (const std::exception &e) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        std::string("Internal Engine Error: ") + e.what());
  }

  return grpc::Status::OK;
}

grpc::Status QubitEngineServiceImpl::StreamGates(
    grpc::ServerContext *context,
    grpc::ServerReaderWriter<qubit_engine::StateResponse,
                             qubit_engine::GateOperation> *stream) {

  std::cout << "DEBUG: StreamGates method invoked!" << std::endl;
  std::cerr << "DEBUG: StreamGates method invoked (stderr)!" << std::endl;

  // We need to initialize the register. But wait, how do we know 'N'?
  // Protocol Design Flaw detected and patched on the fly:
  // We expect the FIRST message to contain a special "setup" op or just assume
  // default/grow. BETTER: Client sends a "special" No-Op gate meant to init?
  // OR: We infer N from the first target qubit? No, unsafe.
  // PATCH: We'll assume the client sends a "SETUP" message or we lazy init.
  // ACTUALLY: Let's assume the first message MIGHT contain a hint.
  // BUT strictly, we'll start with a default or wait for a "Alloc" operation
  // (not defined). WORKAROUND: We'll assume N=30 (Max) effectively or N=1 and
  // grow? No vector doesn't grow. DECISION: We will initialize with 3 qubits by
  // default for this demo, OR check metadata? Let's rely on metadata
  // "num_qubits" passed in context? No, too complex. SIMPLEST: Initialize 3
  // qubits (demo size) or check valid max index seen? HARDCODED DEMO FIX: We'll
  // init 3 qubits. (Production would require a Setup message).

  int num_qubits = 3;
  QuantumRegister qreg(num_qubits);

  try {
    qubit_engine::GateOperation op;
    while (stream->Read(&op)) {
      qubit_engine::StateResponse response;

      // Check if we need to expand? (Not implemented for robustness, strict
      // size)
      if (op.target_qubit() >= num_qubits || op.control_qubit() >= num_qubits) {
        // For now, silently ignore or error effectively?
        // Let's just run applyGate, it throws if out of bounds.
      }

      applyGate(qreg, op, &response);
      serializeState(qreg, &response);

      stream->Write(response);
    }
  } catch (const std::exception &e) {
    return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
  }

  return grpc::Status::OK;
}

// -----------------------------------------------------------------
// Phase 19: VQE Implementation
// -----------------------------------------------------------------
#include "MolecularHamiltonian.hpp"
#include <random>

grpc::Status QubitEngineServiceImpl::RunVQE(
    grpc::ServerContext *context, const qubit_engine::VQERequest *request,
    grpc::ServerWriter<qubit_engine::VQEResponse> *writer) {

  std::cout << "Starting VQE Optimization..." << std::endl;

  // 1. Setup
  auto molType = (request->molecule() == qubit_engine::VQERequest::LiH)
                     ? MolecularHamiltonian::LiH
                     : MolecularHamiltonian::H2;

  int num_qubits = MolecularHamiltonian::getNumQubits(molType);
  auto hamiltonian = MolecularHamiltonian::getHamiltonian(molType);

  // Ansatz Parameters (Ry angles for H2 Hardware Efficient Ansatz)
  // Simple ansatz: Ry(theta) on all qubits + CNOT ladder + Ry(phi)
  // For H2 (2 qubits), minimal useful ansatz is Ry(0), Ry(1), CNOT(0,1), Ry(0),
  // Ry(1) Let's use 4 parameters for a 2-qubit system [Ry0, Ry1, Ry0_2, Ry1_2]
  // or even simpler: just Ry on each, entangle, Ry on each.

  std::vector<double> params(4, 0.0); // Initialize with 0
  // Random init might be better, but 0 is a valid start for Ry.

  // Hyperparameters
  double learning_rate =
      request->learning_rate() > 0 ? request->learning_rate() : 0.1;
  int max_iters = request->max_iterations();

  // SPSA (Simultaneous Perturbation Stochastic Approximation) constants
  double c = 0.05; // Perturbation size
  double gamma = 0.101;
  double alpha = 0.602;
  double A = max_iters * 0.1;
  double a = 0.2; // Gradient Step Size scaling

  for (int k = 0; k < max_iters; k++) {
    // Update SPSA decay params
    double ak = a / std::pow(k + 1 + A, alpha);
    double ck = c / std::pow(k + 1, gamma);

    // Perturbation Vector Delta (+1 or -1 Bernoulli)
    std::vector<double> delta(params.size());
    thread_local std::mt19937 gen(std::random_device{}());
    std::bernoulli_distribution dist(0.5);
    for (size_t i = 0; i < params.size(); ++i)
      delta[i] = dist(gen) ? 1.0 : -1.0;

    // Evaluate E(params + ck*delta)
    auto evalEnergy = [&](const std::vector<double> &p) -> double {
      QuantumRegister qreg(num_qubits);

      // --- ANSATZ CIRCUIT (Hardware Efficient) ---
      // Layer 1: Ry rotations
      qreg.applyRotationY(0, p[0]);
      qreg.applyRotationY(1, p[1]);

      // Entanglement
      qreg.applyCNOT(0, 1);

      // Layer 2: Ry rotations
      qreg.applyRotationY(0, p[2]);
      qreg.applyRotationY(1, p[3]);
      // -------------------------------------------

      // Measure Hamiltonian
      double energy = 0.0;
      for (const auto &term : hamiltonian) {
        energy += term.coefficient * qreg.expectationValue(term.pauli_string);
      }
      return energy;
    };

    std::vector<double> p_plus = params;
    std::vector<double> p_minus = params;
    for (size_t i = 0; i < params.size(); ++i) {
      p_plus[i] += ck * delta[i];
      p_minus[i] -= ck * delta[i];
    }

    double E_plus = evalEnergy(p_plus);
    double E_minus = evalEnergy(p_minus);

    // Gradient Estimate
    double g_est = (E_plus - E_minus) / (2.0 * ck);

    // Update Parameters
    for (size_t i = 0; i < params.size(); ++i) {
      params[i] -= ak * g_est * delta[i];
    }

    // Current Energy (at center) - Optional, expensive to re-eval, use average
    double current_energy = (E_plus + E_minus) / 2.0;

    // Stream Progress
    if (k % 5 == 0 || k == max_iters - 1) { // Throttle updates
      qubit_engine::VQEResponse resp;
      resp.set_iteration(k);
      resp.set_energy(current_energy);
      for (double p : params)
        resp.add_parameters(p);
      resp.set_converged(false);

      // Check convergence (simple threshold on ground state)
      // H2 ground state is ~ -1.137
      if (current_energy < -1.13) {
        resp.set_converged(true);
        writer->Write(resp);
        break;
      }

      writer->Write(resp);
    }
  }

  return grpc::Status::OK;
}
