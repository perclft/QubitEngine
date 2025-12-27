#pragma once

#include "../QuantumRegister.hpp"
#include "GPUContext.hpp"
#include <complex>
#include <vector>

class GPUQuantumRegister {
public:
  explicit GPUQuantumRegister(size_t n); // Defined in .cpp
  ~GPUQuantumRegister();

  // Gate Operations (These will call external Kernels)
  void applyHadamard(size_t target);
  void applyX(size_t target);
  void applyY(size_t target);
  void applyZ(size_t target);
  void applyRotationY(size_t target, double angle);
  // Missing Z rotation for completeness
  void applyRotationZ(size_t target, double angle) {} // Placeholder

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
    // case QuantumRegister::RecordedGate::CNOT: ...
    case QuantumRegister::RecordedGate::RY:
      applyRotationY(gate.qubits[0], gate.params[0]);
      break;
    // case QuantumRegister::RecordedGate::RZ: ...
    default:
      break; // Ignore others
    }
  }

  void applyRegisteredGateInverse(const QuantumRegister::RecordedGate &gate) {
    switch (gate.type) {
    case QuantumRegister::RecordedGate::H:
      applyHadamard(gate.qubits[0]);
      break; // Self-inverse
    case QuantumRegister::RecordedGate::X:
      applyX(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::Y:
      applyY(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::Z:
      applyZ(gate.qubits[0]);
      break;
    case QuantumRegister::RecordedGate::RY:
      applyRotationY(gate.qubits[0], -gate.params[0]);
      break;
    default:
      break;
    }
  }

private:
  size_t num_qubits;
  void *device_state = nullptr;
};
