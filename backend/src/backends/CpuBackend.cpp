#include "CpuBackend.hpp"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#include <random>
#include <vector>

#ifdef MPI_ENABLED
#include <mpi.h>
#endif
#include <complex>

#ifdef MPI_ENABLED
inline MPI_Datatype get_mpi_precision_type() {
    return (sizeof(qubit_engine::Precision) == 8) ? MPI_DOUBLE : MPI_FLOAT;
}
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

static const Precision INV_SQRT_2 = 1.0 / std::sqrt(2.0);

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

  state.resize(local_dim, 0.0);
  if (local_rank == 0)
    state[0] = 1.0;
}

// --- Core Gates ---

void CpuBackend::applyHadamard(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  if (stride < local_dim) {
#if defined(USE_NEON_INTRINSICS)
    // ARM NEON implementation for Apple Silicon (DOUBLE Optimized)
    // float64x2_t holds 1 complex number (2 doubles)
    float64x2_t v_inv_sqrt2 = vdupq_n_f64(INV_SQRT_2);

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
        for (size_t j = i; j < i + stride; j += 1) {
          double *ptr_a = reinterpret_cast<double *>(&state[j]);
          double *ptr_b = reinterpret_cast<double *>(&state[j + stride]);

          float64x2_t v_a = vld1q_f64(ptr_a);
          float64x2_t v_b = vld1q_f64(ptr_b);

          float64x2_t v_sum = vaddq_f64(v_a, v_b);
          v_sum = vmulq_f64(v_sum, v_inv_sqrt2);

          float64x2_t v_diff = vsubq_f64(v_a, v_b);
          v_diff = vmulq_f64(v_diff, v_inv_sqrt2);

          vst1q_f64(ptr_a, v_sum);
          vst1q_f64(ptr_b, v_diff);
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
        for (size_t j = i; j < i + stride; j += 1) {
          double *ptr_a = reinterpret_cast<double *>(&state[j]);
          double *ptr_b = reinterpret_cast<double *>(&state[j + stride]);

          float64x2_t v_a = vld1q_f64(ptr_a);
          float64x2_t v_b = vld1q_f64(ptr_b);

          float64x2_t v_sum = vaddq_f64(v_a, v_b);
          v_sum = vmulq_f64(v_sum, v_inv_sqrt2);

          float64x2_t v_diff = vsubq_f64(v_a, v_b);
          v_diff = vmulq_f64(v_diff, v_inv_sqrt2);

          vst1q_f64(ptr_a, v_sum);
          vst1q_f64(ptr_b, v_diff);
        }
      }
    }
#elif defined(USE_AVX2_INTRINSICS) && defined(__AVX2__)
    // AVX2 double implementation
    const size_t PARALLEL_THRESHOLD = 2048;
    __m256d v_inv_sqrt2 = _mm256_set1_pd(INV_SQRT_2);

    if (2 * stride < local_dim / 4 || stride < PARALLEL_THRESHOLD) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
      for (long long i = 0; i < static_cast<long long>(local_dim);
           i += 2 * stride) {
        long long j = i;
        if (stride >= 2) {
          for (; j + 1 < i + stride; j += 2) {
            double *ptr_a = reinterpret_cast<double *>(&state[j]);
            double *ptr_b = reinterpret_cast<double *>(&state[j + stride]);
            __m256d v_a = _mm256_loadu_pd(ptr_a);
            __m256d v_b = _mm256_loadu_pd(ptr_b);
            __m256d v_sum = _mm256_add_pd(v_a, v_b);
            __m256d v_diff = _mm256_sub_pd(v_a, v_b);
            v_sum = _mm256_mul_pd(v_sum, v_inv_sqrt2);
            v_diff = _mm256_mul_pd(v_diff, v_inv_sqrt2);
            _mm256_storeu_pd(ptr_a, v_sum);
            _mm256_storeu_pd(ptr_b, v_diff);
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
        long long j = i;
        if (stride >= 2) {
          long long end_k = i + stride - 1;
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
          for (long long k = i; k < end_k; k += 2) {
            double *ptr_a = reinterpret_cast<double *>(&state[k]);
            double *ptr_b = reinterpret_cast<double *>(&state[k + stride]);
            __m256d v_a = _mm256_loadu_pd(ptr_a);
            __m256d v_b = _mm256_loadu_pd(ptr_b);
            __m256d v_sum = _mm256_add_pd(v_a, v_b);
            __m256d v_diff = _mm256_sub_pd(v_a, v_b);
            v_sum = _mm256_mul_pd(v_sum, v_inv_sqrt2);
            v_diff = _mm256_mul_pd(v_diff, v_inv_sqrt2);
            _mm256_storeu_pd(ptr_a, v_sum);
            _mm256_storeu_pd(ptr_b, v_diff);
          }
        } else {
          // stride == 1
          Complex a = state[i];
          Complex b = state[i + stride];
          state[i] = (a + b) * INV_SQRT_2;
          state[i + stride] = (a - b) * INV_SQRT_2;
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

    MPI_Sendrecv(state.data(), local_dim * 2, get_mpi_precision_type(), partner, 0,
                 recv_buf.data(), local_dim * 2, get_mpi_precision_type(), partner, 0,
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
    spdlog::error("Global H requested but MPI not enabled.");
#endif
  }
}

void CpuBackend::applyX(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  if (stride < local_dim) {
#if defined(USE_NEON_INTRINSICS)
    // Optimized Neon swap can be expanded, but parallelizing the outer loop is more impactful for large dimensions
#pragma omp parallel for
    for (long long i = 0; i < static_cast<long long>(local_dim); i += 2 * stride) {
      for (size_t j = static_cast<size_t>(i); j < static_cast<size_t>(i) + stride; ++j)
        std::swap(state[j], state[j + stride]);
    }
#else
#pragma omp parallel for
    for (long long i = 0; i < static_cast<long long>(local_dim); i += 2 * stride) {
      for (size_t j = static_cast<size_t>(i); j < static_cast<size_t>(i) + stride; ++j)
        std::swap(state[j], state[j + stride]);
    }
#endif
  } else {
#ifdef MPI_ENABLED
    // Stub for MPI X
    size_t rank_bit = stride / local_dim;
    int partner = local_rank ^ rank_bit;
    std::vector<Complex> recv_buf(local_dim);
    MPI_Sendrecv(state.data(), local_dim * 2, get_mpi_precision_type(), partner, 0,
                 recv_buf.data(), local_dim * 2, get_mpi_precision_type(), partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    state.assign(recv_buf.begin(), recv_buf.end());
#else
    spdlog::error("Global X requested but MPI not enabled.");
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
    spdlog::error("Global Y requested but MPI logic simplified.");
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
        state[j + stride] *= -1.0;
    }
  } else {
#ifdef MPI_ENABLED
    size_t rank_bit = stride / local_dim;
    if (local_rank & rank_bit) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
      for (long long i = 0; i < static_cast<long long>(local_dim); ++i)
        state[i] *= -1.0;
    }
#else
    spdlog::error("Global Z requested but MPI not enabled.");
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

  if (c_is_global && !t_is_global) {
    size_t rank_bit = c_stride / local_dim;
    if (local_rank & rank_bit) {
      applyX(target);
    }
    return;
  }

  if (!c_is_global && t_is_global) {
#ifdef MPI_ENABLED
    size_t rank_bit = t_stride / local_dim;
    int partner = local_rank ^ rank_bit;
    std::vector<Complex> recv_buf(local_dim);

    // This is basically a conditional swap with partner rank
    MPI_Sendrecv(state.data(), local_dim * 2, get_mpi_precision_type(), partner, 0,
                 recv_buf.data(), local_dim * 2, get_mpi_precision_type(), partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim); ++i) {
      if ((i & c_stride)) {
        state[i] = recv_buf[i];
      }
    }
    return;
#else
    spdlog::error("Global CNOT requested but MPI not enabled.");
    return;
#endif
  }

  if (c_is_global && t_is_global) {
#ifdef MPI_ENABLED
    size_t c_rank_bit = c_stride / local_dim;
    size_t t_rank_bit = t_stride / local_dim;
    if (local_rank & c_rank_bit) {
      // Control is 1, so we flip the global target
      int partner = local_rank ^ t_rank_bit;
      std::vector<Complex> recv_buf(local_dim);
      MPI_Sendrecv(state.data(), local_dim * 2, get_mpi_precision_type(), partner, 0,
                   recv_buf.data(), local_dim * 2, get_mpi_precision_type(), partner, 0,
                   MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      state.assign(recv_buf.begin(), recv_buf.end());
    }
    return;
#endif
  }

  spdlog::error("Complex Global CNOT not fully implemented.");
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
  // T-gate phase: e^{iπ/4} = cos(π/4) + i*sin(π/4) = 1/√2 + i/√2
  Complex phase(1.0 / std::sqrt(2.0), 1.0 / std::sqrt(2.0));

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

  Precision c = std::cos(angle / 2.0);
  Precision s = std::sin(angle / 2.0);

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

  Complex z0(std::cos(-angle / 2.0), std::sin(-angle / 2.0));
  Complex z1(std::cos(angle / 2.0), std::sin(angle / 2.0));

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

  Precision c = std::cos(angle / 2.0);
  Precision s = std::sin(angle / 2.0);
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
        state[i] *= -1.0;
      }
    }
  }
}

void CpuBackend::applyDepolarizingNoise(Precision probability) {
  static thread_local std::mt19937 gen(std::random_device{}());
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

std::array<Complex, 4> CpuBackend::getReducedDensityMatrix1Q(size_t target) const {
  size_t stride = 1ULL << target;
  Complex r00(0, 0), r01(0, 0), r11(0, 0);

  for (size_t i = 0; i < state.size(); i += 2 * stride) {
    for (size_t j = i; j < i + stride; ++j) {
      Complex c0 = state[j];
      Complex c1 = state[j + stride];
      r00 += c0 * std::conj(c0);
      r11 += c1 * std::conj(c1);
      r01 += c0 * std::conj(c1);
    }
  }
  return {r00, r01, std::conj(r01), r11};
}

std::array<Complex, 16> CpuBackend::getReducedDensityMatrix2Q(size_t q1, size_t q2) const {
  size_t stride1 = 1ULL << q1;
  size_t stride2 = 1ULL << q2;
  std::array<Complex, 16> rdm{};
  rdm.fill(Complex(0, 0));

  for (size_t j = 0; j < state.size(); ++j) {
    if (!((j & stride1) || (j & stride2))) {
      size_t idx[4] = {j, j | stride2, j | stride1, j | stride1 | stride2};
      Complex amps[4] = {state[idx[0]], state[idx[1]], state[idx[2]], state[idx[3]]};
      for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
          rdm[r * 4 + c] += amps[r] * std::conj(amps[c]);
        }
      }
    }
  }
  return rdm;
}

