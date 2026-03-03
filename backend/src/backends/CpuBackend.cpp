#include "CpuBackend.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#ifdef _OPENMP
#include <omp.h>
#endif
#include <random>
#include <vector>

#ifdef MPI_ENABLED
#include <mpi.h>
#endif

// Intel SIMD intrinsics - only available on x86/x64, not ARM
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) ||             \
    defined(_M_IX86)
#define USE_AVX2_INTRINSICS
#include <immintrin.h>
#endif

// ARM NEON intrinsics - available on ARM64 (Apple Silicon M-series)
#if defined(__aarch64__) || defined(__arm64__)
#define USE_NEON_INTRINSICS
#include <arm_neon.h>
#endif

namespace qubit_engine {

static const Precision INV_SQRT_2 = 1.0f / std::sqrt(2.0f);

// --- Lifecycle ---
CpuBackend::CpuBackend(size_t n, bool force_local) : num_qubits(n) {
  local_rank = 0;
  world_size = 1;

#ifdef MPI_ENABLED
  if (!force_local) {
    int initialized;
    MPI_Initialized(&initialized);
    if (!initialized) {
      MPI_Init(NULL, NULL);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &local_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  }
#endif

  size_t total_dim = 1ULL << n;
  size_t local_dim = total_dim / world_size;
  if (local_dim == 0)
    local_dim = 1;

  state.resize(local_dim, 0.0f);
  if (local_rank == 0)
    state[0] = 1.0f;
}

// --- Core Gates ---

void CpuBackend::applyHadamard(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  if (stride < local_dim) {
#if defined(USE_NEON_INTRINSICS)
    // ARM NEON implementation for Apple Silicon (FLOAT Optimized)
    // float32x4_t holds 2 complex numbers (4 floats)
    float32x4_t v_inv_sqrt2 = vdupq_n_f32(INV_SQRT_2);

    // Threshold for when to switch to inner-loop parallelization
    // If we have few outer iterations (large stride), we must parallelize
    // independent inner blocks.
    const size_t PARALLEL_THRESHOLD = 2048;

    // Case 1: Small Stride (Many outer iterations) -> Parallelize Outer Loop
    if (2 * stride < local_dim / 4 || stride < PARALLEL_THRESHOLD) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
      for (long long i = 0; i < static_cast<long long>(local_dim);
           i += 2 * stride) {
        for (size_t j = i; j < i + stride; j += 2) {
          // Handle vectorization boundary for small strides
          if (j + 1 >= i + stride && stride < 2) {
            Complex a = state[j];
            Complex b = state[j + stride];
            state[j] = (a + b) * INV_SQRT_2;
            state[j + stride] = (a - b) * INV_SQRT_2;
            continue;
          }

          float *ptr_a = reinterpret_cast<float *>(&state[j]);
          float *ptr_b = reinterpret_cast<float *>(&state[j + stride]);

          float32x4_t v_a = vld1q_f32(ptr_a);
          float32x4_t v_b = vld1q_f32(ptr_b);

          float32x4_t v_sum = vaddq_f32(v_a, v_b);
          v_sum = vmulq_f32(v_sum, v_inv_sqrt2);

          float32x4_t v_diff = vsubq_f32(v_a, v_b);
          v_diff = vmulq_f32(v_diff, v_inv_sqrt2);

          vst1q_f32(ptr_a, v_sum);
          vst1q_f32(ptr_b, v_diff);
        }
      }
    } else {
      // Case 2: Large Stride (Few outer iterations, huge inner blocks) ->
      // Parallelize Inner Loop We iterate serially over outer blocks, but
      // inside each block we parallelize
      for (long long i = 0; i < static_cast<long long>(local_dim);
           i += 2 * stride) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (size_t j = i; j < i + stride; j += 2) {
          float *ptr_a = reinterpret_cast<float *>(&state[j]);
          float *ptr_b = reinterpret_cast<float *>(&state[j + stride]);

          float32x4_t v_a = vld1q_f32(ptr_a);
          float32x4_t v_b = vld1q_f32(ptr_b);

          float32x4_t v_sum = vaddq_f32(v_a, v_b);
          v_sum = vmulq_f32(v_sum, v_inv_sqrt2);

          float32x4_t v_diff = vsubq_f32(v_a, v_b);
          v_diff = vmulq_f32(v_diff, v_inv_sqrt2);

          vst1q_f32(ptr_a, v_sum);
          vst1q_f32(ptr_b, v_diff);
        }
      }
    }
#elif defined(USE_AVX2_INTRINSICS) && defined(__AVX2__)
    // AVX2 float implementation
    const size_t PARALLEL_THRESHOLD = 2048;
    __m256 v_inv_sqrt2 = _mm256_set1_ps(INV_SQRT_2);

    if (2 * stride < local_dim / 4 || stride < PARALLEL_THRESHOLD) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
      for (long long i = 0; i < static_cast<long long>(local_dim);
           i += 2 * stride) {
        long long j = i;
        if (stride >= 4) {
          for (; j + 3 < i + stride; j += 4) {
            float *ptr_a = reinterpret_cast<float *>(&state[j]);
            float *ptr_b = reinterpret_cast<float *>(&state[j + stride]);
            __m256 v_a = _mm256_loadu_ps(ptr_a);
            __m256 v_b = _mm256_loadu_ps(ptr_b);
            __m256 v_sum = _mm256_add_ps(v_a, v_b);
            __m256 v_diff = _mm256_sub_ps(v_a, v_b);
            v_sum = _mm256_mul_ps(v_sum, v_inv_sqrt2);
            v_diff = _mm256_mul_ps(v_diff, v_inv_sqrt2);
            _mm256_storeu_ps(ptr_a, v_sum);
            _mm256_storeu_ps(ptr_b, v_diff);
          }
        }
        for (; j < i + stride; ++j) {
          Complex a = state[j];
          Complex b = state[j + stride];
          state[j] = (a + b) * INV_SQRT_2;
          state[j + stride] = (a - b) * INV_SQRT_2;
        }
      }
    } else {
      for (long long i = 0; i < static_cast<long long>(local_dim);
           i += 2 * stride) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (long long j = i; j < i + stride; j += 4) {
          // Checking boundary inside parallel loop might be tricky, simplified
          // assuming aligned
          float *ptr_a = reinterpret_cast<float *>(&state[j]);
          float *ptr_b = reinterpret_cast<float *>(&state[j + stride]);
          __m256 v_a = _mm256_loadu_ps(ptr_a);
          __m256 v_b = _mm256_loadu_ps(ptr_b);
          __m256 v_sum = _mm256_add_ps(v_a, v_b);
          __m256 v_diff = _mm256_sub_ps(v_a, v_b);
          v_sum = _mm256_mul_ps(v_sum, v_inv_sqrt2);
          v_diff = _mm256_mul_ps(v_diff, v_inv_sqrt2);
          _mm256_storeu_ps(ptr_a, v_sum);
          _mm256_storeu_ps(ptr_b, v_diff);
        }
      }
    }
