#pragma once

#include <vector>
#include <complex>
#include <cstddef>
#include <string>

// --- Fix 1: Global Type Alias (Crucial for ServiceImpl_Visualize) ---
using Complex = std::complex<double>;

class QuantumRegister {
public:
    // --- Lifecycle ---
    QuantumRegister(size_t n);
    ~QuantumRegister();

    // --- Core Gates ---
    void applyHadamard(size_t target);
    void applyX(size_t target);
    void applyY(size_t target);
    void applyZ(size_t target);
    void applyCNOT(size_t control, size_t target);

    // --- Advanced Gates ---
    void applyToffoli(size_t control1, size_t control2, size_t target);
    void applyPhaseS(size_t target);
    void applyPhaseT(size_t target);
    void applyRotationY(size_t target, double angle);
    void applyRotationZ(size_t target, double angle);

    // --- Fix 2: Noise Simulation (Restored) ---
    void applyDepolarizingNoise(double probability);

    // --- Measurement & Analysis ---
    int measure(size_t target);
    std::vector<double> getProbabilities();
    double expectationValue(const std::string& pauli_string);

    // --- Distributed Helpers ---
    int getRank() const { return local_rank; }
    int getSize() const { return world_size; }

    // --- Debugging ---
    std::vector<Complex> getStateVector() const;

private:
    size_t num_qubits;
    std::vector<Complex> state;

    // Distributed State
    int local_rank;
    int world_size;
};
