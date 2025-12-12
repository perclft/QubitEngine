#include "QuantumRegister.hpp"
#include <algorithm>
#include <cmath>
#include <omp.h>
#include <random>
#include <stdexcept>
// --- AVX2 SIMD Intrinsics Header ---
#include <immintrin.h>
// ------------------------------------

const double INV_SQRT_2 = 1.0 / std::sqrt(2.0);

QuantumRegister::QuantumRegister(size_t n) : num_qubits(n) {
  size_t dim = 1ULL << n;
  state.resize(dim, 0.0);
  state[0] = 1.0;
}

bool QuantumRegister::measure(size_t target) {
  validateIndex(target);

  double prob_one = 0.0;
  size_t limit = state.size();
  size_t stride = 1ULL << target;

// 1. Calculate Probability of |1>
#pragma omp parallel for reduction(+ : prob_one)
  for (size_t i = stride; i < limit; i += 2 * stride) {
    for (size_t j = i; j < i + stride; ++j) {
      prob_one += std::norm(state[j]);
    }
  }

  // 2. Roll the Quantum Die (THREAD-SAFE FIX)
  // We use thread_local to ensure every execution thread has its own isolated
  // generator. This prevents race conditions during high-concurrency gRPC
  // requests.
  thread_local std::mt19937 gen(std::random_device{}());

  std::bernoulli_distribution d(prob_one);
  bool outcome = d(gen); // true = 1, false = 0

  // 3. Collapse the Wavefunction
  double norm_factor = 1.0 / std::sqrt(outcome ? prob_one : (1.0 - prob_one));

#pragma omp parallel for
  for (size_t i = 0; i < limit; i += 2 * stride) {
    for (size_t j = i; j < i + stride; ++j) {
      size_t k = j + stride;

      if (outcome) {
        state[j] = 0.0;
        state[k] *= norm_factor;
      } else {
        state[j] *= norm_factor;
        state[k] = 0.0;
      }
    }
  }

  return outcome;
}

void QuantumRegister::applyHadamard(size_t target) {
  validateIndex(target);
  size_t limit = state.size();
  size_t stride = 1ULL << target;
  const __m256d inv_sqrt2_vec = _mm256_set1_pd(INV_SQRT_2);

#pragma omp parallel for
  for (size_t i = 0; i < limit; i += 2 * stride) {
    for (size_t j = i; j < i + stride; j += 4) {
      if (j + 4 > i + stride) {
        for (size_t k_scalar = j; k_scalar < i + stride; ++k_scalar) {
          size_t k = k_scalar + stride;
          Complex a = state[k_scalar];
          Complex b = state[k];
          state[k_scalar] = (a + b) * INV_SQRT_2;
          state[k] = (a - b) * INV_SQRT_2;
        }
        break;
      }
      __m256d s0_vec_low =
          _mm256_loadu_pd(reinterpret_cast<const double *>(&state[j]));
      __m256d s0_vec_high =
          _mm256_loadu_pd(reinterpret_cast<const double *>(&state[j + 2]));
      __m256d s1_vec_low =
          _mm256_loadu_pd(reinterpret_cast<const double *>(&state[j + stride]));
      __m256d s1_vec_high = _mm256_loadu_pd(
          reinterpret_cast<const double *>(&state[j + stride + 2]));
      __m256d sum_low = _mm256_add_pd(s0_vec_low, s1_vec_low);
      __m256d diff_low = _mm256_sub_pd(s0_vec_low, s1_vec_low);
      __m256d sum_high = _mm256_add_pd(s0_vec_high, s1_vec_high);
      __m256d diff_high = _mm256_sub_pd(s0_vec_high, s1_vec_high);
      __m256d s0_new_low = _mm256_mul_pd(sum_low, inv_sqrt2_vec);
      __m256d s0_new_high = _mm256_mul_pd(sum_high, inv_sqrt2_vec);
      __m256d s1_new_low = _mm256_mul_pd(diff_low, inv_sqrt2_vec);
      __m256d s1_new_high = _mm256_mul_pd(diff_high, inv_sqrt2_vec);
      _mm256_storeu_pd(reinterpret_cast<double *>(&state[j]), s0_new_low);
      _mm256_storeu_pd(reinterpret_cast<double *>(&state[j + 2]), s0_new_high);
      _mm256_storeu_pd(reinterpret_cast<double *>(&state[j + stride]),
                       s1_new_low);
      _mm256_storeu_pd(reinterpret_cast<double *>(&state[j + stride + 2]),
                       s1_new_high);
    }
  }
}

void QuantumRegister::applyX(size_t target) {
  validateIndex(target);
  size_t limit = state.size();
  size_t stride = 1ULL << target;
#pragma omp parallel for
  for (size_t i = 0; i < limit; i += 2 * stride) {
    for (size_t j = i; j < i + stride; ++j) {
      size_t k = j + stride;
      std::swap(state[j], state[k]);
    }
  }
}

void QuantumRegister::applyCNOT(size_t control, size_t target) {
  validateIndex(control);
  validateIndex(target);
  if (control == target)
    throw std::invalid_argument("Control cannot equal target");
  size_t limit = state.size();
  size_t control_mask = 1ULL << control;
  size_t target_mask = 1ULL << target;
#pragma omp parallel for
  for (size_t i = 0; i < limit; ++i) {
    if ((i & control_mask) && !(i & target_mask)) {
      size_t j = i | target_mask;
      std::swap(state[i], state[j]);
    }
  }
}

