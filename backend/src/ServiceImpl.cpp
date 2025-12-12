#include "ServiceImpl.hpp"
#include "QuantumRegister.hpp"
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
  if (gethostname(hostname, 1024) == 0) {
    response->set_server_id(hostname);
  } else {
    response->set_server_id("unknown-host");
  }
}

grpc::Status
QubitEngineServiceImpl::RunCircuit(grpc::ServerContext *context,
                                   const qubit_engine::CircuitRequest *request,
                                   qubit_engine::StateResponse *response) {

  std::cout << "DEBUG: RunCircuit method invoked!" << std::endl;
  std::cerr << "DEBUG: RunCircuit method invoked (stderr)!" << std::endl;

  int n = request->num_qubits();

  // 1. HARD LIMIT CHECK
  if (n <= 0 || n > 30) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Qubits must be between 1 and 30");
  }

  // 2. DYNAMIC MEMORY CHECK
  if (!hasEnoughMemory(n)) {
    std::string err =
        "Insufficient Server Memory for " + std::to_string(n) + " qubits.";
    std::cerr << "ResourceExhausted: " << err << std::endl;
    return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, err);
  }

  try {
    QuantumRegister qreg(static_cast<size_t>(n));

    for (const auto &op : request->operations()) {
      applyGate(qreg, op, response);
    }

    serializeState(qreg, response);

  } catch (const std::out_of_range &e) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        std::string("Index Error: ") + e.what());
  } catch (const std::invalid_argument &e) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        std::string("Logic Error: ") + e.what());
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
