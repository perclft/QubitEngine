#include "CudaBackend.hpp"
#include "../Exceptions.hpp"
#include "../Types.hpp"
#include "../kernels/GateKernels.hpp"
#include <cuComplex.h>
#include <functional>

// Distributed & Accelerated Comm Libraries
#ifdef MPI_ENABLED
#include <mpi.h>
inline MPI_Datatype get_mpi_precision_type() {
    return (sizeof(qubit_engine::Precision) == 8) ? MPI_DOUBLE : MPI_FLOAT;
}
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
  size_t total_dim = 1ULL << num_qubits_;
  size_t local_dim = total_dim / mpi_size_;
  if (local_dim == 0)
    local_dim = 1;

  size_t size = local_dim * sizeof(Complex);
  cudaError_t err = cudaMalloc(&device_state_, size);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to allocate device memory: " +
                             std::string(cudaGetErrorString(err)));
  }

  // Initialize to |00...0> on device: index 0 = (1,0) on rank 0, rest = (0,0)
  err = cudaMemset(device_state_, 0, size);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to zero device memory: " +
                             std::string(cudaGetErrorString(err)));
  }

  if (mpi_rank_ == 0) {
    Complex one(1.0, 0.0);
    err = cudaMemcpy(device_state_, &one, sizeof(Complex), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
      throw std::runtime_error("CUDA Error: Failed to set initial state: " +
                               std::string(cudaGetErrorString(err)));
    }
  }
}

void CudaBackend::copyStateToDevice(const std::vector<Complex> &host_state) {
  size_t total_dim = 1ULL << num_qubits_;
  size_t local_dim = total_dim / mpi_size_;
  if (local_dim == 0)
    local_dim = 1;

  size_t size = local_dim * sizeof(Complex);
  cudaError_t err = cudaMemcpy(device_state_, host_state.data() + mpi_rank_ * local_dim, size,
                               cudaMemcpyHostToDevice);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to copy state to device: " +
                             std::string(cudaGetErrorString(err)));
  }
}

void CudaBackend::copyStateToHost(std::vector<Complex> &host_state) const {
  auto full_state = getStateVector();
  size_t size = std::min(host_state.size(), full_state.size()) * sizeof(Complex);
  std::memcpy(host_state.data(), full_state.data(), size);
}

void CudaBackend::applyGateDistributed(std::function<void(void*)> launchKernel) {
#ifdef ENABLE_NCCL
  if (mpi_size_ > 1) {
    size_t total_dim = 1ULL << num_qubits_;
    size_t local_dim = total_dim / mpi_size_;
    if (local_dim == 0)
      local_dim = 1;

    Complex *gathered_state = nullptr;
    cudaError_t err = cudaMalloc(&gathered_state, total_dim * sizeof(Complex));
    if (err != cudaSuccess) {
      throw std::runtime_error("CUDA Error: Failed to allocate gathered state: " +
                               std::string(cudaGetErrorString(err)));
    }

    ncclResult_t res = ncclAllGather((const void *)device_state_, (void *)gathered_state,
                                     local_dim * 2, ncclFloat64, nccl_comm_, 0);
    if (res != ncclSuccess) {
      cudaFree(gathered_state);
      throw std::runtime_error("NCCL Error: ncclAllGather failed: " +
                               std::string(ncclGetErrorString(res)));
    }
    cudaDeviceSynchronize();

    launchKernel(gathered_state);
    cudaDeviceSynchronize();

    err = cudaMemcpy(device_state_, gathered_state + mpi_rank_ * local_dim,
                     local_dim * sizeof(Complex), cudaMemcpyDeviceToDevice);
    cudaFree(gathered_state);

    if (err != cudaSuccess) {
      throw std::runtime_error("CUDA Error: Failed to copy shard back to device: " +
                               std::string(cudaGetErrorString(err)));
    }
    return;
  }
#endif
  launchKernel(device_state_);
}

#ifdef ENABLE_NCCL
void CudaBackend::gatherStateNCCL() {
  // Deprecated: logic handled by applyGateDistributed
}
#endif

// --- Core Gates ---

void CudaBackend::applyHadamard(size_t target) {
  applyGateDistributed([this, target](void *state_ptr) {
    qe::cuda::launchHadamard(state_ptr, num_qubits_, target);
  });
}

void CudaBackend::applyX(size_t target) {
  applyGateDistributed([this, target](void *state_ptr) {
    qe::cuda::launchapplyX(state_ptr, num_qubits_, target);
  });
}

void CudaBackend::applyY(size_t target) {
  applyGateDistributed([this, target](void *state_ptr) {
    qe::cuda::launchapplyY(state_ptr, num_qubits_, target);
  });
}

