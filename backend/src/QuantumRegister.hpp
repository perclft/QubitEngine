#pragma once

#include <complex>
#include <cstddef>
#include <string>
#include <vector>

// --- Fix 1: Global Type Alias (Crucial for ServiceImpl_Visualize) ---
using Complex = std::complex<double>;

class QuantumRegister {
public:
  // --- Lifecycle ---
  QuantumRegister(size_t n);
  ~QuantumRegister();

  // --- Core Gates ---
  void applyHadamard(size_t target);
  void applyX(size_t target);
  void applyY(size_t target);
  void applyZ(size_t target);
  void applyCNOT(size_t control, size_t target);

  // --- Advanced Gates ---
  void applyToffoli(size_t control1, size_t control2, size_t target);
  void applyPhaseS(size_t target);
  void applyPhaseT(size_t target);
  void applyRotationY(size_t target, double angle);
  void applyRotationZ(size_t target, double angle);

  // --- Fix 2: Noise Simulation (Restored) ---
  void applyDepolarizingNoise(double probability);

  // --- Measurement & Analysis ---
  int measure(size_t target);
  std::vector<double> getProbabilities();
  double expectationValue(const std::string &pauli_string);

  // --- Distributed Helpers ---
  int getRank() const { return local_rank; }
  int getSize() const { return world_size; }

  // --- Debugging ---
  std::vector<Complex> getStateVector() const;

  // --- Recording / Tape Helper ---
  struct RecordedGate {
    enum Type {
      H,
      X,
      Y,
      Z,
      CNOT,
      RX,
      RY,
      RZ,
      PHASE_S,
      PHASE_T,
      TOFFOLI,
      MEASURE
    };
    Type type;
    std::vector<size_t> qubits; // [target] or [control, target]
    std::vector<double> params; // [angle]
  };

  void enableRecording(bool enable);
  void clearTape();
  const std::vector<RecordedGate> &getTape() const;

  // Manual Gate Application (for Adjoint replay)
  void applyRegisteredGate(const RecordedGate &gate) {
    switch (gate.type) {
    case RecordedGate::H:
      applyHadamard(gate.qubits[0]);
      break;
    case RecordedGate::X:
      applyX(gate.qubits[0]);
      break;
    case RecordedGate::Y:
      applyY(gate.qubits[0]);
      break;
    case RecordedGate::Z:
      applyZ(gate.qubits[0]);
      break;
    case RecordedGate::CNOT:
      applyCNOT(gate.qubits[0], gate.qubits[1]);
      break;
    case RecordedGate::RY:
      applyRotationY(gate.qubits[0], gate.params[0]);
      break;
    case RecordedGate::RZ:
      applyRotationZ(gate.qubits[0], gate.params[0]);
      break;
    default:
      break;
    }
  }

  void applyRegisteredGateInverse(const RecordedGate &gate) {
    switch (gate.type) {
    case RecordedGate::H:
      applyHadamard(gate.qubits[0]);
      break; // Self-inverse
    case RecordedGate::X:
      applyX(gate.qubits[0]);
      break; // Self-inverse
    case RecordedGate::Y:
      applyY(gate.qubits[0]);
      break; // Self-inverse
    case RecordedGate::Z:
      applyZ(gate.qubits[0]);
      break; // Self-inverse
    case RecordedGate::CNOT:
      applyCNOT(gate.qubits[0], gate.qubits[1]);
      break; // Self-inverse
    case RecordedGate::RY:
      applyRotationY(gate.qubits[0], -gate.params[0]);
      break;
    case RecordedGate::RZ:
      applyRotationZ(gate.qubits[0], -gate.params[0]);
      break;
    default:
      break;
    }
  }

private:
  size_t num_qubits;
  std::vector<Complex> state;

  // Recorder
  bool recording_enabled = false;
  std::vector<RecordedGate> tape;

  // Distributed State
  int local_rank;
  int world_size;
};
