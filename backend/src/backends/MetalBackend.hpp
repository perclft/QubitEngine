#pragma once

#include "IQuantumBackend.hpp"
#include <complex>
#include <vector>

namespace qubit_engine {

class MetalBackend : public IQuantumBackend {
public:
  MetalBackend(size_t num_qubits);
  ~MetalBackend() override;

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

private:
  // Pimpl pointers to hide Objective-C types
  void *device_;
  void *commandQueue_;
  void *gpuBuffer_;
  size_t num_qubits_;
  size_t capacity_;

  // Pipelines (void* wrappers for id<MTLComputePipelineState>)
  void *hadamardPipeline_;
  void *paulixPipeline_;
  void *pauliyPipeline_;
  void *paulizPipeline_;
  void *rxPipeline_;
  void *ryPipeline_;
  void *rzPipeline_;
  void *cnotPipeline_;

  void initializeMetal();
  void buildPipelines(void *library);
  void uploadState(const std::vector<std::complex<double>> &cpuState);
  void downloadState(std::vector<std::complex<double>> &cpuState) const;
};

} // namespace qubit_engine