void CpuBackend::applyNoiseChannel1Q(const NoiseChannel1Q& channel,
                                      size_t target) {
  if (channel.operators.empty()) return;

  // 1. Compute Reduced Density Matrix for the target qubit
  auto rdm = getReducedDensityMatrix1Q(target);

  // 2. Compute exact selection probabilities: P(i) = Tr(Ki† Ki ρ)
  std::vector<Precision> probabilities;
  probabilities.reserve(channel.operators.size());
  Precision total_p = 0.0;

  for (const auto& op : channel.operators) {
    // Tr(M ρ) = M00*ρ00 + M01*ρ10 + M10*ρ01 + M11*ρ11
    const auto& M = op.matrix_dag_self;
    Complex tr = M[0] * rdm[0] + M[1] * rdm[2] + M[2] * rdm[1] + M[3] * rdm[3];
    Precision p = std::abs(tr.real()); // Tr(Aρ) is always real for Hermitian A
    probabilities.push_back(p);
    total_p += p;
  }

  // 3. Stochastic selection
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<Precision> dis(0.0, total_p);
  Precision r = dis(gen);

  const KrausOperator1Q* selected = &channel.operators.back();
  Precision selected_prob = probabilities.back();
  Precision cumulative = 0.0;
  for (size_t i = 0; i < channel.operators.size(); ++i) {
    cumulative += probabilities[i];
    if (r <= cumulative) {
      selected = &channel.operators[i];
      selected_prob = probabilities[i];
      break;
    }
  }

  if (selected_prob < 1e-20) return; // Should not happen for complete channels

  // 4. Apply the selected Kraus operator
  const auto& m = selected->matrix;
  size_t stride = 1ULL << target;
  size_t dim = state.size();

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (long long i = 0; i < static_cast<long long>(dim); i += 2 * stride) {
    for (size_t j = static_cast<size_t>(i); j < static_cast<size_t>(i) + stride; ++j) {
      Complex a = state[j];
      Complex b = state[j + stride];
      state[j]          = m[0] * a + m[1] * b;
      state[j + stride] = m[2] * a + m[3] * b;
    }
  }

  // 5. Renormalize: |ψ'⟩ = Ki|ψ⟩ / sqrt(P(i))
  Precision inv_norm = 1.0 / std::sqrt(selected_prob);
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (long long i = 0; i < static_cast<long long>(dim); ++i) {
    state[i] *= inv_norm;
  }
}

