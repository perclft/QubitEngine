#pragma once

#include "backends/IQuantumBackend.hpp"
#include <complex>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Types.hpp"

namespace qubit_engine {

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
  void applyToffoli(size_t control1, size_t control2, size_t target);
  void applyPhaseS(size_t target);
  void applyPhaseT(size_t target);
  void applyRotationX(size_t target, Precision angle);
  void applyRotationY(size_t target, Precision angle);
  void applyRotationZ(size_t target, Precision angle);
  void applySWAP(size_t qubit1, size_t qubit2);
  void applyCZ(size_t control, size_t target);

  // --- Noise Simulation ---
  void applyDepolarizingNoise(Precision probability);

  // --- Measurement & Analysis ---
  int measure(size_t target);
  std::vector<double> getProbabilities();
  double expectationValue(const std::string &pauli_string);

  // --- Distributed Helpers ---
  int getRank() const;
  int getSize() const;
  size_t getNumQubits() const { return num_qubits; }

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
      MEASURE,
      SWAP,
      CZ
    };
    Type type;
    std::vector<size_t> qubits; // [target] or [control, target]
    std::vector<double> params; // [angle]
  };

  void enableRecording(bool enable);
  void enableExecution(bool enable);
  void clearTape();
  const std::vector<RecordedGate> &getTape() const;

  void optimize();        // Optimizes the current tape
  void mapTo1DTopology(); // Routes tape for MPS

  // Helper to Replay Tape (Optional, for adjoint)
  void applyRegisteredGate(const RecordedGate &gate);
  void applyRegisteredGateInverse(const RecordedGate &gate);

private:
  void validateQubit(size_t q) const;
  size_t num_qubits;
  std::unique_ptr<IQuantumBackend> backend;
  bool recording_enabled = false;
  bool execution_enabled = true;
  std::vector<RecordedGate> tape;
};

} // namespace qubit_engine
