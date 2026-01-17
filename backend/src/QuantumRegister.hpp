#pragma once

#include "backends/IQuantumBackend.hpp"
#include <complex>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// Global Type Alias
// Global Type Alias
#include "Types.hpp"
// using Complex = std::complex<double>; // REMOVED

// Forward declaration of backend logic
namespace qubit_engine {
class IQuantumBackend;
}

class QuantumRegister {
public:
  // --- Lifecycle ---
  QuantumRegister(size_t n, bool force_local = false);
  ~QuantumRegister();

  // --- Core Gates ---
  void applyHadamard(size_t target);
  void applyX(size_t target);
  void applyY(size_t target);
  void applyZ(size_t target);
  void applyCNOT(size_t control, size_t target);

  // --- Advanced Gates ---
  // --- Advanced Gates ---
  void applyToffoli(size_t control1, size_t control2, size_t target);
  void applyPhaseS(size_t target);
  void applyPhaseT(size_t target);
  void applyRotationY(size_t target, qubit_engine::Precision angle);
  void applyRotationZ(size_t target, qubit_engine::Precision angle);

  // --- Noise Simulation ---
  void applyDepolarizingNoise(qubit_engine::Precision probability);

  // --- Measurement & Analysis ---
  int measure(size_t target);
  std::vector<double> getProbabilities();
  double expectationValue(const std::string &pauli_string);

  // --- Distributed Helpers ---
  int getRank() const;
  int getSize() const;

  // --- Debugging ---
  std::vector<qubit_engine::Complex> getStateVector() const;

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

  // Helper to Replay Tape (Optional, for adjoint)
  void applyRegisteredGate(const RecordedGate &gate);
  void applyRegisteredGateInverse(const RecordedGate &gate);

private:
  size_t num_qubits;

  // New Backend Architecture
  std::unique_ptr<qubit_engine::IQuantumBackend> backend;

  // Recorder logic remains in proxy
  bool recording_enabled = false;
  std::vector<RecordedGate> tape;
};
