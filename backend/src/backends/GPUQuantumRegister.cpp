#include "GPUQuantumRegister.hpp"
#include "../kernels/GateKernels.hpp"

#ifdef ENABLE_CUDA
// If using CUDA, we delegate to qe::cuda namespace functions
using namespace qe::cuda;
#endif

// If using ROCm, we would use qe::rocm namespace...

GPUQuantumRegister::GPUQuantumRegister(size_t n) : num_qubits(n) {
  size_t num_elements = 1ULL << n;
  size_t size_bytes = num_elements * sizeof(std::complex<double>);

  GPUContext::getInstance().initialize();
  device_state = GPUContext::getInstance().allocate(size_bytes);

  std::vector<std::complex<double>> initial_state(num_elements, {0.0, 0.0});
  initial_state[0] = {1.0, 0.0};

  GPUContext::getInstance().copyToDevice(device_state, initial_state.data(),
                                         size_bytes);
}

GPUQuantumRegister::~GPUQuantumRegister() {
  GPUContext::getInstance().free(device_state);
}

std::vector<std::complex<double>> GPUQuantumRegister::getStateVector() const {
  size_t num_elements = 1ULL << num_qubits;
  size_t size_bytes = num_elements * sizeof(std::complex<double>);
  std::vector<std::complex<double>> host_state(num_elements);

  GPUContext::getInstance().copyToHost(host_state.data(), device_state,
                                       size_bytes);
  return host_state;
}

void GPUQuantumRegister::applyHadamard(size_t target) {
#ifdef ENABLE_CUDA
  launchHadamard(device_state, num_qubits, target);
#endif
}

void GPUQuantumRegister::applyX(size_t target) {
#ifdef ENABLE_CUDA
  launchapplyX(device_state, num_qubits, target);
#endif
}

void GPUQuantumRegister::applyY(size_t target) {
#ifdef ENABLE_CUDA
  launchapplyY(device_state, num_qubits, target);
#endif
}

void GPUQuantumRegister::applyZ(size_t target) {
#ifdef ENABLE_CUDA
  launchapplyZ(device_state, num_qubits, target);
#endif
}

void GPUQuantumRegister::applyRotationY(size_t target, double angle) {
#ifdef ENABLE_CUDA
  launchRotationY(device_state, num_qubits, target, angle);
#endif
}
