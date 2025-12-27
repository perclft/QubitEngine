#pragma once
#include "GPUQuantumRegister.hpp"
#include "IQuantumBackend.hpp"
#include <unistd.h>

class GPUBackend : public IQuantumBackend {
private:
  GPUQuantumRegister qreg;

public:
  explicit GPUBackend(int num_qubits) : qreg(num_qubits) {}

  void applyGate(const qubit_engine::GateOperation &op) override {
    using qubit_engine::GateOperation;
    switch (op.type()) {
    case GateOperation::HADAMARD:
      qreg.applyHadamard(op.target_qubit());
      break;
    case GateOperation::PAULI_X:
      qreg.applyX(op.target_qubit());
      break;
    case GateOperation::PAULI_Y:
      qreg.applyY(op.target_qubit());
      break;
    case GateOperation::PAULI_Z:
      qreg.applyZ(op.target_qubit());
      break;
    case GateOperation::ROTATION_Y:
      qreg.applyRotationY(op.target_qubit(), op.angle());
      break;
    // Missing gates (CNOT, etc) will just be ignored or throw for now in MVP
    default:
      // std::cerr << "Warning: Gate not implemented on GPU: " << op.type() <<
      // std::endl;
      break;
    }
  }

  void getResult(qubit_engine::StateResponse *response) override {
    response->clear_state_vector();
    // Copy from GPU to Host
    std::vector<std::complex<double>> state = qreg.getStateVector();

    for (const auto &amp : state) {
      auto *c = response->add_state_vector();
      c->set_real(amp.real());
      c->set_imag(amp.imag());
    }

    char hostname[1024];
    if (gethostname(hostname, 1024) == 0) {
      response->set_server_id(std::string(hostname) + " (GPU)");
    } else {
      response->set_server_id("unknown (GPU)");
    }
  }
};
