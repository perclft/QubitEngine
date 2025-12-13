#include "QuantumRegister.hpp"
#include <algorithm>
#include <cmath>
#include <immintrin.h> // AVX2
#include <iostream>
#include <mpi.h>
#include <omp.h>
#include <random>
#include <stdexcept>

// Thread-local RNG
thread_local std::mt19937 gen(std::random_device{}());

const double INV_SQRT_2 = 0.70710678118654752440;

QuantumRegister::QuantumRegister(size_t n) : num_qubits(n) {
  // MPI Setup
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  // Calculate local storage size
  // Total Size: 2^n. Local Size: 2^n / mpi_size.
  size_t total_size = 1ULL << num_qubits;
  local_size = total_size / mpi_size;

  if (local_size == 0) {
    throw std::runtime_error("MPI Size too large for this number of qubits. "
                             "Each rank must hold at least 1 amplitude.");
  }

  state.resize(local_size);

  // Initialize |00...0> state
  // Only Rank 0 holds the first amplitude (index 0)
  if (mpi_rank == 0) {
    state[0] = Complex(1.0, 0.0);
    // Rest are 0.0 by default resize
  }
  // Other ranks are all 0.0 (default)
}

// Logic:
// Qubits 0 to log2(local_size)-1 are LOCAL. (Least Significant)
// Qubits log2(local_size) to n-1 are GLOBAL. (Most Significant)
bool QuantumRegister::isLocalQubit(size_t qubit_index) const {
  return (1ULL << qubit_index) < local_size;
}

size_t QuantumRegister::getGlobalPairRank(size_t qubit_index) const {
  size_t local_bits = 0;
  while ((1ULL << local_bits) < local_size)
    local_bits++;

  size_t rank_bit = qubit_index - local_bits;
  return mpi_rank ^ (1ULL << rank_bit);
}