#else
    // Scalar fallback
    const size_t PARALLEL_THRESHOLD = 2048;
    if (2 * stride < local_dim / 4 || stride < PARALLEL_THRESHOLD) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
      for (long long i = 0; i < static_cast<long long>(local_dim);
           i += 2 * stride) {
        for (size_t j = i; j < i + stride; ++j) {
          Complex a = state[j];
          Complex b = state[j + stride];
          state[j] = (a + b) * INV_SQRT_2;
          state[j + stride] = (a - b) * INV_SQRT_2;
        }
      }
    } else {
      for (long long i = 0; i < static_cast<long long>(local_dim);
           i += 2 * stride) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (size_t j = i; j < i + stride; ++j) {
          Complex a = state[j];
          Complex b = state[j + stride];
          state[j] = (a + b) * INV_SQRT_2;
          state[j + stride] = (a - b) * INV_SQRT_2;
        }
      }
    }
#endif
  } else {
#ifdef MPI_ENABLED
    size_t rank_bit = stride / local_dim;
    bool is_one = (local_rank & rank_bit);
    int partner = local_rank ^ rank_bit;
    std::vector<Complex> recv_buf(local_dim);

    MPI_Sendrecv(state.data(), local_dim * 2, MPI_FLOAT, partner, 0,
                 recv_buf.data(), local_dim * 2, MPI_FLOAT, partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    if (is_one) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
      for (long long i = 0; i < static_cast<long long>(local_dim); ++i) {
        state[i] = (recv_buf[i] - state[i]) * INV_SQRT_2;
      }
    } else {
#ifdef _OPENMP
#pragma omp parallel for
#endif
      for (long long i = 0; i < static_cast<long long>(local_dim); ++i) {
        state[i] = (state[i] + recv_buf[i]) * INV_SQRT_2;
      }
    }
#else
    std::cerr << "Error: Global H requested but MPI not enabled." << std::endl;
#endif
  }
}

void CpuBackend::applyX(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  if (stride < local_dim) {
#if defined(USE_NEON_INTRINSICS)
    // Optimized Neon swap? Just copying bytes is fast enough or use intrinsics
    // if needed. X gate is just swap.
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j)
        std::swap(state[j], state[j + stride]);
    }
