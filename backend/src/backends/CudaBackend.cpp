#include "CudaBackend.hpp"
#include "../Exceptions.hpp"
#include "../Types.hpp"
#include "../kernels/GateKernels.hpp"
#include <cuComplex.h>

// Distributed & Accelerated Comm Libraries
#ifdef MPI_ENABLED
#include <mpi.h>
#endif
#ifdef ENABLE_NCCL
#include <nccl.h>
#endif

#include <cuda_runtime.h>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace qubit_engine {

CudaBackend::CudaBackend(size_t num_qubits)
    : num_qubits_(num_qubits), device_state_(nullptr),
      telemetry_stream_(nullptr), pinned_telemetry_buf_(nullptr),
      pinned_buf_size_(0) {

  // 1. Resolve MPI Rank and Size for Sharding context
  int rank = 0;
  int size = 1;
#ifdef MPI_ENABLED
  int mpi_initialized;
  MPI_Initialized(&mpi_initialized);
  if (mpi_initialized) {
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
  }
#endif
  mpi_rank_ = rank;
  mpi_size_ = size;

  // 2. Map logical rank to physical GPU device
  int num_devices = 0;
  cudaGetDeviceCount(&num_devices);
  if (num_devices == 0) {
    throw std::runtime_error(
        "CUDA Error: No CUDA-capable devices found for CudaBackend.");
  }
  int gpu_id = mpi_rank_ % num_devices;
  cudaSetDevice(gpu_id);

  // 3. Initialize NCCL
#ifdef ENABLE_NCCL
  ncclUniqueId id;
  if (mpi_rank_ == 0) {
    ncclGetUniqueId(&id);
  }
#ifdef MPI_ENABLED
  if (mpi_size_ > 1) {
    MPI_Bcast(&id, sizeof(id), MPI_BYTE, 0, MPI_COMM_WORLD);
  }
#endif

  ncclResult_t res = ncclCommInitRank(&nccl_comm_, mpi_size_, id, mpi_rank_);
  if (res != ncclSuccess) {
    throw std::runtime_error(
        "NCCL Error: Failed to initialize NCCL communicator: " +
        std::string(ncclGetErrorString(res)));
  }
#endif

  // 4. Init Local Device Shard Memory
  initializeCuda();

  // 5. Create secondary telemetry stream + pinned host buffer
  cudaStream_t tstream;
  cudaStreamCreate(&tstream);
  telemetry_stream_ = (void *)tstream;

  size_t dim = 1ULL << num_qubits_;
  pinned_buf_size_ = dim * sizeof(Complex);
  cudaError_t pin_err =
      cudaMallocHost(&pinned_telemetry_buf_, pinned_buf_size_);
  if (pin_err != cudaSuccess) {
    // Fallback: null buffer, getStateVectorAsync degrades to sync path
    pinned_telemetry_buf_ = nullptr;
    pinned_buf_size_ = 0;
  }
}

CudaBackend::~CudaBackend() {
  if (device_state_) {
    cudaFree(device_state_);
  }
  // Destroy telemetry stream and pinned buffer
  if (telemetry_stream_) {
    cudaStreamDestroy((cudaStream_t)telemetry_stream_);
  }
  if (pinned_telemetry_buf_) {
    cudaFreeHost(pinned_telemetry_buf_);
  }
#ifdef ENABLE_NCCL
  if (nccl_comm_) {
    ncclCommDestroy(nccl_comm_);
  }
#endif
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

#ifdef ENABLE_NCCL
void CudaBackend::gatherStateNCCL() {
  if (mpi_size_ <= 1) return;
  size_t local_dim = 1ULL << num_qubits_;
  size_t global_dim = local_dim * mpi_size_;
  Complex *gathered_state;
  cudaMalloc(&gathered_state, global_dim * sizeof(Complex));
  ncclAllGather((const void *)device_state_, (void *)gathered_state,
                local_dim, ncclFloat64, nccl_comm_, 0);
  cudaDeviceSynchronize();
  cudaFree(gathered_state);
}
#endif

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
#ifdef ENABLE_NCCL
  gatherStateNCCL();
#endif
  qe::cuda::launchCNOT(device_state_, num_qubits_, control, target);
}

// --- Advanced Gates ---

void CudaBackend::applyToffoli(size_t control1, size_t control2,
                               size_t target) {
#ifdef ENABLE_NCCL
  gatherStateNCCL();
#endif
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
  qe::cuda::launchRotationX(device_state_, num_qubits_, target, angle);
}

void CudaBackend::applySWAP(size_t qubit1, size_t qubit2) {
#ifdef ENABLE_NCCL
  gatherStateNCCL();
#endif
  // SWAP = CNOT(q1,q2) * CNOT(q2,q1) * CNOT(q1,q2)
  applyCNOT(qubit1, qubit2);
  applyCNOT(qubit2, qubit1);
  applyCNOT(qubit1, qubit2);
}

