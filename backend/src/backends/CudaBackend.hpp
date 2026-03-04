#pragma once

#include "../Types.hpp"
#include "IQuantumBackend.hpp"
#include <memory>
#include <string>
#include <vector>

// Distributed & Accelerated Comm Libraries
#ifdef MPI_ENABLED
#include <mpi.h>
#endif

#ifdef ENABLE_NCCL
#include <nccl.h>
#endif

namespace qubit_engine {

class CudaBackend : public IQuantumBackend {
public:
  CudaBackend(size_t num_qubits);
  ~CudaBackend() override;

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
  void applyDepolarizingNoise(Precision probability) override;

  // --- Measurement ---
  int measure(size_t target) override;
  std::vector<double> getProbabilities() override;
  double expectationValue(const std::string &pauli_string) override;

  // --- State Access ---
  std::vector<Complex> getStateVector() const override;

  // --- Distributed Helpers ---
  int getRank() const override { return mpi_rank_; }
  int getSize() const override { return mpi_size_; }

private:
  size_t num_qubits_;
  void *device_state_; // Pointer to GPU memory

  // Distributed Sharding
#ifdef ENABLE_NCCL
  ncclComm_t nccl_comm_ = nullptr;
#endif
  int mpi_rank_ = 0;
  int mpi_size_ = 1;

  // Internal helpers
  void initializeCuda();
  void copyStateToDevice(const std::vector<Complex> &host_state);
  void copyStateToHost(std::vector<Complex> &host_state) const;
};

} // namespace qubit_engine
