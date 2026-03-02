#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../QuantumRegister.hpp"
#include "GPUContext.hpp"
#include <vector>

using qubit_engine::QuantumRegister;

class GPUQuantumRegister {
public:
  explicit GPUQuantumRegister(size_t n); // Defined in .cpp
  ~GPUQuantumRegister();

  // Gate Operations (These will call external Kernels)
  void applyHadamard(size_t target);
  void applyX(size_t target);
  void applyY(size_t target);
  void applyZ(size_t target);
  void applyCNOT(size_t control, size_t target);
  void applyToffoli(size_t control1, size_t control2, size_t target);
  void applyPhaseS(size_t target);
  void applyPhaseT(size_t target);
  void applyRotationX(size_t target, double angle);
  void applyRotationY(size_t target, double angle);
  void applyRotationZ(size_t target, double angle);
  void applySWAP(size_t qubit1, size_t qubit2);
  void applyCZ(size_t control, size_t target);

  // Data Transfer
  std::vector<std::complex<double>> getStateVector() const;

  // Helper for Adjoint / Tape Replay
  void applyRegisteredGate(const QuantumRegister::RecordedGate &gate) {
    switch (gate.type) {
    case QuantumRegister::RecordedGate::H:
      applyHadamard(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::X:
      applyX(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::Y:
      applyY(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::Z:
      applyZ(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::CNOT:
      applyCNOT(gate.qubits[0], gate.qubits[1]);
      break;
    case QuantumRegister::RecordedGate::RX:
      applyRotationX(gate.qubits[0], gate.params[0]);
      break;
    case QuantumRegister::RecordedGate::RY:
      applyRotationY(gate.qubits[0], gate.params[0]);
      break;
    case QuantumRegister::RecordedGate::RZ:
      applyRotationZ(gate.qubits[0], gate.params[0]);
      break;
    case QuantumRegister::RecordedGate::PHASE_S:
      applyPhaseS(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::PHASE_T:
      applyPhaseT(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::TOFFOLI:
      applyToffoli(gate.qubits[0], gate.qubits[1], gate.qubits[2]);
      break;
    case QuantumRegister::RecordedGate::SWAP:
      applySWAP(gate.qubits[0], gate.qubits[1]);
      break;
    case QuantumRegister::RecordedGate::CZ:
      applyCZ(gate.qubits[0], gate.qubits[1]);
      break;
    default:
      break;
    }
  }

  void applyRegisteredGateInverse(const QuantumRegister::RecordedGate &gate) {
    switch (gate.type) {
    // Self-inverse gates
    case QuantumRegister::RecordedGate::H:
      applyHadamard(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::X:
      applyX(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::Y:
      applyY(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::Z:
      applyZ(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::CNOT:
      applyCNOT(gate.qubits[0], gate.qubits[1]);
      break;
    case QuantumRegister::RecordedGate::TOFFOLI:
      applyToffoli(gate.qubits[0], gate.qubits[1], gate.qubits[2]);
      break;
    case QuantumRegister::RecordedGate::SWAP:
      applySWAP(gate.qubits[0], gate.qubits[1]);
      break;
    case QuantumRegister::RecordedGate::CZ:
      applyCZ(gate.qubits[0], gate.qubits[1]);
      break;
    // Rotation inverses: negate the angle
    case QuantumRegister::RecordedGate::RX:
      applyRotationX(gate.qubits[0], -gate.params[0]);
      break;
    case QuantumRegister::RecordedGate::RY:
      applyRotationY(gate.qubits[0], -gate.params[0]);
      break;
    case QuantumRegister::RecordedGate::RZ:
      applyRotationZ(gate.qubits[0], -gate.params[0]);
      break;
    // Phase gates: S† = S*S*S, T† = T^7 — use rotation equivalents
    case QuantumRegister::RecordedGate::PHASE_S:
      applyRotationZ(gate.qubits[0], -M_PI / 2.0); // S† = Rz(-π/2)
      break;
    case QuantumRegister::RecordedGate::PHASE_T:
      applyRotationZ(gate.qubits[0], -M_PI / 4.0); // T† = Rz(-π/4)
      break;
    default:
      break;
    }
  }

private:
  size_t num_qubits;
  void *device_state = nullptr;
};
