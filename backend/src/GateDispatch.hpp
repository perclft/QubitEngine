#pragma once

// GateDispatch.hpp — Single source of truth for mapping protobuf GateOperation
// to QuantumRegister calls.  Used by ServiceImpl and worker job execution.

#include "QuantumRegister.hpp"
#include "NoiseModel.hpp"
#include "quantum.pb.h" // Generated protobuf header for GateOperation

#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace qubit_engine {

/// Unified gate dispatch — maps a protobuf GateOperation to QuantumRegister
/// calls.
///
/// @param qreg           The quantum register to apply the gate on.
/// @param op             A protobuf GateOperation.
/// @param measurements   Optional output map for MEASURE results (worker path).
///                       Pass nullptr if you don't need it.
/// @param classical_out  Optional protobuf map to write classical results into
///                       (ServiceImpl response path).  May be nullptr.
inline void dispatchGate(
    QuantumRegister &qreg,
    const GateOperation &op,
    std::unordered_map<int32_t, bool> *measurements,
    google::protobuf::Map<uint32_t, bool> *classical_out) {

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
  case GateOperation::CNOT:
    qreg.applyCNOT(op.control_qubit(), op.target_qubit());
    break;
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
  case GateOperation::ROTATION_X:
    qreg.applyRotationX(op.target_qubit(), op.angle());
    break;
  case GateOperation::ROTATION_Y:
    qreg.applyRotationY(op.target_qubit(), op.angle());
    break;
  case GateOperation::ROTATION_Z:
    qreg.applyRotationZ(op.target_qubit(), op.angle());
    break;
  case GateOperation::SWAP:
    qreg.applySWAP(op.target_qubit(), op.second_target_qubit());
    break;
  case GateOperation::CZ:
    qreg.applyCZ(op.control_qubit(), op.target_qubit());
    break;
  case GateOperation::DEPOLARIZING_NOISE:
    qreg.applyDepolarizingNoise(op.noise_probability());
    break;
  case GateOperation::AMPLITUDE_DAMPING: {
    auto channel = qubit_engine::makeAmplitudeDampingChannel(op.noise_gamma());
    for (size_t q = 0; q < qreg.getNumQubits(); ++q) {
      qreg.applyNoiseChannel1Q(channel, q);
    }
    break;
  }
  case GateOperation::PHASE_DAMPING: {
    auto channel = qubit_engine::makePhaseDampingChannel(op.noise_gamma());
    for (size_t q = 0; q < qreg.getNumQubits(); ++q) {
      qreg.applyNoiseChannel1Q(channel, q);
    }
    break;
  }
  case GateOperation::MEASURE: {
    bool result = (qreg.measure(op.target_qubit()) != 0);
    uint32_t reg_id =
        (op.classical_register() > 0) ? op.classical_register()
                                      : static_cast<uint32_t>(op.target_qubit());
    if (measurements) {
      (*measurements)[static_cast<int32_t>(reg_id)] = result;
    }
    if (classical_out) {
      (*classical_out)[reg_id] = result;
    }
    break;
  }
  default:
    throw std::invalid_argument("Unknown Gate Type");
  }
}

} // namespace qubit_engine
