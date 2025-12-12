#pragma once
#include "../QuantumRegister.hpp"
#include "IQuantumBackend.hpp"
#include <unistd.h> // for gethostname

class SimulatorBackend : public IQuantumBackend {
private:
  QuantumRegister qreg;

public:
  explicit SimulatorBackend(int num_qubits) : qreg(num_qubits) {}

  void applyGate(const qubit_engine::GateOperation &op) override {
    // Reuse the logic from ServiceImpl, or better, move that logic to a shared
    // helper? Since QuantumRegister has specific methods, we map op -> method
    // here. For simplicity, let's copy the switch-case logic or include a
    // helper. Actually, cleaner to define the mapping here.

    using qubit_engine::GateOperation;
    switch (op.type()) {
    case GateOperation::HADAMARD:
      qreg.applyHadamard(op.target_qubit());
      break;
    case GateOperation::PAULI_X:
      qreg.applyX(op.target_qubit());
      break;
    case GateOperation::CNOT:
      qreg.applyCNOT(op.control_qubit(), op.target_qubit());
      break;
    case GateOperation::MEASURE:
      qreg.measure(op.target_qubit());
      break; // We ignore result storage for now in this simple wrapper? No, we
             // need it.
    // Wait, measure returns bool. The interface ApplyGate is void.
    // We should probably handle measurement results.
    // For now, let's just run the side-effect.
    // Real hardware usually returns counts at the END.
    // Simulator allows mid-circuit measurement.

    // New Gates
    case GateOperation::TOFFOLI:
      qreg.applyToffoli(op.control_qubit(), op.second_control_qubit(),
                        op.target_qubit());
      break;
    case GateOperation::PHASE_S:
      qreg.applyPhaseS(op.target_qubit());
      break;
    case GateOperation::PHASE_T:
      qreg.applyPhaseT(op.target_qubit());
      break;
    case GateOperation::ROTATION_Y:
      qreg.applyRotationY(op.target_qubit(), op.angle());
      break;
    case GateOperation::ROTATION_Z:
      qreg.applyRotationZ(op.target_qubit(), op.angle());
      break;
    default:
      break;
    }
  }

  void getResult(qubit_engine::StateResponse *response) override {
    response->clear_state_vector();
    const auto &state = qreg.getStateVector();
    for (const auto &amp : state) {
      auto *c = response->add_state_vector();
      c->set_real(amp.real());
      c->set_imag(amp.imag());
    }

    char hostname[1024];
    if (gethostname(hostname, 1024) == 0) {
      response->set_server_id(std::string(hostname) + " (Simulator)");
    } else {
      response->set_server_id("unknown (Simulator)");
    }
  }
};
