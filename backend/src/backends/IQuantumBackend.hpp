#pragma once

#include "Types.hpp"
#include <complex>
#include <cstddef>
#include <string>
#include <vector>

namespace qubit_engine {

class IQuantumBackend {
public:
  virtual ~IQuantumBackend() = default;

  // --- Core Gates ---
  virtual void applyHadamard(size_t target) = 0;
  virtual void applyX(size_t target) = 0;
  virtual void applyY(size_t target) = 0;
  virtual void applyZ(size_t target) = 0;
  virtual void applyCNOT(size_t control, size_t target) = 0;

  // --- Advanced Gates ---
  virtual void applyToffoli(size_t control1, size_t control2,
                            size_t target) = 0;
  virtual void applyPhaseS(size_t target) = 0;
  virtual void applyPhaseT(size_t target) = 0;
  virtual void applyRotationX(size_t target, Precision angle) = 0;
  virtual void applyRotationY(size_t target, Precision angle) = 0;
  virtual void applyRotationZ(size_t target, Precision angle) = 0;
  virtual void applySWAP(size_t qubit1, size_t qubit2) = 0;
  virtual void applyCZ(size_t control, size_t target) = 0;

  // --- Noise ---
  virtual void applyDepolarizingNoise(Precision probability) = 0;

  // --- Measurement & Analysis ---
  virtual int measure(size_t target) = 0;
  virtual std::vector<double>
  getProbabilities() = 0; // Return double for probs usually fine
  virtual double expectationValue(const std::string &pauli_string) = 0;

  // --- State Access ---
  virtual std::vector<Complex> getStateVector() const = 0;

  // --- Distributed Helpers (Optional / Backend Specific) ---
  virtual int getRank() const { return 0; }
  virtual int getSize() const { return 1; }
};

} // namespace qubit_engine
