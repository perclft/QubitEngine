#include "QuantumRegister.hpp"
#include <cmath>
#include <algorithm>
#include <omp.h>
#include <stdexcept>
#include <random>

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
void QuantumRegister::applyHadamard(size_t target) {
    validateIndex(target);
    size_t limit = state.size();
    size_t stride = 1ULL << target;

    #pragma omp parallel for
    for (size_t i = 0; i < limit; i += 2 * stride) {
        for (size_t j = i; j < i + stride; ++j) {
            size_t k = j + stride;
            Complex a = state[j];
            Complex b = state[k];
            state[j] = (a + b) * INV_SQRT_2;
            state[k] = (a - b) * INV_SQRT_2;
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
