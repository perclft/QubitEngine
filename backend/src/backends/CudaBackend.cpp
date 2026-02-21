#include "CudaBackend.hpp"
#include "../Types.hpp"
#include "../kernels/GateKernels.hpp"
#include <cuda_runtime.h>
#include <iostream>
#include <random>
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
  size_t dim = 1ULL << num_qubits_;
  size_t size = dim * sizeof(Complex);
  cudaError_t err = cudaMalloc(&device_state_, size);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to allocate device memory: " +
                             std::string(cudaGetErrorString(err)));
  }

  // Initialize to |00...0> on device: index 0 = (1,0), rest = (0,0)
  err = cudaMemset(device_state_, 0, size);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to zero device memory: " +
                             std::string(cudaGetErrorString(err)));
  }

  // Set amplitude of |0...0> to 1.0
  Complex one(1.0, 0.0);
  err =
      cudaMemcpy(device_state_, &one, sizeof(Complex), cudaMemcpyHostToDevice);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to set initial state: " +
                             std::string(cudaGetErrorString(err)));
  }
}

void CudaBackend::copyStateToDevice(const std::vector<Complex> &host_state) {
  size_t size = host_state.size() * sizeof(Complex);
  cudaError_t err = cudaMemcpy(device_state_, host_state.data(), size,
                               cudaMemcpyHostToDevice);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to copy state to device: " +
                             std::string(cudaGetErrorString(err)));
  }
}

void CudaBackend::copyStateToHost(std::vector<Complex> &host_state) const {
  size_t size = host_state.size() * sizeof(Complex);
  cudaError_t err = cudaMemcpy(host_state.data(), device_state_, size,
                               cudaMemcpyDeviceToHost);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to copy state to host: " +
                             std::string(cudaGetErrorString(err)));
  }
}

// --- Core Gates ---

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
  qe::cuda::launchCNOT(device_state_, num_qubits_, control, target);
}

// --- Advanced Gates ---

void CudaBackend::applyToffoli(size_t control1, size_t control2,
                               size_t target) {
  qe::cuda::launchToffoli(device_state_, num_qubits_, control1, control2,
                          target);
}

void CudaBackend::applyPhaseS(size_t target) {
  qe::cuda::launchPhaseS(device_state_, num_qubits_, target);
}

void CudaBackend::applyPhaseT(size_t target) {
  qe::cuda::launchPhaseT(device_state_, num_qubits_, target);
}

void CudaBackend::applyRotationY(size_t target, Precision angle) {
  qe::cuda::launchRotationY(device_state_, num_qubits_, target, angle);
}

void CudaBackend::applyRotationZ(size_t target, Precision angle) {
  qe::cuda::launchRotationZ(device_state_, num_qubits_, target, angle);
}

void CudaBackend::applyRotationX(size_t target, Precision angle) {
  // Rx(θ) = H * Rz(θ) * H
  applyHadamard(target);
  applyRotationZ(target, angle);
  applyHadamard(target);
}

void CudaBackend::applySWAP(size_t qubit1, size_t qubit2) {
  // SWAP = CNOT(q1,q2) * CNOT(q2,q1) * CNOT(q1,q2)
  applyCNOT(qubit1, qubit2);
  applyCNOT(qubit2, qubit1);
  applyCNOT(qubit1, qubit2);
}

void CudaBackend::applyCZ(size_t control, size_t target) {
  // CZ = H(target) * CNOT(control, target) * H(target)
  applyHadamard(target);
  applyCNOT(control, target);
  applyHadamard(target);
}

// --- Noise ---

void CudaBackend::applyDepolarizingNoise(Precision probability) {
  // Depolarizing noise: with probability p, apply X, Y, or Z each with p/3
  // Fallback to host-side random gate selection
  if (probability <= 0.0)
    return;

  static std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<double> dist(0.0, 1.0);

  for (size_t q = 0; q < num_qubits_; ++q) {
    double r = dist(rng);
    if (r < probability) {
      double gate_r = dist(rng);
      if (gate_r < 1.0 / 3.0) {
        applyX(q);
      } else if (gate_r < 2.0 / 3.0) {
        applyY(q);
      } else {
        applyZ(q);
      }
    }
  }
}

// --- Measurement ---

int CudaBackend::measure(size_t target) {
  // Copy probabilities to host and sample
  std::vector<double> probs = getProbabilities();

  // Compute marginal probability of target qubit being |1>
  double prob_one = 0.0;
  size_t dim = 1ULL << num_qubits_;
  for (size_t i = 0; i < dim; ++i) {
    if ((i >> target) & 1) {
      prob_one += probs[i];
    }
  }

  // Sample
  static std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  int result = (dist(rng) < prob_one) ? 1 : 0;

  // Collapse state: zero out amplitudes inconsistent with measurement
  std::vector<Complex> state = getStateVector();
  double norm = 0.0;
  for (size_t i = 0; i < dim; ++i) {
    bool bit = (i >> target) & 1;
    if (bit != result) {
      state[i] = Complex(0.0, 0.0);
    } else {
      norm += std::norm(state[i]);
    }
  }

  // Renormalize
  double inv_norm = 1.0 / std::sqrt(norm);
  for (size_t i = 0; i < dim; ++i) {
    state[i] *= inv_norm;
  }

  // Copy collapsed state back to device
  copyStateToDevice(state);

  return result;
}

std::vector<double> CudaBackend::getProbabilities() {
  size_t dim = 1ULL << num_qubits_;

  // Allocate device memory for probabilities
  double *device_probs = nullptr;
  cudaError_t err = cudaMalloc(&device_probs, dim * sizeof(double));
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to allocate probs: " +
                             std::string(cudaGetErrorString(err)));
  }

  // Launch probability kernel
  qe::cuda::launchComputeProbabilities(device_state_, device_probs, dim);

  // Copy back to host
  std::vector<double> probs(dim);
  err = cudaMemcpy(probs.data(), device_probs, dim * sizeof(double),
                   cudaMemcpyDeviceToHost);
  cudaFree(device_probs);

  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to copy probs to host: " +
                             std::string(cudaGetErrorString(err)));
  }

  return probs;
}

// --- Expectation Value ---

double CudaBackend::expectationValue(const std::string &pauli_string) {
  // Fallback: Copy to host and compute on CPU
  std::vector<Complex> state = getStateVector();

  Complex expected_value = 0.0;
  size_t local_dim = state.size();

  for (size_t i = 0; i < local_dim; ++i) {
    size_t j = i;
    Complex coeff = 1.0;

    for (size_t q = 0; q < num_qubits_ && q < pauli_string.size(); ++q) {
      char op = pauli_string[q];
      if (op == 'I')
        continue;

      bool bit_set = (i >> q) & 1;

      if (op == 'X') {
        j ^= (1ULL << q);
      } else if (op == 'Y') {
        j ^= (1ULL << q);
        coeff *= (bit_set ? Complex(0, -1) : Complex(0, 1));
      } else if (op == 'Z') {
        if (bit_set)
          coeff *= -1.0;
      }
    }

    if (j < local_dim) {
      expected_value += std::conj(state[i]) * coeff * state[j];
    }
  }
  return expected_value.real();
}

// --- State Access ---

std::vector<Complex> CudaBackend::getStateVector() const {
  size_t dim = 1ULL << num_qubits_;
  std::vector<Complex> host_state(dim);
  copyStateToHost(host_state);
  return host_state;
}

} // namespace qubit_engine