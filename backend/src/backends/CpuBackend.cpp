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

static const double INV_SQRT_2 = 1.0 / std::sqrt(2.0);
using Complex = std::complex<double>;

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
    // ARM NEON implementation for Apple Silicon
    float64x2_t v_inv_sqrt2 = vdupq_n_f64(INV_SQRT_2);

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        // Load complex numbers (each is 2 doubles: real, imag)
        double *ptr_a = reinterpret_cast<double *>(&state[j]);
        double *ptr_b = reinterpret_cast<double *>(&state[j + stride]);

        float64x2_t v_a = vld1q_f64(ptr_a); // Load complex a
        float64x2_t v_b = vld1q_f64(ptr_b); // Load complex b

        // (a + b) * inv_sqrt2
        float64x2_t v_sum = vaddq_f64(v_a, v_b);
        v_sum = vmulq_f64(v_sum, v_inv_sqrt2);

        // (a - b) * inv_sqrt2
        float64x2_t v_diff = vsubq_f64(v_a, v_b);
        v_diff = vmulq_f64(v_diff, v_inv_sqrt2);

        vst1q_f64(ptr_a, v_sum);
        vst1q_f64(ptr_b, v_diff);
      }
    }
#elif defined(USE_AVX2_INTRINSICS) && defined(__AVX2__)
    __m256d v_inv_sqrt2 = _mm256_set1_pd(INV_SQRT_2);
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      size_t j = i;
      // Process 2 * 2 doubles (4 doubles) at a time if stride >= 2?
      // simplified loop structure for safety if stride is small
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
#else
    // Scalar fallback with OpenMP
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        Complex a = state[j];
        Complex b = state[j + stride];
        state[j] = (a + b) * INV_SQRT_2;
        state[j + stride] = (a - b) * INV_SQRT_2;
      }
    }
#endif
  } else {
#ifdef MPI_ENABLED
    size_t rank_bit = stride / local_dim;
    bool is_one = (local_rank & rank_bit);
    int partner = local_rank ^ rank_bit;
    std::vector<Complex> recv_buf(local_dim);

    MPI_Sendrecv(state.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                 recv_buf.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    if (is_one) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
      for (size_t i = 0; i < local_dim; ++i) {
        state[i] = (recv_buf[i] - state[i]) * INV_SQRT_2;
      }
    } else {
#ifdef _OPENMP
#pragma omp parallel for
#endif
      for (size_t i = 0; i < local_dim; ++i) {
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
#if defined(USE_AVX2_INTRINSICS) && defined(__AVX2__)
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      size_t j = i;
      if (stride >= 2) {
        for (; j + 1 < i + stride; j += 2) {
          double *ptr_a = reinterpret_cast<double *>(&state[j]);
          double *ptr_b = reinterpret_cast<double *>(&state[j + stride]);
          __m256d v_a = _mm256_loadu_pd(ptr_a);
          __m256d v_b = _mm256_loadu_pd(ptr_b);
          _mm256_storeu_pd(ptr_a, v_b);
          _mm256_storeu_pd(ptr_b, v_a);
        }
      }
      for (; j < i + stride; ++j)
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
    size_t rank_bit = stride / local_dim;
    int partner = local_rank ^ rank_bit;
    std::vector<Complex> recv_buf(local_dim);
    MPI_Sendrecv(state.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                 recv_buf.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
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
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        Complex a = state[j];
        Complex b = state[j + stride];
        state[j] = -i_unit * b;
        state[j + stride] = i_unit * a;
      }
    }
  } else {
#ifdef MPI_ENABLED
    size_t rank_bit = stride / local_dim;
    bool is_one = (local_rank & rank_bit);
    int partner = local_rank ^ rank_bit;
    std::vector<Complex> recv_buf(local_dim);
    MPI_Sendrecv(state.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                 recv_buf.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (is_one) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
      for (size_t i = 0; i < local_dim; ++i)
        state[i] = i_unit * recv_buf[i];
    } else {
#ifdef _OPENMP
#pragma omp parallel for
#endif
      for (size_t i = 0; i < local_dim; ++i)
        state[i] = -i_unit * recv_buf[i];
    }
#else
    std::cerr << "Error: Global Y requested but MPI not enabled." << std::endl;
#endif
  }
}