void CudaBackend::applyZ(size_t target) {
  applyGateDistributed([this, target](void *state_ptr) {
    qe::cuda::launchapplyZ(state_ptr, num_qubits_, target);
  });
}

void CudaBackend::applyCNOT(size_t control, size_t target) {
  applyGateDistributed([this, control, target](void *state_ptr) {
    qe::cuda::launchCNOT(state_ptr, num_qubits_, control, target);
  });
}

// --- Advanced Gates ---

void CudaBackend::applyToffoli(size_t control1, size_t control2,
                               size_t target) {
  applyGateDistributed([this, control1, control2, target](void *state_ptr) {
    qe::cuda::launchToffoli(state_ptr, num_qubits_, control1, control2, target);
  });
}

void CudaBackend::applyPhaseS(size_t target) {
  applyGateDistributed([this, target](void *state_ptr) {
    qe::cuda::launchPhaseS(state_ptr, num_qubits_, target);
  });
}

void CudaBackend::applyPhaseT(size_t target) {
  applyGateDistributed([this, target](void *state_ptr) {
    qe::cuda::launchPhaseT(state_ptr, num_qubits_, target);
  });
}

void CudaBackend::applyRotationY(size_t target, Precision angle) {
  applyGateDistributed([this, target, angle](void *state_ptr) {
    qe::cuda::launchRotationY(state_ptr, num_qubits_, target, angle);
  });
}

void CudaBackend::applyRotationZ(size_t target, Precision angle) {
  applyGateDistributed([this, target, angle](void *state_ptr) {
    qe::cuda::launchRotationZ(state_ptr, num_qubits_, target, angle);
  });
}

void CudaBackend::applyRotationX(size_t target, Precision angle) {
  applyGateDistributed([this, target, angle](void *state_ptr) {
    qe::cuda::launchRotationX(state_ptr, num_qubits_, target, angle);
  });
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


void CudaBackend::applyNoiseChannel1Q(const NoiseChannel1Q& channel,
                                       size_t target) {
  if (channel.operators.empty()) return;

  // Use the pre-computed uniform probability weights for stochastic unravelling.
  // This avoids a full state reduction O(2^N) on the GPU per gate.
  Precision total_p = 0.0;
  for (const auto& op : channel.operators) {
      total_p += op.probability;
  }

  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<Precision> dis(0.0, total_p);
  Precision r = dis(gen);

  const KrausOperator1Q* selected = &channel.operators.back();
  Precision cumulative = 0.0;
  for (size_t i = 0; i < channel.operators.size(); ++i) {
    cumulative += channel.operators[i].probability;
    if (r <= cumulative) {
      selected = &channel.operators[i];
      break;
    }
  }

  if (selected->probability < 1e-20) return;

  Precision inv_norm = 1.0 / std::sqrt(selected->probability);
  
  // Convert std::array<Complex, 4> to cuDoubleComplex array for launch
  std::vector<cuDoubleComplex> m(4);
  for(int i=0; i<4; ++i) {
      m[i] = make_cuDoubleComplex(selected->matrix[i].real(), selected->matrix[i].imag());
  }

  applyGateDistributed([this, target, &m, inv_norm](void *state_ptr) {
    qe::cuda::launchApplyKraus1Q(state_ptr, num_qubits_, target, m.data(), inv_norm);
  });
}

void CudaBackend::applyNoiseChannel2Q(const NoiseChannel2Q& channel,
                                       size_t q1, size_t q2) {
  if (channel.operators.empty()) return;

  Precision total_p = 0.0;
  for (const auto& op : channel.operators) {
      total_p += op.probability;
  }

  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<Precision> dis(0.0, total_p);
  Precision r = dis(gen);

  const KrausOperator2Q* selected = &channel.operators.back();
  Precision cumulative = 0.0;
  for (size_t i = 0; i < channel.operators.size(); ++i) {
    cumulative += channel.operators[i].probability;
    if (r <= cumulative) {
      selected = &channel.operators[i];
      break;
    }
  }

  if (selected->probability < 1e-20) return;

  Precision inv_norm = 1.0 / std::sqrt(selected->probability);
  
  std::vector<cuDoubleComplex> m(16);
  for(int i=0; i<16; ++i) {
      m[i] = make_cuDoubleComplex(selected->matrix[i].real(), selected->matrix[i].imag());
  }

  applyGateDistributed([this, q1, q2, &m, inv_norm](void *state_ptr) {
    qe::cuda::launchApplyKraus2Q(state_ptr, num_qubits_, q1, q2, m.data(), inv_norm);
  });
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
  size_t total_dim = 1ULL << num_qubits_;

  // Allocate device memory for probabilities
  double *device_probs = nullptr;
  cudaError_t err = cudaMalloc(&device_probs, total_dim * sizeof(double));
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to allocate probs: " +
                             std::string(cudaGetErrorString(err)));
  }

  void *eval_state = device_state_;
