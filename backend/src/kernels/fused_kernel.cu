#include "GateKernels.hpp"
#include <cuComplex.h>
#include <cuda_runtime.h>

// ---------------------------------------------------------------------------
// JIT Fused Unitary Kernel
//
// Applies an arbitrary 2^k × 2^k unitary matrix (k ≤ 3) to a localized
// sub-register of k target qubits.  The fused matrix is uploaded to constant
// memory once, and every thread applies the matrix-vector product in
// registers, eliminating intermediate global memory round-trips that occur
// with per-gate kernel launches.
// ---------------------------------------------------------------------------

// Maximum fused block: 3 qubits → 8×8 matrix → 64 entries
__constant__ cuDoubleComplex c_fused_unitary[64];

// k=1: 2×2, k=2: 4×4, k=3: 8×8
__global__ void kApplyFusedUnitary(cuDoubleComplex *state, int num_qubits,
                                   const int *targets, int k) {
  // Total dimension and sub-dimension
  int dim = 1 << num_qubits;
  int sub_dim = 1 << k; // 2^k rows in the unitary

  // Each thread handles one "base" index from which we extract the sub-register
  int thread_id = blockIdx.x * blockDim.x + threadIdx.x;

  // Number of outer iterations = dim / sub_dim
  int num_outer = dim >> k;
  if (thread_id >= num_outer)
    return;

  // Compute the base index by "inserting" 0-bits at each target qubit position
  // We iterate through outer bits, skipping the target qubit positions
  int base = thread_id;
  // Sort targets to insert bits in order
  int sorted_targets[3];
  for (int i = 0; i < k; ++i)
    sorted_targets[i] = targets[i];
  // Simple insertion sort for k ≤ 3
  for (int i = 1; i < k; ++i) {
    int val = sorted_targets[i];
    int j = i - 1;
    while (j >= 0 && sorted_targets[j] > val) {
      sorted_targets[j + 1] = sorted_targets[j];
      j--;
    }
    sorted_targets[j + 1] = val;
  }

  // Insert zero-bits at sorted target positions
  for (int i = 0; i < k; ++i) {
    int t = sorted_targets[i];
    int mask_low = (1 << t) - 1;
    int mask_high = ~mask_low;
    base = ((base & mask_high) << 1) | (base & mask_low);
  }

  // Collect sub_dim amplitudes into registers
  cuDoubleComplex amps[8]; // max sub_dim = 8 for k=3
  int indices[8];

  for (int s = 0; s < sub_dim; ++s) {
    int idx = base;
    for (int b = 0; b < k; ++b) {
      if ((s >> b) & 1) {
        idx |= (1 << targets[b]);
      }
    }
    indices[s] = idx;
    amps[s] = state[idx];
  }

  // Apply unitary: result[r] = sum_c U[r][c] * amps[c]
  for (int r = 0; r < sub_dim; ++r) {
    cuDoubleComplex acc = make_cuDoubleComplex(0.0, 0.0);
    for (int c = 0; c < sub_dim; ++c) {
      cuDoubleComplex u_rc = c_fused_unitary[r * sub_dim + c];
      // Complex multiply-add
      double re =
          cuCreal(u_rc) * cuCreal(amps[c]) - cuCimag(u_rc) * cuCimag(amps[c]);
      double im =
          cuCreal(u_rc) * cuCimag(amps[c]) + cuCimag(u_rc) * cuCreal(amps[c]);
      acc = make_cuDoubleComplex(cuCreal(acc) + re, cuCimag(acc) + im);
    }
    state[indices[r]] = acc;
  }
}

// ---------------------------------------------------------------------------
// Launch wrapper
// ---------------------------------------------------------------------------
namespace qe {
namespace cuda {

void launchFusedUnitary(void *deviceState, int num_qubits, const int *d_targets,
                        int k, const cuDoubleComplex *h_unitary) {
  int sub_dim = 1 << k;

  // Upload unitary matrix to constant memory
  cudaMemcpyToSymbol(c_fused_unitary, h_unitary,
                     sub_dim * sub_dim * sizeof(cuDoubleComplex));

  int num_outer = (1 << num_qubits) >> k;
  int blockSize = 256;
  int numBlocks = (num_outer + blockSize - 1) / blockSize;

  kApplyFusedUnitary<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState,
                                               num_qubits, d_targets, k);
}

} // namespace cuda
} // namespace qe
