#pragma once
#include "IQuantumBackend.hpp"
#include "Types.hpp"
#include <vector>
#include <string>

namespace qubit_engine {

class MetalBackend : public IQuantumBackend {
public:
  explicit MetalBackend(size_t num_qubits);
  ~MetalBackend() override;

  void applyHadamard(size_t target) override;
  void applyX(size_t target) override;
  void applyY(size_t target) override;
  void applyZ(size_t target) override;
  void applyCNOT(size_t control, size_t target) override;
  void applyToffoli(size_t control1, size_t control2, size_t target) override;
  void applyPhaseS(size_t target) override;
  void applyPhaseT(size_t target) override;
  void applyRotationX(size_t target, Precision angle) override;
  void applyRotationY(size_t target, Precision angle) override;
  void applyRotationZ(size_t target, Precision angle) override;
  void applySWAP(size_t qubit1, size_t qubit2) override;
  void applyCZ(size_t control, size_t target) override;
  void applyDepolarizingNoise(Precision p) override;

  void applyDenseUnitary(const std::vector<size_t> &targets,
                         const std::vector<Complex> &matrix) override;

  int measure(size_t target) override;
  std::vector<double> getProbabilities() const override;
  double expectationValue(const std::string &pauli) const override;

  int getRank() const override { return 0; }
  int getSize() const override { return 1; }
  std::vector<Complex> getStateVector() const override;

private:
  void initializeMetal();
  void buildPipelines(void *library);
  void initializeBuffer(const std::vector<Complex> &initialState);
  void uploadState(const std::vector<Complex> &cpuState);
  void downloadState(std::vector<Complex> &cpuState) const;
  void dispatchHelper(void *queue, void *pso, void *buffer, size_t dim,
                      std::vector<void *> args, std::vector<size_t> sizes);

  size_t num_qubits_;
  size_t capacity_;

  void *device_;
  void *commandQueue_;
  void *gpuBuffer_;
  void *lastCommandBuffer_ = nullptr;

  void *hadamardPipeline_ = nullptr;
  void *paulixPipeline_ = nullptr;
  void *pauliyPipeline_ = nullptr;
  void *paulizPipeline_ = nullptr;
  void *rxPipeline_ = nullptr;
  void *ryPipeline_ = nullptr;
  void *rzPipeline_ = nullptr;
  void *phaseSPipeline_ = nullptr;
  void *phaseTPipeline_ = nullptr;
  void *cnotPipeline_ = nullptr;
  void *toffoliPipeline_ = nullptr;

  void *measureProb0Pipeline_ = nullptr;
  void *projectStatePipeline_ = nullptr;
  void *expectationZPipeline_ = nullptr;
  void *computeProbabilitiesPipeline_ = nullptr;
  void *diagonalExpectationPipeline_ = nullptr;
  void *denseUnitary1qPipeline_ = nullptr;
  void *denseUnitary2qPipeline_ = nullptr;
};

} // namespace qubit_engine