#ifdef ENABLE_NCCL
  size_t local_dim = total_dim / mpi_size_;
  if (local_dim == 0) local_dim = 1;
  Complex *gathered_state = nullptr;
  if (mpi_size_ > 1) {
    cudaMalloc(&gathered_state, total_dim * sizeof(Complex));
    ncclAllGather((const void *)device_state_, (void *)gathered_state,
                  local_dim * 2, ncclFloat64, nccl_comm_, 0);
    cudaDeviceSynchronize();
    eval_state = gathered_state;
  }
#endif

  // Launch probability kernel (eval_state is the gathered full state)
  qe::cuda::launchComputeProbabilities(eval_state, device_probs, total_dim);

  // Copy back to host
  std::vector<double> probs(total_dim);
  err = cudaMemcpy(probs.data(), device_probs, total_dim * sizeof(double),
                   cudaMemcpyDeviceToHost);

#ifdef ENABLE_NCCL
  if (gathered_state) {
    cudaFree(gathered_state);
  }
#endif
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

  // If mpi_size_ > 1, we must gather the state vector first!
  void *eval_state = device_state_;
#ifdef ENABLE_NCCL
  size_t total_dim = 1ULL << num_qubits_;
  size_t local_dim = total_dim / mpi_size_;
  if (local_dim == 0) local_dim = 1;
  Complex *gathered_state = nullptr;
  if (mpi_size_ > 1) {
    cudaMalloc(&gathered_state, total_dim * sizeof(Complex));
    ncclAllGather((const void *)device_state_, (void *)gathered_state,
                  local_dim * 2, ncclFloat64, nccl_comm_, 0);
    cudaDeviceSynchronize();
    eval_state = gathered_state;
  }
#endif

  // Launch device-side reduction kernel
  qe::cuda::launchPauliExpectation(eval_state, num_qubits_, d_pauli_ops,
                                   d_result);

  // Copy only 8 bytes back
  double result = 0.0;
  cudaMemcpy(&result, d_result, sizeof(double), cudaMemcpyDeviceToHost);

#ifdef ENABLE_NCCL
  if (gathered_state) {
    cudaFree(gathered_state);
  }
#endif

  cudaFree(d_pauli_ops);
  cudaFree(d_result);

  return result;
}

// --- State Access ---

std::vector<Complex> CudaBackend::getStateVector() const {
  size_t total_dim = 1ULL << num_qubits_;
  size_t local_dim = total_dim / mpi_size_;
  if (local_dim == 0) local_dim = 1;

  std::vector<Complex> local_host_state(local_dim);
  cudaError_t err = cudaMemcpy(local_host_state.data(), device_state_, local_dim * sizeof(Complex),
                               cudaMemcpyDeviceToHost);
  if (err != cudaSuccess) {
    throw std::runtime_error("CUDA Error: Failed to copy local state to host: " +
                             std::string(cudaGetErrorString(err)));
  }

#ifdef MPI_ENABLED
  if (mpi_size_ > 1) {
      std::vector<Complex> global_host_state(total_dim);
      MPI_Allgather(local_host_state.data(), local_dim * 2, get_mpi_precision_type(),
                    global_host_state.data(), local_dim * 2, get_mpi_precision_type(),
                    MPI_COMM_WORLD);
      return global_host_state;
  }
#endif
  return local_host_state;
}

// --- Async Telemetry Readback (Pinned Memory + Secondary Stream) ---

std::vector<Complex> CudaBackend::getStateVectorAsync() const {
  if (mpi_size_ > 1) {
    return getStateVector();
  }

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

  applyGateDistributed([this, d_targets, k, &h_unitary](void *state_ptr) {
    qe::cuda::launchFusedUnitary(state_ptr, num_qubits_, d_targets, k,
                                 h_unitary.data());
  });

  cudaFree(d_targets);
}

void CudaBackend::applyDenseUnitary(const std::vector<size_t> &targets,
                                    const std::vector<Complex> &matrix) {
  int k = static_cast<int>(targets.size());
  if (k < 1 || k > 3) {
    throw FeatureNotSupportedException(
        "applyDenseUnitary only supports up to 3 target qubits in CUDA backend.");
  }
  std::vector<int> int_targets(targets.begin(), targets.end());
  applyFusedBlock(int_targets, matrix);
}

} // namespace qubit_engine