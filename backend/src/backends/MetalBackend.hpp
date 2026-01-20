#pragma once

#include "../Types.hpp"
#include "IQuantumBackend.hpp"
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
  void applyRotationY(size_t target, Precision angle) override;
  void applyRotationZ(size_t target, Precision angle) override;

  // --- Noise ---
  void applyDepolarizingNoise(Precision probability) override;

  // --- Measurement ---
  int measure(size_t target) override;
  std::vector<double> getProbabilities() override;
  double expectationValue(const std::string &pauli_string) override;

  // --- State Access ---
  std::vector<Complex> getStateVector() const override;

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
  void *phaseSPipeline_;
  void *phaseTPipeline_;
  void *cnotPipeline_;
  void *toffoliPipeline_;

  void initializeMetal();
  void buildPipelines(void *library);
  void initializeBuffer(const std::vector<Complex> &initialState);
  void uploadState(const std::vector<Complex> &cpuState);
  void downloadState(std::vector<Complex> &cpuState) const;
  void dispatchHelper(void *queue, void *pipeline, void *buffer, size_t dim,
                      std::vector<void *> args, std::vector<size_t> sizes);
};

} // namespace qubit_engine