#else
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j)
        std::swap(state[j], state[j + stride]);
    }
#endif
  } else {
#ifdef MPI_ENABLED
    // Stub for MPI X
    size_t rank_bit = stride / local_dim;
    int partner = local_rank ^ rank_bit;
    std::vector<Complex> recv_buf(local_dim);
    MPI_Sendrecv(state.data(), local_dim * 2, MPI_FLOAT, partner, 0,
                 recv_buf.data(), local_dim * 2, MPI_FLOAT, partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    state = recv_buf;
#else
    std::cerr << "Error: Global X requested but MPI not enabled." << std::endl;
#endif
  }
}

void CpuBackend::applyY(size_t target) {
  Complex i_unit(0, 1);
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;
  if (stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim);
         i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        Complex a = state[j];
        Complex b = state[j + stride];
        state[j] = -i_unit * b;
        state[j + stride] = i_unit * a;
      }
    }
  } else {
    // MPI implementation skipped for brevity
    std::cerr << "Error: Global Y requested but MPI logic simplified."
              << std::endl;
  }
}

void CpuBackend::applyZ(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;
  if (stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim);
         i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j)
        state[j + stride] *= -1.0f;
    }
  } else {
#ifdef MPI_ENABLED
    size_t rank_bit = stride / local_dim;
    if (local_rank & rank_bit) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
      for (long long i = 0; i < static_cast<long long>(local_dim); ++i)
        state[i] *= -1.0f;
    }
#else
    std::cerr << "Error: Global Z requested but MPI not enabled." << std::endl;
#endif
  }
}

void CpuBackend::applyCNOT(size_t control, size_t target) {
  if (control == target)
    throw std::invalid_argument(
        "Control and Target qubits cannot be the same.");

  size_t local_dim = state.size();

  size_t c_stride = 1ULL << control;
  size_t t_stride = 1ULL << target;

  bool c_is_global = (c_stride >= local_dim);
  bool t_is_global = (t_stride >= local_dim);

  if (!c_is_global && !t_is_global) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim); ++i) {
      if ((i & c_stride)) {
        size_t partner = i ^ t_stride;
        if (i < partner) {
          std::swap(state[i], state[partner]);
        }
      }
    }
    return;
  }

  // MPI Logic omitted for brevity as CNOT global is complex without dedicated
  // buffers
  std::cerr << "Global CNOT not fully implemented in this refactor."
            << std::endl;
}

// --- Advanced Gates ---

void CpuBackend::applyToffoli(size_t c1, size_t c2, size_t t) {
  size_t local_dim = state.size();
  size_t c1_s = 1ULL << c1;
  size_t c2_s = 1ULL << c2;
  size_t t_s = 1ULL << t;

  if (t_s < local_dim && c1_s < local_dim && c2_s < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim); ++i) {
      if ((i & c1_s) && (i & c2_s) && !(i & t_s)) {
        std::swap(state[i], state[i + t_s]);
      }
    }
  }
}

void CpuBackend::applyPhaseS(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;
  Complex i_unit(0, 1);

  if (stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim);
         i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        state[j + stride] *= i_unit;
      }
    }
  }
}

void CpuBackend::applyPhaseT(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;
  Complex phase(1.0f / std::sqrt(2.0f), 1.0f / std::sqrt(2.0f));

  if (stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim);
         i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        state[j + stride] *= phase;
      }
    }
  }
}

void CpuBackend::applyRotationY(size_t target, Precision angle) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  Precision c = std::cos(angle / 2.0f);
  Precision s = std::sin(angle / 2.0f);

  if (stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim);
         i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        Complex a = state[j];
        Complex b = state[j + stride];
        state[j] = c * a - s * b;
        state[j + stride] = s * a + c * b;
      }
    }
  }
}

void CpuBackend::applyRotationZ(size_t target, Precision angle) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  Complex z0(std::cos(-angle / 2.0f), std::sin(-angle / 2.0f));
  Complex z1(std::cos(angle / 2.0f), std::sin(angle / 2.0f));

  if (stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim);
         i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        state[j] *= z0;
        state[j + stride] *= z1;
      }
    }
  }
}

void CpuBackend::applyRotationX(size_t target, Precision angle) {
  // Rx(θ) = [[cos(θ/2), -i*sin(θ/2)], [-i*sin(θ/2), cos(θ/2)]]
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  Precision c = std::cos(angle / 2.0f);
  Precision s = std::sin(angle / 2.0f);
  Complex neg_is(0, -s); // -i*sin(θ/2)

  if (stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim);
         i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        Complex a = state[j];
        Complex b = state[j + stride];
        state[j] = c * a + neg_is * b;
        state[j + stride] = neg_is * a + c * b;
      }
    }
  }
}

