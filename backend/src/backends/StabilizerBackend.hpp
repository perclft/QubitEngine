#ifndef STABILIZER_BACKEND_HPP
#define STABILIZER_BACKEND_HPP

// Clifford / Stabilizer QEC Backend
// Implements the Gottesman-Knill theorem for efficient simulation
// of thousands of qubits under Clifford operations.

#include <string>
#include <vector>

#include "../Types.hpp"
#include "IQuantumBackend.hpp"

namespace qubit_engine {

class StabilizerBackend : public IQuantumBackend {
public:
  StabilizerBackend(size_t num_qubits);
  ~StabilizerBackend() override = default;

  // Measurement and expectation values
  int measure(size_t target) override;
  std::vector<double> getProbabilities() override;
  double expectationValue(const std::string &pauli_string) override;

  // Retrieve full state vector (Warning: Exponential complexity)
  std::vector<Complex> getStateVector() const override;

  // Basic gates (Clifford)
  void applyHadamard(size_t target) override;
  void applyX(size_t target) override;
  void applyY(size_t target) override;
  void applyZ(size_t target) override;
  void applyCNOT(size_t control, size_t target) override;

  // Advanced gates (Clifford)
  void applyPhaseS(size_t target) override;
  void applyCZ(size_t control, size_t target) override;
  void applySWAP(size_t qubit1, size_t qubit2) override;

  // Non-Clifford gates (Throws exception or falls back)
  void applyToffoli(size_t control1, size_t control2, size_t target) override;
  void applyPhaseT(size_t target) override;
  void applyRotationX(size_t target, Precision angle) override;
  void applyRotationY(size_t target, Precision angle) override;
  void applyRotationZ(size_t target, Precision angle) override;

  // Noise
  void applyDepolarizingNoise(Precision probability) override;

  // ID and Device Info
  std::string getBackendType() const { return "Stabilizer"; }
  int getRank() const { return 0; }
  int getSize() const { return 1; }

private:
  size_t num_qubits_;

  // Tableau representation elements
  // 2N x (2N + 1) binary matrix representing destabilizers (top half)
  // and stabilizers (bottom half). The extra column is the sign bit.
  std::vector<std::vector<bool>> tableau_;

  // Internal helper methods for Gottesman-Knill simulation
  void updateTableauHadamard(size_t target);
  void updateTableauPhaseS(size_t target);
  void updateTableauCNOT(size_t control, size_t target);
  int rowSum(int h, int i);
};

} // namespace qubit_engine

#endif // STABILIZER_BACKEND_HPP
