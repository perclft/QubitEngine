#include "StabilizerBackend.hpp"
#include "../Exceptions.hpp"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>


namespace qubit_engine {

StabilizerBackend::StabilizerBackend(size_t num_qubits)
    : num_qubits_(num_qubits) {
  // Initialize standard tableau for |0...0> state.
  // Dimensions are 2N rows by (2N + 1) columns:
  // Rows 0 to N-1: Destabilizers (Xi)
  // Rows N to 2N-1: Stabilizers (Zi)
  // Columns 0 to N-1: X components
  // Columns N to 2N-1: Z components
  // Column 2N: sign (r) phase bit (0 for +1, 1 for -1)

  // Also allocate a 2N+1 row as a scratch space for measurement.
  size_t rows = 2 * num_qubits_ + 1;
  size_t cols = 2 * num_qubits_ + 1;
  tableau_ =
      std::vector<std::vector<bool>>(rows, std::vector<bool>(cols, false));

  for (size_t i = 0; i < num_qubits_; ++i) {
    // Initial destabilizers: X_i
    tableau_[i][i] = true;
    // Initial stabilizers: Z_i
    tableau_[i + num_qubits_][i + num_qubits_] = true;
  }
}

int StabilizerBackend::measure(size_t target) {
  int p = -1;
  for (size_t i = num_qubits_; i < 2 * num_qubits_; ++i) {
    if (tableau_[i][target]) {
      p = i;
      break;
    }
  }

  if (p != -1) {
    // Random outcome branch
    for (size_t i = 0; i < 2 * num_qubits_; ++i) {
      if (i != static_cast<size_t>(p) && tableau_[i][target]) {
        rowSum(i, p);
      }
    }
    // Set destabilizer (p - N) to stabilizer p
    tableau_[p - num_qubits_] = tableau_[p];

    // Set stabilizer p to Z_target
    std::fill(tableau_[p].begin(), tableau_[p].end(), false);
    tableau_[p][target + num_qubits_] = true;

    // Sample random outcome
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dis(0, 1);
    int outcome = dis(gen);
    
    tableau_[p][2 * num_qubits_] = (outcome == 1);
    return outcome;
  } else {
    // Deterministic outcome branch
    int scratch = 2 * num_qubits_;
    std::fill(tableau_[scratch].begin(), tableau_[scratch].end(), false);

    for (size_t i = 0; i < num_qubits_; ++i) {
      if (tableau_[i][target]) {
        rowSum(scratch, i + num_qubits_);
      }
    }
    return tableau_[scratch][2 * num_qubits_] ? 1 : 0;
  }
}

std::vector<double> StabilizerBackend::getProbabilities() const {
  throw std::runtime_error("Stabilizer Backend does not return flat dense "
                           "probability distributions efficiently.");
}

double StabilizerBackend::expectationValue(const std::string &pauli_string) const {
  throw std::runtime_error(
      "Expectation values pending implementation logic via algebraic tracing.");
}

std::vector<Complex> StabilizerBackend::getStateVector() const {
  throw std::runtime_error("WARNING: Extracting state vector from a Stabilizer "
                           "Backend scales as 2^N. Blocked for safety.");
}

// --- Internal Tableau Update Logic (Gottesman-Knill) ---
int StabilizerBackend::rowSum(int h, int i) {
    auto g = [](bool x1, bool z1, bool x2, bool z2) -> int {
        if (!x1 && !z1) return 0;
        if (x1 && z1) return (z2 ? 1 : 0) - (x2 ? 1 : 0);
        if (x1 && !z1) return z2 * (x2 ? 1 : -1);
        if (!x1 && z1) return x2 * (z2 ? -1 : 1);
        return 0;
    };

    int sum_term = 2 * tableau_[h][2 * num_qubits_] + 2 * tableau_[i][2 * num_qubits_];
    for (size_t j = 0; j < num_qubits_; ++j) {
        sum_term += g(tableau_[i][j], tableau_[i][j + num_qubits_],
                      tableau_[h][j], tableau_[h][j + num_qubits_]);
    }
    
    sum_term = (sum_term % 4 + 4) % 4; 
    tableau_[h][2 * num_qubits_] = (sum_term == 2);
    
    for (size_t j = 0; j < num_qubits_; ++j) {
        tableau_[h][j] = tableau_[i][j] ^ tableau_[h][j];
        tableau_[h][j + num_qubits_] = tableau_[i][j + num_qubits_] ^ tableau_[h][j + num_qubits_];
    }
    return 0;
}

// --- Core Clifford Gates ---

// Hadamard gate (H) swaps X and Z
void StabilizerBackend::applyHadamard(size_t target) {
  for (size_t i = 0; i < 2 * num_qubits_; ++i) {
    // r_i ^= x_i * z_i
    tableau_[i][2 * num_qubits_] =
        tableau_[i][2 * num_qubits_] ^
        (tableau_[i][target] & tableau_[i][target + num_qubits_]);
    // Swap x_i and z_i
    bool temp = tableau_[i][target];
    tableau_[i][target] = tableau_[i][target + num_qubits_];
    tableau_[i][target + num_qubits_] = temp;
  }
}

// Pauli-X (X) flips signs of Z stabilizers targeting it
void StabilizerBackend::applyX(size_t target) {
  for (size_t i = 0; i < 2 * num_qubits_; ++i) {
    tableau_[i][2 * num_qubits_] =
        tableau_[i][2 * num_qubits_] ^ tableau_[i][target + num_qubits_];
  }
}

// Pauli-Y (Y) = iXZ
void StabilizerBackend::applyY(size_t target) {
  applyZ(target);
  applyX(target);
}

// Pauli-Z (Z) flips signs of X stabilizers targeting it
void StabilizerBackend::applyZ(size_t target) {
  for (size_t i = 0; i < 2 * num_qubits_; ++i) {
    tableau_[i][2 * num_qubits_] =
        tableau_[i][2 * num_qubits_] ^ tableau_[i][target];
  }
}

// CNOT uses binary XOR logic connecting control to target rows
void StabilizerBackend::applyCNOT(size_t control, size_t target) {
  for (size_t i = 0; i < 2 * num_qubits_; ++i) {
    // r_i ^= x_{i, control} * z_{i, target} * (x_{i, target} ^ z_{i, control} ^
    // 1)
    bool r = tableau_[i][2 * num_qubits_];
    bool xc = tableau_[i][control];
    bool zc = tableau_[i][control + num_qubits_];
    bool xt = tableau_[i][target];
    bool zt = tableau_[i][target + num_qubits_];

    tableau_[i][2 * num_qubits_] = r ^ (xc & zt & ((xt ^ zc ^ true)));
    tableau_[i][target] = xt ^ xc;
    tableau_[i][control + num_qubits_] = zc ^ zt;
  }
}

// Phase (S) updates Z to ZX
void StabilizerBackend::applyPhaseS(size_t target) {
  for (size_t i = 0; i < 2 * num_qubits_; ++i) {
    tableau_[i][2 * num_qubits_] =
        tableau_[i][2 * num_qubits_] ^
        (tableau_[i][target] & tableau_[i][target + num_qubits_]);
    tableau_[i][target + num_qubits_] =
        tableau_[i][target + num_qubits_] ^ tableau_[i][target];
  }
}

// CZ = H(target) * CNOT(control, target) * H(target)
void StabilizerBackend::applyCZ(size_t control, size_t target) {
  applyHadamard(target);
  applyCNOT(control, target);
  applyHadamard(target);
}

// SWAP = CNOT(q1,q2) * CNOT(q2,q1) * CNOT(q1,q2)
void StabilizerBackend::applySWAP(size_t qubit1, size_t qubit2) {
  applyCNOT(qubit1, qubit2);
  applyCNOT(qubit2, qubit1);
  applyCNOT(qubit1, qubit2);
}

// --- Non-Clifford Faults ---
void StabilizerBackend::applyToffoli(size_t, size_t, size_t) {
  throw std::runtime_error("Error: Toffoli is not a Clifford gate. Cannot "
                           "simulate in StabilizerBackend.");
}

void StabilizerBackend::applyPhaseT(size_t) {
  throw std::runtime_error("Error: T gate is not a Clifford gate. Cannot "
                           "simulate in StabilizerBackend.");
}

void StabilizerBackend::applyRotationX(size_t, Precision) {
  throw std::runtime_error("Error: Continuous Rx rotations are non-Clifford.");
}

void StabilizerBackend::applyRotationY(size_t, Precision) {
  throw std::runtime_error("Error: Continuous Ry rotations are non-Clifford.");
}

void StabilizerBackend::applyRotationZ(size_t, Precision) {
  throw std::runtime_error("Error: Continuous Rz rotations are non-Clifford.");
}

void StabilizerBackend::applyDepolarizingNoise(Precision) {
  throw std::runtime_error("Error: Continuous noise application pending "
                           "stabilizer discrete measurement injection.");
}

void StabilizerBackend::applyDenseUnitary(const std::vector<size_t> &targets,
                                         const std::vector<Complex> &matrix) {
  throw FeatureNotSupportedException(
      "applyDenseUnitary not supported in Stabilizer backend.");
}

} // namespace qubit_engine
