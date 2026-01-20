#include "CudaBackend.hpp"
#include "../Types.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace qubit_engine {

CudaBackend::CudaBackend(size_t num_qubits)
    : num_qubits_(num_qubits), device_state_(nullptr) {
  initializeCuda();
}

CudaBackend::~CudaBackend() {
  // if (device_state_) cudaFree(device_state_);
}

void CudaBackend::initializeCuda() {
  // Stub: Check device count, allocate memory
  // cudaMalloc(&device_state_, (1ULL << num_qubits_) * sizeof(Complex));
  std::cout << "CudaBackend initialized (Stub)" << std::endl;
}

void CudaBackend::copyStateToDevice(const std::vector<Complex> &host_state) {
  // cudaMemcpy(device_state_, host_state.data(), ..., cudaMemcpyHostToDevice);
}

void CudaBackend::copyStateToHost(std::vector<Complex> &host_state) const {
  // cudaMemcpy(host_state.data(), device_state_, ..., cudaMemcpyDeviceToHost);
}

void CudaBackend::applyHadamard(size_t target) {
  // Launch Kernel
}

void CudaBackend::applyX(size_t target) {}
void CudaBackend::applyY(size_t target) {}
void CudaBackend::applyZ(size_t target) {}
void CudaBackend::applyCNOT(size_t control, size_t target) {}
void CudaBackend::applyToffoli(size_t control1, size_t control2,
                               size_t target) {}
void CudaBackend::applyPhaseS(size_t target) {}
void CudaBackend::applyPhaseT(size_t target) {}
void CudaBackend::applyRotationY(size_t target, Precision angle) {}
void CudaBackend::applyRotationZ(size_t target, Precision angle) {}
void CudaBackend::applyDepolarizingNoise(Precision probability) {}

int CudaBackend::measure(size_t target) {
  return 0; // Stub
}

std::vector<double> CudaBackend::getProbabilities() { return {}; }

double CudaBackend::expectationValue(const std::string &pauli_string) {
  return 0.0;
}

std::vector<Complex> CudaBackend::getStateVector() const {
  size_t dim = 1ULL << num_qubits_;
  std::vector<Complex> host_state(dim, {0, 0});
  // copyStateToHost(host_state);
  if (dim > 0)
    host_state[0] = {1, 0}; // Default |0> for stub
  return host_state;
}

} // namespace qubit_engine