void CpuBackend::applySWAP(size_t qubit1, size_t qubit2) {
  // SWAP = 3 CNOTs: CNOT(q1,q2) * CNOT(q2,q1) * CNOT(q1,q2)
  // But direct implementation is simpler and more efficient
  size_t local_dim = state.size();
  size_t s1 = 1ULL << qubit1;
  size_t s2 = 1ULL << qubit2;

  if (s1 < local_dim && s2 < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim); ++i) {
      bool b1 = (i >> qubit1) & 1;
      bool b2 = (i >> qubit2) & 1;
      if (b1 != b2) {
        // Swap bits qubit1 and qubit2
        size_t partner = i ^ s1 ^ s2;
        if (i < partner) {
          std::swap(state[i], state[partner]);
        }
      }
    }
  }
}

void CpuBackend::applyCZ(size_t control, size_t target) {
  // CZ: Apply -1 phase when both qubits are |1>
  size_t local_dim = state.size();
  size_t c_stride = 1ULL << control;
  size_t t_stride = 1ULL << target;

  if (c_stride < local_dim && t_stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim); ++i) {
      if ((i & c_stride) && (i & t_stride)) {
        state[i] *= -1.0f;
      }
    }
  }
}

void CpuBackend::applyDepolarizingNoise(Precision probability) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0.0, 1.0);

  for (size_t i = 0; i < num_qubits; ++i) {
    if (dis(gen) < probability) {
      double type = dis(gen);
      if (type < 0.333)
        applyX(i);
      else if (type < 0.666)
        applyY(i);
      else
        applyZ(i);
    }
  }
}

// --- Measurement ---

int CpuBackend::measure(size_t target) {
  Precision prob0 = 0.0;
  size_t stride = 1ULL << target;
  for (size_t i = 0; i < state.size(); ++i) {
    if (!(i & stride))
      prob0 += std::norm(state[i]);
  }

  thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<> dis(0.0, 1.0);
  int outcome = (dis(gen) > prob0) ? 1 : 0;

  Precision norm = 0.0;
  if (outcome == 0) {
    for (size_t i = 0; i < state.size(); ++i) {
      if (i & stride)
        state[i] = 0.0f;
      else
        norm += std::norm(state[i]);
    }
  } else {
    for (size_t i = 0; i < state.size(); ++i) {
      if (!(i & stride))
        state[i] = 0.0f;
      else
        norm += std::norm(state[i]);
    }
  }
  norm = std::sqrt(norm);
  if (norm > 1e-9) {
    for (auto &val : state)
      val /= norm;
  }
  return outcome;
}

std::vector<double> CpuBackend::getProbabilities() {
  std::vector<double> probs(state.size(), 0.0);
  for (size_t i = 0; i < state.size(); ++i) {
    probs[i] = std::norm(state[i]);
  }
  return probs;
}

double CpuBackend::expectationValue(const std::string &pauli_string) {
  Complex expected_value = 0.0;
  size_t local_dim = state.size();
  // Note: Only safe for single-rank execution (local_dim == total_dim)
  // For distributed, this requires communication (Swap/Gather).
  // Assuming singleton execution for VQE on H2/LiH (small qubits).

  // #pragma omp parallel for reduction(+:expected_value) // Complex reduction
  // requires OpenMP 4.0+
  for (size_t i = 0; i < local_dim; ++i) {
    size_t j = i;
    Complex coeff = 1.0;

    for (size_t q = 0; q < num_qubits && q < pauli_string.size(); ++q) {
      char op = pauli_string[q];
      if (op == 'I')
        continue;

      bool bit_set = (i >> q) & 1;

      if (op == 'X') {
        j ^= (1ULL << q);
      } else if (op == 'Y') {
        j ^= (1ULL << q);
        // Y|0> = i|1>, Y|1> = -i|0>
        coeff *= (bit_set ? Complex(0, -1) : Complex(0, 1));
      } else if (op == 'Z') {
        if (bit_set)
          coeff *= -1.0;
      }
    }

    if (j < local_dim) {
      // <psi|P|psi> = sum_i conj(psi[i]) * coeff * psi[j]
      expected_value += std::conj(state[i]) * coeff * state[j];
    }
  }
  return expected_value.real();
}

std::vector<Complex> CpuBackend::getStateVector() const {
  return std::vector<Complex>(state.begin(), state.end());
}

} // namespace qubit_engine
