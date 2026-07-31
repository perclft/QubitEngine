#include "GateKernels.hpp"
#include <cuComplex.h>
#include <cuda_runtime.h>

// Portable double-precision atomicAdd for architectures < sm_60
#if !defined(__CUDA_ARCH__) || __CUDA_ARCH__ < 600
__device__ static double atomicAdd_double(double* address, double val) {
  unsigned long long int* address_as_ull = (unsigned long long int*)address;
  unsigned long long int old = *address_as_ull, assumed;
  do {
    assumed = old;
    old = atomicCAS(address_as_ull, assumed,
                    __double_as_longlong(val + __longlong_as_double(assumed)));
  } while (assumed != old);
  return __longlong_as_double(old);
}
#else
__device__ static inline double atomicAdd_double(double* address, double val) {
  return atomicAdd(address, val);
}
#endif

// ---------------------------------------------------------------------------
// Device-Side Pauli Expectation Value Reduction
//
// Computes ⟨ψ|H|ψ⟩ entirely on the GPU for a single Pauli string.
// Each thread evaluates conj(ψ[i]) * coeff * ψ[j] where j is determined
// by the Pauli bit-flip pattern.  Block-level shared-memory reduction
// feeds a global atomicAdd so only 8 bytes cross PCIe.
// ---------------------------------------------------------------------------

// pauli_ops encoding per qubit: 0=I, 1=X, 2=Y, 3=Z
__global__ void kPauliExpectation(const cuDoubleComplex *state,
                                  const int *pauli_ops, int num_qubits, int dim,
                                  double *result) {
  extern __shared__ double sdata[];

  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int tid = threadIdx.x;

  double local_real = 0.0;

  if (idx < dim) {
    // Compute target index j and coefficient by applying Pauli string
    int j = idx;
    double coeff_real = 1.0;
    double coeff_imag = 0.0;

    for (int q = 0; q < num_qubits; ++q) {
      int op = pauli_ops[q];
      bool bit_set = (idx >> q) & 1;

      if (op == 1) {
        // Pauli X: flip bit q
        j ^= (1 << q);
      } else if (op == 2) {
        // Pauli Y: flip bit q, multiply by ±i
        j ^= (1 << q);
        // Multiply coeff by (bit_set ? -i : +i)
        // (a + bi) * (0 + ci) = -bc + aci  where c = ±1
        double c = bit_set ? -1.0 : 1.0;
        double new_real = -coeff_imag * c;
        double new_imag = coeff_real * c;
        coeff_real = new_real;
        coeff_imag = new_imag;
      } else if (op == 3) {
        // Pauli Z: negate if bit is set
        if (bit_set) {
          coeff_real = -coeff_real;
          coeff_imag = -coeff_imag;
        }
      }
      // op == 0 (Identity): no-op
    }

    if (j < dim) {
      // conj(state[i]) * coeff * state[j]
      double si_re = cuCreal(state[idx]);
      double si_im = cuCimag(state[idx]);
      double sj_re = cuCreal(state[j]);
      double sj_im = cuCimag(state[j]);

      // coeff * state[j]
      double cj_re = coeff_real * sj_re - coeff_imag * sj_im;
      double cj_im = coeff_real * sj_im + coeff_imag * sj_re;

      // conj(state[i]) * (coeff * state[j]) — take real part only
      // conj(a+bi) * (c+di) = (ac + bd) + (ad - bc)i
      local_real = si_re * cj_re + si_im * cj_im;
    }
  }

  // Block-level reduction in shared memory
  sdata[tid] = local_real;
  __syncthreads();

  for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
    if (tid < s) {
      sdata[tid] += sdata[tid + s];
    }
    __syncthreads();
  }

  // Thread 0 of each block atomically adds to global result
  if (tid == 0) {
    atomicAdd_double(result, sdata[0]);
  }
}

// ---------------------------------------------------------------------------
// Launch wrapper
// ---------------------------------------------------------------------------
namespace qe {
namespace cuda {

void launchPauliExpectation(const void *deviceState, int num_qubits,
                            const int *d_pauli_ops, double *d_result) {
  int dim = 1 << num_qubits;
  int blockSize = 256;
  int numBlocks = (dim + blockSize - 1) / blockSize;
  size_t sharedSize = blockSize * sizeof(double);

  // Zero the result accumulator
  cudaMemset(d_result, 0, sizeof(double));

  kPauliExpectation<<<numBlocks, blockSize, sharedSize>>>(
      (const cuDoubleComplex *)deviceState, d_pauli_ops, num_qubits, dim,
      d_result);
}

} // namespace cuda
} // namespace qe