void QuantumRegister::applyHadamard(size_t q) {
  validateIndex(q);
  if (isLocalQubit(q)) {
    // --- LOCAL OMP PARALLELIZATION ---
    size_t step = 1ULL << q;
#pragma omp parallel for
    for (size_t i = 0; i < local_size; i += 2 * step) {
      for (size_t j = i; j < i + step; ++j) {
        Complex a = state[j];
        Complex b = state[j + step];
        state[j] = (a + b) * INV_SQRT_2;
        state[j + step] = (a - b) * INV_SQRT_2;
      }
    }
  } else {
    // --- DISTRIBUTED (MPI) ---
    int partner = getGlobalPairRank(q);

    std::vector<Complex> partner_state(local_size);

    // Use MPI_Sendrecv for simultaneous exchange
    // vector<Complex> is basically vector<double> size*2
    MPI_Sendrecv(state.data(), local_size * 2, MPI_DOUBLE, partner, 0,
                 partner_state.data(), local_size * 2, MPI_DOUBLE, partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    bool am_zero = mpi_rank < partner;

#pragma omp parallel for
    for (size_t i = 0; i < local_size; ++i) {
      Complex my_val = state[i];
      Complex other_val = partner_state[i];

      if (am_zero) {
        state[i] = (my_val + other_val) * INV_SQRT_2;
      } else {
        state[i] = (other_val - my_val) * INV_SQRT_2;
      }
    }
  }
}

void QuantumRegister::applyX(size_t q) {
  validateIndex(q);
  if (isLocalQubit(q)) {
    size_t step = 1ULL << q;
#pragma omp parallel for
    for (size_t i = 0; i < local_size; i += 2 * step) {
      for (size_t j = i; j < i + step; ++j) {
        std::swap(state[j], state[j + step]);
      }
    }
  } else {
    // Distributed X: Just swap entire state with partner
    int partner = getGlobalPairRank(q);
    std::vector<Complex> buffer(local_size);

    MPI_Sendrecv(state.data(), local_size * 2, MPI_DOUBLE, partner, 1,
                 buffer.data(), local_size * 2, MPI_DOUBLE, partner, 1,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    state = buffer;
  }
}

void QuantumRegister::applyCNOT(size_t control, size_t target) {
  validateIndex(control);
  validateIndex(target);

  // Initial Implementation: Only Local CNOT supported for now
  if (!isLocalQubit(control) || !isLocalQubit(target)) {
    if (mpi_rank == 0)
      std::cerr << "Warning: Distributed CNOT not fully implemented."
                << std::endl;
    return;
  }

  // Local Implementation
  // Since we are iterating local state, index 'i' corresponds to global index
  // 'i + offset' But 'local' checks mean we only care about bits within the
  // local mask.
  size_t N = local_size;
#pragma omp parallel for
  for (size_t i = 0; i < N; ++i) {
    // Local indices are sufficient if both qubits are local
    if ((i >> control) & 1) {
      size_t pair = i ^ (1ULL << target);
      if (pair > i) {
        std::swap(state[i], state[pair]);
      }
    }
  }
}

void QuantumRegister::applyPhaseS(size_t t) {
  validateIndex(t); /* Local Only Stub */
}
void QuantumRegister::applyPhaseT(size_t t) {
  validateIndex(t); /* Local Only Stub */
}
void QuantumRegister::applyRotationY(size_t t, double a) {
  validateIndex(t); /* Local Only Stub */
}
void QuantumRegister::applyRotationZ(size_t t, double a) {
  validateIndex(t); /* Local Only Stub */
}
void QuantumRegister::applyToffoli(size_t c1, size_t c2, size_t t) {
  validateIndex(c1); /* Local Only Stub */
}
void QuantumRegister::applyDepolarizingNoise(double p) { /* Local Only Stub */ }

// Measure Helper
bool QuantumRegister::measure(size_t target) {
  // Distributed Measurement requires MPI_Allreduce to get total probability
  double prob1 = 0.0;

  // Only verify target is valid globally?
  // validateIndex checks if >= num_qubits.
  validateIndex(target);

// Calculate local probability
#pragma omp parallel for reduction(+ : prob1)
  for (size_t i = 0; i < local_size; ++i) {
    size_t global_i = i + (static_cast<size_t>(mpi_rank) * local_size);
    if ((global_i >> target) & 1) {
      prob1 += std::norm(state[i]);
    }
  }

  // Reduce across all ranks
  double global_prob1 = 0.0;
  MPI_Allreduce(&prob1, &global_prob1, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

  std::uniform_real_distribution<double> dist(0.0, 1.0);
  // Rank 0 decides? Or all ranks decide assuming same seeded RNG?
  // Better: Rank 0 decides and broadcasts.
  // Or we rely on MPI_Allreduce of the outcome?
  // Let's have each rank roll independent? NO. They must agree on outcome.

  // Simplest: Rank 0 rolls, Bcasts result.
  int outcome_int = 0;
  if (mpi_rank == 0) {
    bool outcome_bool = dist(gen) < global_prob1;
    outcome_int = outcome_bool ? 1 : 0;
    // Bcast
  }
  MPI_Bcast(&outcome_int, 1, MPI_INT, 0, MPI_COMM_WORLD);
  bool outcome = (outcome_int == 1);

  // Collapse State (Distributed)
  double norm_factor = 0.0;
  if (outcome) {
    if (global_prob1 > 1e-12)
      norm_factor = 1.0 / std::sqrt(global_prob1);
  } else {
    if ((1.0 - global_prob1) > 1e-12)
      norm_factor = 1.0 / std::sqrt(1.0 - global_prob1);
  }

#pragma omp parallel for
  for (size_t i = 0; i < local_size; ++i) {
    size_t global_i = i + (static_cast<size_t>(mpi_rank) * local_size);
    bool bit_set = (global_i >> target) & 1;

    if (bit_set == outcome) {
      state[i] *= norm_factor;
    } else {
      state[i] = 0.0;
    }
  }

  return outcome;
}

double QuantumRegister::expectationValue(const std::string &pauli) {
  return 0.0; /* Stub */
}

const std::vector<Complex> &QuantumRegister::getStateVector() const {
  // Should gather... but for now return local
  return state;
}

void QuantumRegister::syncState() { MPI_Barrier(MPI_COMM_WORLD); }