void CpuBackend::applyNoiseChannel2Q(const NoiseChannel2Q& channel,
                                      size_t q1, size_t q2) {
  if (channel.operators.empty()) return;

  // 1. Compute Reduced Density Matrix for the qubit pair
  auto rdm = getReducedDensityMatrix2Q(q1, q2);

  // 2. Compute exact selection probabilities
  std::vector<Precision> probabilities;
  probabilities.reserve(channel.operators.size());
  Precision total_p = 0.0;

  for (const auto& op : channel.operators) {
    const auto& M = op.matrix_dag_self;
    Complex tr(0, 0);
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        // Tr(M ρ) = Σ_{r,c} M_{rc} ρ_{cr}
        tr += M[r * 4 + c] * rdm[c * 4 + r];
      }
    }
    Precision p = std::abs(tr.real());
    probabilities.push_back(p);
    total_p += p;
  }

  // 3. Stochastic selection
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<Precision> dis(0.0, total_p);
  Precision r = dis(gen);

  const KrausOperator2Q* selected = &channel.operators.back();
  Precision selected_prob = probabilities.back();
  Precision cumulative = 0.0;
  for (size_t i = 0; i < channel.operators.size(); ++i) {
    cumulative += probabilities[i];
    if (r <= cumulative) {
      selected = &channel.operators[i];
      selected_prob = probabilities[i];
      break;
    }
  }

  if (selected_prob < 1e-20) return;

  // 4. Apply the selected 4×4 Kraus operator
  const auto& m = selected->matrix;
  size_t m0 = 1ULL << q1;
  size_t m1 = 1ULL << q2;
  size_t dim = state.size();

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (long long i = 0; i < static_cast<long long>(dim); ++i) {
    if (!((i & m0) || (i & m1))) {
      size_t idx[4] = {
        static_cast<size_t>(i),
        static_cast<size_t>(i | m1),
        static_cast<size_t>(i | m0),
        static_cast<size_t>(i | m0 | m1)
      };
      Complex v[4] = {state[idx[0]], state[idx[1]], state[idx[2]], state[idx[3]]};
      for (int row = 0; row < 4; ++row) {
        Complex res(0, 0);
        for (int col = 0; col < 4; ++col) {
          res += m[row * 4 + col] * v[col];
        }
        state[idx[row]] = res;
      }
    }
  }

  // 5. Renormalize
  Precision inv_norm = 1.0 / std::sqrt(selected_prob);
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (long long i = 0; i < static_cast<long long>(dim); ++i) {
    state[i] *= inv_norm;
  }
}

