#include "CudaBackend.hpp"
#include "../Types.hpp"
#include "../kernels/GateKernels.hpp"
#include <cuda_runtime.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace qubit_engine {

CudaBackend::CudaBackend(size_t num_qubits)
    : num_qubits_(num_qubits), device_state_(nullptr) {
  initializeCuda();
}

CudaBackend::~CudaBackend() {
  if (device_state_) {
    cudaFree(device_state_);
  }
}

void CudaBackend::initializeCuda() {
  size_t size = (1ULL << num_qubits_) * sizeof(Complex);
  cudaError_t err = cudaMalloc(&device_state_, size);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to allocate device memory: " +
                             std::string(cudaGetErrorString(err)));
  }
}

void CudaBackend::copyStateToDevice(const std::vector<Complex> &host_state) {
  size_t size = host_state.size() * sizeof(Complex);
  cudaError_t err =
      cudaMemcpy(device_state_, host_state.data(), size, cudaMemcpyHostToDevice);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to copy state to device: " +
                             std::string(cudaGetErrorString(err)));
  }
}

void CudaBackend::copyStateToHost(std::vector<Complex> &host_state) const {
  size_t size = host_state.size() * sizeof(Complex);
  cudaError_t err =
      cudaMemcpy(host_state.data(), device_state_, size, cudaMemcpyDeviceToHost);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to copy state to host: " +
                             std::string(cudaGetErrorString(err)));
  }
}

void CudaBackend::applyHadamard(size_t target) {
  qe::cuda::launchHadamard(device_state_, num_qubits_, target);
}

void CudaBackend::applyX(size_t target) {
  qe::cuda::launchapplyX(device_state_, num_qubits_, target);
}

void CudaBackend::applyY(size_t target) {
  qe::cuda::launchapplyY(device_state_, num_qubits_, target);
}

void CudaBackend::applyZ(size_t target) {
  qe::cuda::launchapplyZ(device_state_, num_qubits_, target);
}

void CudaBackend::applyCNOT(size_t control, size_t target) {
  // TODO: Implement CNOT kernel
  // qe::cuda::launchCNOT(device_state_, num_qubits_, control, target);
}

void CudaBackend::applyToffoli(size_t control1, size_t control2,
                               size_t target) {
  // TODO: Implement Toffoli kernel
}

void CudaBackend::applyPhaseS(size_t target) {
    // S = Rz(pi/2) -> actually Phase gate.
    // Use RotationKernel with pi/2 or implement specific kernel
    // qe::cuda::launchPhaseS(device_state_, num_qubits_, target);
}

void CudaBackend::applyPhaseT(size_t target) {}

void CudaBackend::applyRotationY(size_t target, Precision angle) {
  qe::cuda::launchRotationY(device_state_, num_qubits_, target, angle);
}

void CudaBackend::applyRotationZ(size_t target, Precision angle) {
    // qe::cuda::launchRotationZ(device_state_, num_qubits_, target, angle);
}

void CudaBackend::applyDepolarizingNoise(Precision probability) {}

int CudaBackend::measure(size_t target) {
  return 0; // Stub: Requires random number generation on device or copy back
}

std::vector<double> CudaBackend::getProbabilities() { return {}; }

double CudaBackend::expectationValue(const std::string &pauli_string) {
  return 0.0;
}

std::vector<Complex> CudaBackend::getStateVector() const {
  size_t dim = 1ULL << num_qubits_;
  std::vector<Complex> host_state(dim);
  copyStateToHost(host_state);
  return host_state;
}

} // namespace qubit_engine
