#include "src/QuantumRegister.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <mpi.h>
#include <vector>

// Simple mock or usage of QuantumRegister directly to verify shared state
// logic. We don't need the full gRPC server for this unit test of distributed
// logic.

bool are_close(std::complex<double> a, std::complex<double> b,
               double tol = 1e-5) {
  return std::abs(a - b) < tol;
}

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  if (world_size != 2) {
    if (world_rank == 0) {
      std::cerr << "Error: This test requires exactly 2 MPI ranks."
                << std::endl;
    }
    MPI_Finalize();
    return 1;
  }

  // 4 qubits. Local dim = 16 / 2 = 8.
  // Rank 0: Indices 0-7 (0000 to 0111) -> MSB (Qubit 3) is 0
  // Rank 1: Indices 8-15 (1000 to 1111) -> MSB (Qubit 3) is 1
  size_t num_qubits = 4;
  QuantumRegister reg(num_qubits);

  // Initial state |0000> -> Index 0 on Rank 0 has amplitude 1.0.
  // All others 0.0.

  // 1. Apply H on Qubit 3 (Global).
  // Moves |0000> to (|0000> + |1000>) / sqrt(2).
  // Rank 0 Index 0: 1/sqrt(2)
  // Rank 1 Index 0 (Global 8): 1/sqrt(2)
  if (world_rank == 0)
    std::cout << "Applying H(3)..." << std::endl;
  reg.applyHadamard(3);

  auto state = reg.getStateVector();
  double inv_sqrt2 = 1.0 / std::sqrt(2.0);

  if (world_rank == 0) {
    assert(are_close(state[0], inv_sqrt2));
  } else {
    assert(are_close(state[0],
                     inv_sqrt2)); // This corresponds to global index 8 (|1000>)
  }

  // 2. Apply CNOT(3, 0). Control=3 (Global), Target=0 (Local).
  // |1000> becomes |1001>.
  // Rank 1 Index 0 (1000) should become 0.
  // Rank 1 Index 1 (1001) should become 1/sqrt(2).
  // Rank 0 Index 0 (0000) stays 1/sqrt(2) (Control is 0).
  if (world_rank == 0)
    std::cout << "Applying CNOT(3, 0)..." << std::endl;
  reg.applyCNOT(3, 0);

  state = reg.getStateVector(); // Refresh

  if (world_rank == 0) {
    // Should be unchanged |0000>
    if (are_close(state[0], inv_sqrt2)) {
      std::cout << "[Rank 0] |0000> verified." << std::endl;
    } else {
      std::cerr << "[Rank 0] FAILED: |0000> amplitude mismatch." << std::endl;
      return 1;
    }
  } else {
    // Should have moved from |1000> (local 0) to |1001> (local 1)
    if (are_close(state[0], 0.0) && are_close(state[1], inv_sqrt2)) {
      std::cout << "[Rank 1] |1001> verified." << std::endl;
    } else {
      std::cerr << "[Rank 1] FAILED: |1001> amplitude mismatch." << std::endl;
      return 1;
    }
  }

  MPI_Finalize();
  return 0;
}