// --- Measurement ---

int CpuBackend::measure(size_t target) {
  Precision prob0 = 0.0;
  size_t stride = 1ULL << target;
  long long dim = static_cast<long long>(state.size());

  // Parallel probability accumulation
#ifdef _OPENMP
#pragma omp parallel for reduction(+:prob0) schedule(static)
#endif
  for (long long i = 0; i < dim; ++i) {
    if (!(i & stride))
      prob0 += std::norm(state[i]);
  }

  thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<> dis(0.0, 1.0);
  int outcome = (dis(gen) > prob0) ? 1 : 0;

  // Parallel projection and norm accumulation
  Precision norm = 0.0;
  if (outcome == 0) {
#ifdef _OPENMP
#pragma omp parallel for reduction(+:norm) schedule(static)
#endif
    for (long long i = 0; i < dim; ++i) {
      if (i & stride)
        state[i] = 0.0;
      else
        norm += std::norm(state[i]);
    }
  } else {
#ifdef _OPENMP
#pragma omp parallel for reduction(+:norm) schedule(static)
#endif
    for (long long i = 0; i < dim; ++i) {
      if (!(i & stride))
        state[i] = 0.0;
      else
        norm += std::norm(state[i]);
    }
  }
  norm = std::sqrt(norm);
  if (norm > 1e-9) {
    // Parallel renormalization
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (long long i = 0; i < dim; ++i)
      state[i] /= norm;
  }
  return outcome;
}

std::vector<double> CpuBackend::getProbabilities() const {
  std::vector<double> probs(state.size(), 0.0);
  for (size_t i = 0; i < state.size(); ++i) {
    probs[i] = std::norm(state[i]);
  }
  return probs;
}

