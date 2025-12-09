#include "QuantumRegister.hpp"
#include <cmath>
#include <algorithm> // For std::swap
#include <omp.h>

const double INV_SQRT_2 = 1.0 / std::sqrt(2.0);

QuantumRegister::QuantumRegister(size_t n) : num_qubits(n) {
    size_t dim = 1ULL << n;
    state.resize(dim, 0.0);
    state[0] = 1.0;
}

void QuantumRegister::applyHadamard(size_t target) {
    validateIndex(target); // FIX: Safety check
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

// FIX: Optimized CNOT using Block/Stride pattern
void QuantumRegister::applyCNOT(size_t control, size_t target) {
    validateIndex(control);
    validateIndex(target);
    if (control == target) throw std::invalid_argument("Control cannot equal target");

    size_t limit = state.size();
    
    // We need to iterate over pairs where target bit is 0/1
    // BUT only swap if control bit is 1.
    
    size_t target_stride = 1ULL << target;
    size_t control_mask = 1ULL << control;

    // To avoid complex bit logic in the loop, we can just iterate the standard
    // "target" blocks, and inside check the control bit.
    
    #pragma omp parallel for
    for (size_t i = 0; i < limit; i += 2 * target_stride) {
        for (size_t j = i; j < i + target_stride; ++j) {
            // j has target bit 0
            // k has target bit 1
            size_t k = j + target_stride;

            // Only act if control bit is set (1)
            // Since j and k only differ by target bit, they have SAME control bit
            if (j & control_mask) {
                std::swap(state[j], state[k]);
            }
        }
    }
}

const std::vector<Complex>& QuantumRegister::getStateVector() const {
    return state;
}
