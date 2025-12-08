#pragma once
#include <vector>
#include <complex>
#include <iostream>

using Complex = std::complex<double>;

class QuantumRegister {
private:
    std::vector<Complex> state;
    int num_qubits;

public:
    explicit QuantumRegister(int n);
    
    // Core Gates
    void applyHadamard(int target_qubit);
    void applyX(int target_qubit);
    void applyCNOT(int control_qubit, int target_qubit);
    
    // Utilities
    const std::vector<Complex>& getStateVector() const;
    void printState() const;
};