double CpuBackend::expectationValue(const std::string &pauli_string) const {
  Precision partial_expected_value = 0.0;
  size_t local_dim = state.size();

    // 1. Identify if this Pauli term requires inter-rank communication
    size_t global_flip_mask = 0;
    for (size_t q = 0; q < num_qubits && q < pauli_string.size(); ++q) {
        if ((pauli_string[q] == 'X' || pauli_string[q] == 'Y') && (1ULL << q) >= local_dim) {
            global_flip_mask |= (1ULL << q);
        }
    }

    const Complex* target_data = state.data();
    std::vector<Complex> recv_buf;

#ifdef MPI_ENABLED
    if (global_flip_mask != 0 && world_size > 1) {
        int partner = local_rank ^ (global_flip_mask / local_dim);
        recv_buf.resize(local_dim);
        MPI_Sendrecv(const_cast<Complex*>(state.data()), local_dim * 2, get_mpi_precision_type(), partner, 0,
                     recv_buf.data(), local_dim * 2, get_mpi_precision_type(), partner, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        target_data = recv_buf.data();
    }
#endif

    // 2. Compute partial expectation value
#ifdef _OPENMP
#pragma omp parallel for reduction(+:partial_expected_value) schedule(static)
#endif
    for (long long i = 0; i < static_cast<long long>(local_dim); ++i) {
      size_t global_idx = i + (local_rank * local_dim);
      size_t j_global = global_idx;
      Complex coeff = 1.0;

      for (size_t q = 0; q < num_qubits && q < pauli_string.size(); ++q) {
        char op = pauli_string[q];
        if (op == 'I') continue;

        bool bit_set = (global_idx >> q) & 1;
        if (op == 'X') {
          j_global ^= (1ULL << q);
        } else if (op == 'Y') {
          j_global ^= (1ULL << q);
          coeff *= (bit_set ? Complex(0, -1) : Complex(0, 1));
        } else if (op == 'Z') {
          if (bit_set) coeff *= -1.0;
        }
      }

      size_t j_local = j_global % local_dim;
      // We now have the correct state[j] either in 'state' or 'recv_buf'
      partial_expected_value += (std::conj(state[i]) * coeff * target_data[j_local]).real();
    }

  double total_expected_value = (double)partial_expected_value;

#ifdef MPI_ENABLED
  if (world_size > 1) {
    double global_sum = 0.0;
    MPI_Allreduce(&total_expected_value, &global_sum, 1, MPI_DOUBLE, MPI_SUM,
                  MPI_COMM_WORLD);
    total_expected_value = global_sum;
  }
#endif

  return total_expected_value;
}

std::vector<Complex> CpuBackend::getStateVector() const {
  return std::vector<Complex>(state.begin(), state.end());
}

void CpuBackend::applyDenseUnitary(const std::vector<size_t> &targets,
                                   const std::vector<Complex> &matrix) {
  size_t local_dim = state.size();
  if (targets.size() == 1) {
    size_t t0 = targets[0];
    size_t stride = 1ULL << t0;
    if (stride >= local_dim) {
      throw std::runtime_error("applyDenseUnitary not implemented for global qubits in CPU backend.");
    }

#pragma omp parallel for
    for (long long i = 0; i < static_cast<long long>(local_dim); i += 2 * stride) {
      for (size_t j = 0; j < stride; ++j) {
        size_t idx0 = i + j;
        size_t idx1 = i + j + stride;
        Complex a = state[idx0];
        Complex b = state[idx1];
        state[idx0] = matrix[0] * a + matrix[1] * b;
        state[idx1] = matrix[2] * a + matrix[3] * b;
      }
    }
  } else if (targets.size() == 2) {
    size_t q0 = targets[0];
    size_t q1 = targets[1];
    size_t m0 = 1ULL << q0;
    size_t m1 = 1ULL << q1;
    if (m0 >= local_dim || m1 >= local_dim) {
      throw std::runtime_error("applyDenseUnitary not implemented for global qubits in CPU backend.");
    }

#pragma omp parallel for
    for (long long i = 0; i < static_cast<long long>(local_dim); ++i) {
      if (!(i & m0) && !(i & m1)) {
        size_t i00 = i;
        size_t i01 = i | m0; // Index 1: targets[0] set
        size_t i10 = i | m1; // Index 2: targets[1] set
        size_t i11 = i | m0 | m1;

        Complex v00 = state[i00];
        Complex v01 = state[i01];
        Complex v10 = state[i10];
        Complex v11 = state[i11];

        state[i00] = matrix[0] * v00 + matrix[1] * v01 + matrix[2] * v10 + matrix[3] * v11;
        state[i01] = matrix[4] * v00 + matrix[5] * v01 + matrix[6] * v10 + matrix[7] * v11;
        state[i10] = matrix[8] * v00 + matrix[9] * v01 + matrix[10] * v10 + matrix[11] * v11;
        state[i11] = matrix[12] * v00 + matrix[13] * v01 + matrix[14] * v10 + matrix[15] * v11;
      }
    }
  } else {
    throw std::runtime_error("applyDenseUnitary only implemented for 1 or 2 qubits in CPU backend currently.");
  }
}

} // namespace qubit_engine
