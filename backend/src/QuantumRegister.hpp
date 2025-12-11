#pragma once
#include <vector>
#include <complex>
#include <stdexcept> // For std::out_of_range

using Complex = std::complex<double>;

class QuantumRegister {
private:
    std::vector<Complex> state;
    size_t num_qubits; // Changed to size_t

    // Helper to validate indices
    void validateIndex(size_t index) const {
        if (index >= num_qubits) {
            throw std::out_of_range("Qubit index out of bounds");
        }
    }

public:
    explicit QuantumRegister(size_t n);
    
    void applyHadamard(size_t target_qubit) __attribute__((target("avx2,fma")));
    void applyX(size_t target_qubit);
    void applyCNOT(size_t control_qubit, size_t target_qubit);
    bool measure(size_t target_qubit);
    
    const std::vector<Complex>& getStateVector() const;
};
