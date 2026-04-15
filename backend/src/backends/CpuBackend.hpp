#pragma once

#include "../Allocator.hpp"
#include "../Types.hpp"
#include "IQuantumBackend.hpp"
#include <array>
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
  void applyRotationX(size_t target, Precision angle) override;
  void applyRotationY(size_t target, Precision angle) override;
  void applyRotationZ(size_t target, Precision angle) override;
  void applySWAP(size_t qubit1, size_t qubit2) override;
  void applyCZ(size_t control, size_t target) override;
  void applyDenseUnitary(const std::vector<size_t> &targets,
                         const std::vector<Complex> &matrix) override;

  // --- Noise ---
  void applyDepolarizingNoise(Precision probability) override;
  void applyNoiseChannel1Q(const NoiseChannel1Q& channel, size_t target) override;
  void applyNoiseChannel2Q(const NoiseChannel2Q& channel, size_t q1, size_t q2) override;

  // --- Measurement ---
  int measure(size_t target) override;
  std::vector<double> getProbabilities() const override;
  double expectationValue(const std::string &pauli_string) const override;

  // --- State Access ---
  std::vector<Complex> getStateVector() const override;

  // --- Distributed Helpers ---
  int getRank() const override { return local_rank; }
  int getSize() const override { return world_size; }

private:
  size_t num_qubits;
  std::vector<Complex, HugePageAllocator<Complex>> state;
  int local_rank = 0;
  int world_size = 1;

  // --- Noise Selection Helpers ---
  [[nodiscard]] std::array<Complex, 4> getReducedDensityMatrix1Q(size_t target) const;
  [[nodiscard]] std::array<Complex, 16> getReducedDensityMatrix2Q(size_t q1, size_t q2) const;
};

} // namespace qubit_engine
