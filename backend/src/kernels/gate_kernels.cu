#include "GateKernels.hpp"
#include <cuComplex.h>
#include <cuda_runtime.h>
#include <iostream>

// Helper for complex numbers
__device__ cuDoubleComplex add(cuDoubleComplex a, cuDoubleComplex b) {
  return make_cuDoubleComplex(cuCreal(a) + cuCreal(b), cuCimag(a) + cuCimag(b));
}

__device__ cuDoubleComplex sub(cuDoubleComplex a, cuDoubleComplex b) {
  return make_cuDoubleComplex(cuCreal(a) - cuCreal(b), cuCimag(a) - cuCimag(b));
}

__device__ cuDoubleComplex mul(cuDoubleComplex a, cuDoubleComplex b) {
  return make_cuDoubleComplex(cuCreal(a) * cuCreal(b) - cuCimag(a) * cuCimag(b),
                              cuCreal(a) * cuCimag(b) +
                                  cuCimag(a) * cuCreal(b));
}

__device__ cuDoubleComplex scale(cuDoubleComplex a, double s) {
  return make_cuDoubleComplex(cuCreal(a) * s, cuCimag(a) * s);
}

// --- Kernels ---

// Hadamard Kernel
__global__ void kHadamard(cuDoubleComplex *state, int num_qubits, int target) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);

  if (idx >= half_dim)
    return;

  // Mask logic is same as kApplyX
  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  cuDoubleComplex v0 = state[i0];
  cuDoubleComplex v1 = state[i1];

  double is2 = 0.70710678118654752440; // 1/sqrt(2)

  state[i0] = scale(add(v0, v1), is2);
  state[i1] = scale(sub(v0, v1), is2);
}

// Pauli X Kernel
__global__ void kApplyX(cuDoubleComplex *state, int num_qubits, int target) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim)
    return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  cuDoubleComplex temp = state[i0];
  state[i0] = state[i1];
  state[i1] = temp;
}

// Pauli Y Kernel
__global__ void kApplyY(cuDoubleComplex *state, int num_qubits, int target) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim)
    return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  cuDoubleComplex v0 = state[i0];
  cuDoubleComplex v1 = state[i1];

  // Y = [[0, -i], [i, 0]]
  cuDoubleComplex I = make_cuDoubleComplex(0, 1);
  cuDoubleComplex nI = make_cuDoubleComplex(0, -1);

  state[i0] = mul(nI, v1);
  state[i1] = mul(I, v0);
}

// Pauli Z Kernel
__global__ void kApplyZ(cuDoubleComplex *state, int num_qubits, int target) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim)
    return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  // Z rule: v1 -> -v1
  state[i1] = scale(state[i1], -1.0);
}

// Rotation Y Kernel
__global__ void kRotationY(cuDoubleComplex *state, int num_qubits, int target,
                           double angle) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim)
    return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  cuDoubleComplex v0 = state[i0];
  cuDoubleComplex v1 = state[i1];

  double half_theta = angle / 2.0;
  double c = cos(half_theta);
  double s = sin(half_theta);

  cuDoubleComplex term1 = scale(v0, c);
  cuDoubleComplex term2 = scale(v1, s);

  state[i0] = sub(term1, term2);
  state[i1] = add(scale(v0, s), scale(v1, c));
}

// CNOT Kernel
__global__ void kCNOT(cuDoubleComplex *state, int num_qubits, int control,
                      int target) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim)
    return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  // Check if control bit is set in i0
  if ((i0 >> control) & 1) {
    cuDoubleComplex temp = state[i0];
    state[i0] = state[i1];
    state[i1] = temp;
  }
}

namespace qe {
namespace cuda {

void launchHadamard(void *deviceState, int num_qubits, int target) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kHadamard<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState,
                                      num_qubits, target);
  cudaDeviceSynchronize();
}

void launchapplyX(void *deviceState, int num_qubits, int target) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kApplyX<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState, num_qubits,
                                    target);
  cudaDeviceSynchronize();
}

void launchapplyY(void *deviceState, int num_qubits, int target) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kApplyY<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState, num_qubits,
                                    target);
  cudaDeviceSynchronize();
}

void launchapplyZ(void *deviceState, int num_qubits, int target) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kApplyZ<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState, num_qubits,
                                    target);
  cudaDeviceSynchronize();
}

void launchRotationY(void *deviceState, int num_qubits, int target,
                     double angle) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kRotationY<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState,
                                       num_qubits, target, angle);
  cudaDeviceSynchronize();
}

void launchCNOT(void *deviceState, int num_qubits, int control, int target) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kCNOT<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState, num_qubits,
                                  control, target);
  cudaDeviceSynchronize();
}

} // namespace cuda
} // namespace qe