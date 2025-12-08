#include "QuantumRegister.hpp"
#include <cmath>
#include <omp.h>

const double INV_SQRT_2 = 1.0 / std::sqrt(2.0);

QuantumRegister::QuantumRegister(int n) : num_qubits(n) {
    size_t dim = 1ULL << n;
    state.resize(dim, 0.0);
    state[0] = 1.0; // Initialize |0...0>
}

void QuantumRegister::applyHadamard(int target) {
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

void QuantumRegister::applyX(int target) {
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

void QuantumRegister::applyCNOT(int control, int target) {
    size_t limit = state.size();
    size_t control_mask = 1ULL << control;
    size_t target_mask = 1ULL << target;
    
    // Logic: If control bit is 1, flip the target bit (Swap indices)
    // We iterate through all states, find those where control=1 and target=0, 
    // and swap them with the corresponding state where target=1.
    
    #pragma omp parallel for
    for (size_t i = 0; i < limit; ++i) {
        // We only process the "lower" index of the swap pair to avoid double swapping
        // Check if control is 1 AND target is 0
        if ((i & control_mask) && !(i & target_mask)) {
            size_t partner = i | target_mask;
            std::swap(state[i], state[partner]);
        }
    }
}

const std::vector<Complex>& QuantumRegister::getStateVector() const {
    return state;
}

void QuantumRegister::printState() const {
    for (const auto& val : state) {
        std::cout << val << "\n";
    }
}