// -----------------------------------------------------------------
// Phase 3: Extended Gate Set
// -----------------------------------------------------------------

void QuantumRegister::applyToffoli(size_t c1, size_t c2, size_t target) {
  validateIndex(c1);
  validateIndex(c2);
  validateIndex(target);
  if (c1 == c2 || c1 == target || c2 == target)
    throw std::invalid_argument("Qubits must be unique");

  size_t limit = state.size();
  size_t c1_mask = 1ULL << c1;
  size_t c2_mask = 1ULL << c2;
  size_t t_mask = 1ULL << target;

#pragma omp parallel for
  for (size_t i = 0; i < limit; ++i) {
    // Swap ONLY if both controls are 1 AND target is 0
    if ((i & c1_mask) && (i & c2_mask) && !(i & t_mask)) {
      size_t j = i | t_mask;
      std::swap(state[i], state[j]);
    }
  }
}

void QuantumRegister::applyPhaseS(size_t target) {
  validateIndex(target);
  size_t limit = state.size();
  size_t mask = 1ULL << target;
  const Complex i_unit(0, 1);

#pragma omp parallel for
  for (size_t k = 0; k < limit; ++k) {
    if (k & mask) {
      state[k] *= i_unit;
    }
  }
}

void QuantumRegister::applyPhaseT(size_t target) {
  validateIndex(target);
  size_t limit = state.size();
  size_t mask = 1ULL << target;
  // T = exp(i * pi/4) = cos(pi/4) + i*sin(pi/4) = 1/sqrt(2) + i/sqrt(2)
  const Complex t_val(INV_SQRT_2, INV_SQRT_2);

#pragma omp parallel for
  for (size_t k = 0; k < limit; ++k) {
    if (k & mask) {
      state[k] *= t_val;
    }
  }
}

void QuantumRegister::applyRotationY(size_t target, double angle) {
  validateIndex(target);
  size_t limit = state.size();
  size_t stride = 1ULL << target;

  double c = std::cos(angle / 2.0);
  double s = std::sin(angle / 2.0); // Ry matrix involves sin(theta/2)

#pragma omp parallel for
  for (size_t i = 0; i < limit; i += 2 * stride) {
    for (size_t j = i; j < i + stride; ++j) {
      size_t k = j + stride;

      // |0> -> cos(a/2)|0> + sin(a/2)|1>
      // |1> -> -sin(a/2)|0> + cos(a/2)|1>
      Complex val0 = state[j];
      Complex val1 = state[k];

      state[j] = c * val0 - s * val1;
      state[k] = s * val0 + c * val1;
    }
  }
}

void QuantumRegister::applyRotationZ(size_t target, double angle) {
  validateIndex(target);
  size_t limit = state.size();
  size_t mask = 1ULL << target;

  // Rz(theta) = diag(exp(-i*theta/2), exp(i*theta/2))
  // Note: Global phase is often ignored, but strict definition requires this.
  Complex phase0 = std::exp(Complex(0, -angle / 2.0));
  Complex phase1 = std::exp(Complex(0, angle / 2.0));

#pragma omp parallel for
  for (size_t k = 0; k < limit; ++k) {
    if (k & mask) {
      state[k] *= phase1;
    } else {
      state[k] *= phase0;
    }
  }
}

const std::vector<Complex> &QuantumRegister::getStateVector() const {
  return state;
}
void QuantumRegister::applyDepolarizingNoise(double p) {
  if (p <= 0.0)
    return;

  // We iterate through each qubit and probabilistically apply a Pauli error.
  // This simulates a "Depolarizing Channel" in a Monte Carlo wavefunction
  // trajectory.
  thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<> dis(0.0, 1.0);

  for (size_t q = 0; q < num_qubits; ++q) {
    if (dis(gen) < p) {
      // Error Occurred!
      // 1/3 chance of X, Y, or Z
      double type = dis(gen);
      if (type < 0.3333) {
        applyX(q);
      } else if (type < 0.6666) {
        // Y = i * X * Z. We can just use RotationY(pi) which is Y (ignoring
        // global phase)
        applyRotationY(q, M_PI);
      } else {
        // Z
        applyRotationZ(q, M_PI);
      }
    }
  }
}

// Phase 19: VQE Expectation Value
double QuantumRegister::expectationValue(const std::string &pauli) {
  if (pauli.length() != num_qubits) {
    throw std::invalid_argument(
        "Pauli string length must match number of qubits");
  }

  double total_expectation = 0.0;
  size_t dim = state.size();

#pragma omp parallel for reduction(+ : total_expectation)
  for (size_t i = 0; i < dim; ++i) {
    size_t j = i;
    Complex phase = 1.0;

    for (size_t q = 0; q < num_qubits; ++q) {
      char op = pauli[q];
      bool bit_set = (i >> q) & 1;

      if (op == 'X') {
        j ^= (1ULL << q);
      } else if (op == 'Y') {
        j ^= (1ULL << q);
        phase *= (bit_set ? Complex(0, -1) : Complex(0, 1));
      } else if (op == 'Z') {
        if (bit_set)
          phase *= -1.0;
      }
    }

    Complex contribution = std::conj(state[i]) * phase * state[j];
    total_expectation += contribution.real();
  }

  return total_expectation;
}
