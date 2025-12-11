#include "QuantumRegister.hpp"
#include <cmath>
#include <algorithm>
#include <omp.h>
#include <stdexcept>
#include <random>
// --- AVX2 SIMD Intrinsics Header ---
#include <immintrin.h>
// ------------------------------------

const double INV_SQRT_2 = 1.0 / std::sqrt(2.0);

// Assuming Complex is defined as std::complex<double> in your header
// If not, you must define it here or in the header:
// using Complex = std::complex<double>; 

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
    // Sum amplitudes where the target bit is 1
    #pragma omp parallel for reduction(+:prob_one)
    for (size_t i = stride; i < limit; i += 2 * stride) {
        for (size_t j = i; j < i + stride; ++j) {
            prob_one += std::norm(state[j]); // |amp|^2
        }
    }

    // 2. Roll the Quantum Die
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::bernoulli_distribution d(prob_one);
    bool outcome = d(gen); // true = 1, false = 0

    // 3. Collapse the Wavefunction (Non-Unitary Operation)
    double norm_factor = 1.0 / std::sqrt(outcome ? prob_one : (1.0 - prob_one));

    #pragma omp parallel for
    for (size_t i = 0; i < limit; i += 2 * stride) {
        for (size_t j = i; j < i + stride; ++j) {
            size_t k = j + stride; // Index where bit is 1
            
            if (outcome) {
                // Measured 1: Kill 0-states, boost 1-states
                state[j] = 0.0;
                state[k] *= norm_factor;
            } else {
                // Measured 0: Boost 0-states, kill 1-states
                state[j] *= norm_factor;
                state[k] = 0.0;
            }
        }
    }

    return outcome;
}

// -----------------------------------------------------------------
// 🎯 AVX2 + OpenMP Implementation for Hadamard Gate
// -----------------------------------------------------------------
void QuantumRegister::applyHadamard(size_t target) {
    validateIndex(target);
    size_t limit = state.size();
    size_t stride = 1ULL << target;
    
    // Load constant 1/sqrt(2) into an AVX2 register, duplicated across 4 doubles
    const __m256d inv_sqrt2_vec = _mm256_set1_pd(INV_SQRT_2);

    #pragma omp parallel for 
    for (size_t i = 0; i < limit; i += 2 * stride) {
        
        // Inner loop: Vectorize in blocks of 4 complex numbers (8 doubles)
        for (size_t j = i; j < i + stride; j += 4) {
            
            // --- SCALAR TAIL HANDLING (for unaligned end of segment) ---
            if (j + 4 > i + stride) {
                for (size_t k_scalar = j; k_scalar < i + stride; ++k_scalar) {
                    size_t k = k_scalar + stride;
                    Complex a = state[k_scalar];
                    Complex b = state[k];
                    state[k_scalar] = (a + b) * INV_SQRT_2;
                    state[k] = (a - b) * INV_SQRT_2;
                }
                break; // Exit the inner SIMD loop
            }

            // --- AVX2 Core Block: Process 4 Complex Numbers ---

            // Load 4 complex numbers (8 doubles) for the '0' side (j)
            __m256d s0_vec_low = _mm256_loadu_pd(reinterpret_cast<const double*>(&state[j]));
            __m256d s0_vec_high = _mm256_loadu_pd(reinterpret_cast<const double*>(&state[j+2]));
            
            // Load 4 complex numbers (8 doubles) for the '1' side (j + stride)
            __m256d s1_vec_low = _mm256_loadu_pd(reinterpret_cast<const double*>(&state[j + stride]));
            __m256d s1_vec_high = _mm256_loadu_pd(reinterpret_cast<const double*>(&state[j + stride + 2]));

            // Calculate Sums and Differences
            __m256d sum_low = _mm256_add_pd(s0_vec_low, s1_vec_low);
            __m256d diff_low = _mm256_sub_pd(s0_vec_low, s1_vec_low);
            __m256d sum_high = _mm256_add_pd(s0_vec_high, s1_vec_high);
            __m256d diff_high = _mm256_sub_pd(s0_vec_high, s1_vec_high);

            // Apply 1/sqrt(2) multiplication (FMA is implicitly used here by the compiler with -mfma)
            __m256d s0_new_low = _mm256_mul_pd(sum_low, inv_sqrt2_vec);
            __m256d s0_new_high = _mm256_mul_pd(sum_high, inv_sqrt2_vec);
            __m256d s1_new_low = _mm256_mul_pd(diff_low, inv_sqrt2_vec);
            __m256d s1_new_high = _mm256_mul_pd(diff_high, inv_sqrt2_vec);
            
            // Store results back to memory
            _mm256_storeu_pd(reinterpret_cast<double*>(&state[j]), s0_new_low);
            _mm256_storeu_pd(reinterpret_cast<double*>(&state[j + 2]), s0_new_high);
            _mm256_storeu_pd(reinterpret_cast<double*>(&state[j + stride]), s1_new_low);
            _mm256_storeu_pd(reinterpret_cast<double*>(&state[j + stride + 2]), s1_new_high);
        }
    }
}
// -----------------------------------------------------------------


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
    if (control == target) throw std::invalid_argument("Control cannot equal target");

    size_t limit = state.size();
    size_t control_mask = 1ULL << control;
    size_t target_mask = 1ULL << target;

    // Iterate through all states. 
    // We only swap pairs (i, j) where:
    // 1. Control bit is 1 for both
    // 2. Target bit is 0 for i and 1 for j
    #pragma omp parallel for
    for (size_t i = 0; i < limit; ++i) {
        // Check if this index acts as the '0' side of the target pair AND has control bit set
        if ((i & control_mask) && !(i & target_mask)) {
            size_t j = i | target_mask; // The corresponding state with target bit 1
            std::swap(state[i], state[j]);
        }
    }
}

const std::vector<Complex>& QuantumRegister::getStateVector() const {
    return state;
}
