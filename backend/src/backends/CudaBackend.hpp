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
#include "qubit_engine_export.h"

namespace qubit_engine {

class QUBIT_ENGINE_EXPORT CudaBackend : public IQuantumBackend {
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
  void applyNoiseChannel1Q(const NoiseChannel1Q& channel, size_t target) override;
  void applyNoiseChannel2Q(const NoiseChannel2Q& channel, size_t q1, size_t q2) override;

  // --- Measurement ---
  int measure(size_t target) override;
  std::vector<double> getProbabilities() const override;
  double expectationValue(const std::string &pauli_string) const override;

  void applyDenseUnitary(const std::vector<size_t> &targets,
                         const std::vector<Complex> &matrix) override;

  // --- State Access ---
  std::vector<Complex> getStateVector() const override;
  void* getDeviceState() const { return device_state_; }
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

#ifdef ENABLE_NCCL
  void gatherStateNCCL();
#endif

  // Async telemetry stream (non-blocking D2H on secondary stream)
  void *telemetry_stream_;     // cudaStream_t (opaque for non-CUDA TUs)
  void *pinned_telemetry_buf_; // Page-locked host buffer via cudaMallocHost
  size_t pinned_buf_size_;

public:
  // Async state vector readback for gRPC telemetry
  std::vector<Complex> getStateVectorAsync() const;

  // JIT fused-block application
  void applyFusedBlock(const std::vector<int> &targets,
                       const std::vector<Complex> &unitary);
};

} // namespace qubit_engine
