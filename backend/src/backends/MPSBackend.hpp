#pragma once

#include "backends/IQuantumBackend.hpp"
#include <complex>
#include <vector>
#include "qubit_engine_export.h"

namespace qubit_engine {

// Represents a single node in a Matrix Product State (MPS)
struct QUBIT_ENGINE_EXPORT MPSTensor {
  // For a spin-1/2 chain, physical dimension is always 2.
  // virtual bond dimensions: left_dim, right_dim
  int left_dim;
  int right_dim;

  // Data stored as a flattened array: shape (left_dim, 2, right_dim)
  // Indexing: [l * 2 * right_dim + p * right_dim + r]
  std::vector<Complex> data;
};

class QUBIT_ENGINE_EXPORT MPSBackend : public IQuantumBackend {
private:
  int num_qubits;
  int max_bond_dimension;
  std::vector<MPSTensor> nodes;

  void applySingleQubitGate(size_t target, const std::vector<Complex> &matrix);
  void applyTwoQubitGate(size_t q1, size_t q2,
                         const std::vector<Complex> &matrix);

  // Truncates the bond between node_i and node_i+1 using SVD
  void contractAndTruncate(size_t left_qubit);

  // Density matrix helpers for noise models
  [[nodiscard]] std::array<Complex, 4> getReducedDensityMatrix1Q(size_t target) const;
  [[nodiscard]] std::array<Complex, 16> getReducedDensityMatrix2Q(size_t q1, size_t q2) const;

public:
  MPSBackend(int num_qubits, int max_bond_dimension = 64);
  ~MPSBackend() override = default;

  // --- Core Gates ---
  void applyHadamard(size_t target) override;
  void applyX(size_t target) override;
  void applyY(size_t target) override;
  void applyZ(size_t target) override;
  void applyCNOT(size_t control, size_t target) override;

  // --- Advanced Gates ---
  void applyToffoli(size_t control1, size_t control2, size_t target) override;
  void applyPhaseS(size_t target) override;
  void applyPhaseT(size_t target) override;
  void applyRotationX(size_t target, Precision angle) override;
  void applyRotationY(size_t target, Precision angle) override;
  void applyRotationZ(size_t target, Precision angle) override;
  void applySWAP(size_t qubit1, size_t qubit2) override;
  void applyCZ(size_t control, size_t target) override;

  // --- Noise ---
  void applyDepolarizingNoise(Precision p) override;
  void applyNoiseChannel1Q(const NoiseChannel1Q& channel, size_t target) override;
  void applyNoiseChannel2Q(const NoiseChannel2Q& channel, size_t q1, size_t q2) override;

  int measure(size_t target) override;
  std::vector<double> getProbabilities() const override;
  double expectationValue(const std::string &pauli_string) const override;
  void applyDenseUnitary(const std::vector<size_t> &targets,
                         const std::vector<Complex> &matrix) override;

  std::vector<Complex> getStateVector() const override;
  
  size_t getNumQubits() const override { return static_cast<size_t>(num_qubits); }
};

} // namespace qubit_engine
