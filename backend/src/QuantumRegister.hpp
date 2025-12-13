#pragma once
#include <complex>
#include <stdexcept> // For std::out_of_range
#include <vector>

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

  // Distributed Computing (MPI)
  int mpi_rank = 0;
  int mpi_size = 1;
  size_t local_size;

  bool isLocalQubit(size_t qubit_index) const;
  size_t getGlobalPairRank(size_t qubit_index) const;

public:
  explicit QuantumRegister(size_t n);

  void applyHadamard(size_t target_qubit) __attribute__((target("avx2,fma")));
  void applyX(size_t target_qubit);
  void applyCNOT(size_t control_qubit, size_t target_qubit);

  // Phase 3: New Gates
  void applyToffoli(size_t control1, size_t control2, size_t target);
  void applyPhaseS(size_t target); // Z90
  void applyPhaseT(size_t target); // Z45
  void applyRotationY(size_t target, double angle);
  void applyRotationZ(size_t target, double angle);

  // Phase 9: Noise
  void applyDepolarizingNoise(double probability);

  bool measure(size_t target_qubit);

  // Phase 19: VQE Support
  double expectationValue(const std::string &pauli_string);

  const std::vector<Complex> &getStateVector() const;

  // MPI Synchronization Helper
  void syncState();

  // Phase 23: Information
  int getRank() const { return mpi_rank; }
  int getSize() const { return mpi_size; }
};