void CudaBackend::applyCZ(size_t control, size_t target) {
#ifdef ENABLE_NCCL
  gatherStateNCCL();
#endif
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

void CudaBackend::applyNoiseChannel1Q(const NoiseChannel1Q& /*channel*/,
                                       size_t /*target*/) {
  // TODO: Implement GPU-native Kraus channel application
  // For now, noise is applied at the QuantumRegister level via CPU fallback
}

void CudaBackend::applyNoiseChannel2Q(const NoiseChannel2Q& /*channel*/,
                                       size_t /*q1*/, size_t /*q2*/) {
  // TODO: Implement GPU-native 2Q Kraus channel application
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

std::vector<double> CudaBackend::getProbabilities() const {
  size_t dim = 1ULL << num_qubits_;

  // Allocate device memory for probabilities
  double *device_probs = nullptr;
  cudaError_t err = cudaMalloc(&device_probs, dim * sizeof(double));
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to allocate probs: " +
                             std::string(cudaGetErrorString(err)));
  }

  // Launch probability kernel (Local Shard)
  qe::cuda::launchComputeProbabilities(device_state_, device_probs, dim);

  // Cross-GPU Reduce
#ifdef ENABLE_NCCL
  if (mpi_size_ > 1) {
    double *reduced_probs = nullptr;
    cudaMalloc(&reduced_probs, dim * sizeof(double));

    ncclAllReduce((const void *)device_probs, (void *)reduced_probs, dim,
                  ncclDouble, ncclSum, nccl_comm_, 0);
    cudaDeviceSynchronize();

    cudaFree(device_probs);
    device_probs = reduced_probs;
  }
#endif

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

// --- Expectation Value (Device-Side Reduction) ---

double CudaBackend::expectationValue(const std::string &pauli_string) const {
  // Encode Pauli string into int array: 0=I, 1=X, 2=Y, 3=Z
  std::vector<int> h_pauli_ops(num_qubits_, 0);
  for (size_t q = 0; q < num_qubits_ && q < pauli_string.size(); ++q) {
    char op = pauli_string[q];
    if (op == 'X')
      h_pauli_ops[q] = 1;
    else if (op == 'Y')
      h_pauli_ops[q] = 2;
    else if (op == 'Z')
      h_pauli_ops[q] = 3;
  }

  // Allocate device-side pauli ops and result
  int *d_pauli_ops = nullptr;
  double *d_result = nullptr;
  cudaMalloc(&d_pauli_ops, num_qubits_ * sizeof(int));
  cudaMalloc(&d_result, sizeof(double));

  cudaMemcpy(d_pauli_ops, h_pauli_ops.data(), num_qubits_ * sizeof(int),
             cudaMemcpyHostToDevice);

  // Launch device-side reduction kernel
  qe::cuda::launchPauliExpectation(device_state_, num_qubits_, d_pauli_ops,
                                   d_result);

  // Copy only 8 bytes back
  double result = 0.0;
  cudaMemcpy(&result, d_result, sizeof(double), cudaMemcpyDeviceToHost);

  cudaFree(d_pauli_ops);
  cudaFree(d_result);

  return result;
}

// --- State Access ---

std::vector<Complex> CudaBackend::getStateVector() const {
  size_t dim = 1ULL << num_qubits_;
  std::vector<Complex> host_state(dim);
  copyStateToHost(host_state);
  return host_state;
}

// --- Async Telemetry Readback (Pinned Memory + Secondary Stream) ---

std::vector<Complex> CudaBackend::getStateVectorAsync() const {
  size_t dim = 1ULL << num_qubits_;
  size_t bytes = dim * sizeof(Complex);

  // If pinned buffer wasn't allocated, fall back to sync path
  if (!pinned_telemetry_buf_ || pinned_buf_size_ < bytes) {
    return getStateVector();
  }

  // Async copy on secondary telemetry stream
  cudaMemcpyAsync(pinned_telemetry_buf_, device_state_, bytes,
                  cudaMemcpyDeviceToHost, (cudaStream_t)telemetry_stream_);
  cudaStreamSynchronize((cudaStream_t)telemetry_stream_);

  // Copy from pinned buffer to vector
  std::vector<Complex> host_state(dim);
  std::memcpy(host_state.data(), pinned_telemetry_buf_, bytes);
  return host_state;
}

// --- JIT Fused Block Application ---

void CudaBackend::applyFusedBlock(const std::vector<int> &targets,
                                  const std::vector<Complex> &unitary) {
  int k = static_cast<int>(targets.size());
  if (k < 1 || k > 3)
    return; // Only support fused blocks up to 3 qubits

  int sub_dim = 1 << k;
  if (static_cast<int>(unitary.size()) != sub_dim * sub_dim)
    return;

  // Allocate device targets array
  int *d_targets = nullptr;
  cudaMalloc(&d_targets, k * sizeof(int));
  cudaMemcpy(d_targets, targets.data(), k * sizeof(int),
             cudaMemcpyHostToDevice);

  // Convert Complex → cuDoubleComplex for constant memory upload
  std::vector<cuDoubleComplex> h_unitary(sub_dim * sub_dim);
  for (int i = 0; i < sub_dim * sub_dim; ++i) {
    h_unitary[i] = make_cuDoubleComplex(unitary[i].real(), unitary[i].imag());
  }

  qe::cuda::launchFusedUnitary(device_state_, num_qubits_, d_targets, k,
                               h_unitary.data());

  cudaFree(d_targets);
}

void CudaBackend::applyDenseUnitary(const std::vector<size_t> &targets,
                                   const std::vector<Complex> &matrix) {
  throw FeatureNotSupportedException(
      "applyDenseUnitary not supported in CUDA backend.");
}

} // namespace qubit_engine