void CpuBackend::applyZ(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;
  if (stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
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
      for (size_t i = 0; i < local_dim; ++i)
        state[i] *= -1.0;
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
    for (size_t i = 0; i < local_dim; ++i) {
      if ((i & c_stride)) {
        size_t partner = i ^ t_stride;
        if (i < partner) {
          std::swap(state[i], state[partner]);
        }
      }
    }
    return;
  }

#ifdef MPI_ENABLED
  int rank = local_rank;
  if (c_is_global) {
    size_t rank_c_bit = c_stride / local_dim;
    bool control_set = (rank & rank_c_bit);

    if (control_set) {
      if (t_is_global) {
        size_t rank_t_bit = t_stride / local_dim;
        int partner = rank ^ rank_t_bit;
        std::vector<Complex> recv_buf(local_dim);
        MPI_Sendrecv(state.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                     recv_buf.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        state = recv_buf;
      } else {
        applyX(target);
      }
    }
    return;
  }

  if (t_is_global) {
    size_t rank_t_bit = t_stride / local_dim;
    int partner = rank ^ rank_t_bit;
    std::vector<Complex> recv_buf(local_dim);
    MPI_Sendrecv(state.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                 recv_buf.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (size_t i = 0; i < local_dim; ++i) {
      if (i & c_stride) {
        state[i] = recv_buf[i];
      } else {
      }
    }
  }
#else
  if (c_is_global || t_is_global) {
    std::cerr << "Error: Global CNOT requested but MPI not enabled."
              << std::endl;
  }
#endif
}

// --- Advanced Gates ---

void CpuBackend::applyToffoli(size_t c1, size_t c2, size_t t) {
  size_t local_dim = state.size();
  size_t c1_s = 1ULL << c1;
  size_t c2_s = 1ULL << c2;
  size_t t_s = 1ULL << t;

  // Assuming local for MVP
  if (t_s < local_dim && c1_s < local_dim && c2_s < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (size_t i = 0; i < local_dim; ++i) {
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
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        state[j + stride] *= i_unit;
      }
    }
  }
}

void CpuBackend::applyPhaseT(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;
  Complex phase(1.0 / std::sqrt(2.0), 1.0 / std::sqrt(2.0));

  if (stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        state[j + stride] *= phase;
      }
    }
  }
}

void CpuBackend::applyRotationY(size_t target, double angle) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  double c = std::cos(angle / 2.0);
  double s = std::sin(angle / 2.0);

  if (stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        Complex a = state[j];
        Complex b = state[j + stride];
        state[j] = c * a - s * b;
        state[j + stride] = s * a + c * b;
      }
    }
  }
}

void CpuBackend::applyRotationZ(size_t target, double angle) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  Complex z0(std::cos(-angle / 2.0), std::sin(-angle / 2.0));
  Complex z1(std::cos(angle / 2.0), std::sin(angle / 2.0));

  if (stride < local_dim) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        state[j] *= z0;
        state[j + stride] *= z1;
      }
    }
  }
}

void CpuBackend::applyDepolarizingNoise(double probability) {
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
  double prob0 = 0.0;
  size_t stride = 1ULL << target;
  for (size_t i = 0; i < state.size(); ++i) {
    if (!(i & stride))
      prob0 += std::norm(state[i]);
  }

  thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<> dis(0.0, 1.0);
  int outcome = (dis(gen) > prob0) ? 1 : 0;

  double norm = 0.0;
  if (outcome == 0) {
    for (size_t i = 0; i < state.size(); ++i) {
      if (i & stride)
        state[i] = 0.0;
      else
        norm += std::norm(state[i]);
    }
  } else {
    for (size_t i = 0; i < state.size(); ++i) {
      if (!(i & stride))
        state[i] = 0.0;
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
  return {}; // Not implemented
}

double CpuBackend::expectationValue(const std::string &pauli_string) {
  double expected_value = 0.0;
  // #pragma omp parallel for reduction(+:expected_value)
  for (size_t i = 0; i < state.size(); ++i) {
    double prob = std::norm(state[i]);
    if (prob < 1e-15)
      continue;

    int sign = 1;
    for (size_t q = 0; q < num_qubits && q < pauli_string.size(); ++q) {
      char op = pauli_string[q];
      if (op == 'Z') {
        if ((i >> q) & 1)
          sign *= -1;
      }
    }
    expected_value += prob * sign;
  }
  return expected_value;
}

std::vector<std::complex<double>> CpuBackend::getStateVector() const {
  return state;
}

} // namespace qubit_engine
