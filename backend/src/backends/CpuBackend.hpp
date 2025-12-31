#pragma once

#include "IQuantumBackend.hpp"
#include <complex>
#include <vector>

namespace qubit_engine {

class CpuBackend : public IQuantumBackend {
public:
  CpuBackend(size_t num_qubits, bool force_local = false);
  ~CpuBackend() override = default;

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
  void applyRotationY(size_t target, double angle) override;
  void applyRotationZ(size_t target, double angle) override;

  // --- Noise ---
  void applyDepolarizingNoise(double probability) override;

  // --- Measurement ---
  int measure(size_t target) override;
  std::vector<double> getProbabilities() override;
  double expectationValue(const std::string &pauli_string) override;

  // --- State Access ---
  std::vector<std::complex<double>> getStateVector() const override;

  // --- Distributed Helpers ---
  int getRank() const override { return local_rank; }
  int getSize() const override { return world_size; }

private:
  size_t num_qubits;
  std::vector<std::complex<double>> state;
  int local_rank = 0;
  int world_size = 1;
};

} // namespace qubit_engine
