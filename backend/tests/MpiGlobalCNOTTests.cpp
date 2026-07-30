#include "../src/backends/CpuBackend.hpp"
#include <cassert>
#include <cmath>
#include <complex>
#include <iostream>
#include <mpi.h>
#include <vector>

using namespace qubit_engine;

static bool are_close(Complex a, Complex b, double tol = 1e-5) {
  return std::abs(a - b) < tol;
}

int main(int argc, char **argv) {
  int initialized;
  MPI_Initialized(&initialized);
  if (!initialized) {
    MPI_Init(&argc, &argv);
  }

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  double start_time = MPI_Wtime();

  // We require size >= 2 (supports 2, 4, 8 ranks)
  if (size < 2) {
    if (rank == 0) {
      std::cout << "[MPI Global CNOT Test] Single rank detected. Skipping MPI multi-rank test." << std::endl;
    }
    MPI_Finalize();
    return 0;
  }

  // 5 Qubits: Total dim = 32.
  // For 4 ranks: local_dim = 8 (3 local qubits 0,1,2; 2 global qubits 3,4).
  // For 2 ranks: local_dim = 16 (4 local qubits 0,1,2,3; 1 global qubit 4).
  size_t num_qubits = 5;
  CpuBackend backend(num_qubits, false);

  size_t local_dim = 1ULL << num_qubits / size;
  size_t control_qubit = 4; // Global qubit for both 2 and 4 ranks
  size_t target_qubit = (size >= 4) ? 3 : 0; // Global for >= 4 ranks

  if (rank == 0) {
    std::cout << "[MPI Global CNOT Test] Running with " << size << " ranks on " << num_qubits
              << " qubits. Control=" << control_qubit << ", Target=" << target_qubit << std::endl;
  }

  // Step 1: Apply H(control_qubit) -> Creates superposition between rank 0 and control rank
  backend.applyHadamard(control_qubit);

  // Check timeout (5 seconds limit)
  if (MPI_Wtime() - start_time > 5.0) {
    std::cerr << "[MPI Rank " << rank << "] TIMEOUT EXCEEDED during H(" << control_qubit << ")" << std::endl;
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  // Step 2: Apply Global CNOT(control_qubit, target_qubit)
  backend.applyCNOT(control_qubit, target_qubit);

  if (MPI_Wtime() - start_time > 5.0) {
    std::cerr << "[MPI Rank " << rank << "] TIMEOUT EXCEEDED during CNOT(" << control_qubit << ", " << target_qubit << ")" << std::endl;
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  // Step 3: Verify State Vector
  auto state = backend.getStateVector();
  double inv_sqrt2 = 1.0 / std::sqrt(2.0);

  size_t control_rank_bit = (1ULL << control_qubit) / state.size();
  size_t target_rank_bit = (1ULL << target_qubit) / state.size();

  bool has_control = (rank & control_rank_bit) != 0;
  bool has_target = (rank & target_rank_bit) != 0;

  if (!has_control && !has_target) {
    // Rank 0: Should have amplitude 1/sqrt(2) at index 0 (|00000>)
    if (!are_close(state[0], inv_sqrt2)) {
      std::cerr << "[Rank " << rank << "] FAIL: Expected state[0] = 1/sqrt(2), got " << state[0] << std::endl;
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
  } else if (has_control && has_target) {
    // Rank with both control and target bits set: Should have amplitude 1/sqrt(2) at index 0 (|11000>)
    if (!are_close(state[0], inv_sqrt2)) {
      std::cerr << "[Rank " << rank << "] FAIL: Expected state[0] = 1/sqrt(2), got " << state[0] << std::endl;
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
  }

  if (rank == 0) {
    std::cout << "[MPI Global CNOT Test] SUCCESS: Dual global CNOT completed without deadlock. State vector verified in "
              << (MPI_Wtime() - start_time) << " seconds." << std::endl;
  }

  MPI_Finalize();
  return 0;
}
