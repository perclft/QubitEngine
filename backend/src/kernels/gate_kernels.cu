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

// Rotation Z Kernel
// Rz(theta) = [[e^(-i*theta/2), 0], [0, e^(i*theta/2)]]
__global__ void kRotationZ(cuDoubleComplex *state, int num_qubits, int target,
                           double angle) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim)
    return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  double half_theta = angle / 2.0;
  // e^(-i*theta/2)
  cuDoubleComplex phase0 =
      make_cuDoubleComplex(cos(-half_theta), sin(-half_theta));
  // e^(i*theta/2)
  cuDoubleComplex phase1 =
      make_cuDoubleComplex(cos(half_theta), sin(half_theta));

  state[i0] = mul(phase0, state[i0]);
  state[i1] = mul(phase1, state[i1]);
}

// Phase S Kernel: S = [[1, 0], [0, i]]
__global__ void kPhaseS(cuDoubleComplex *state, int num_qubits, int target) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim)
    return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  cuDoubleComplex phase = make_cuDoubleComplex(0, 1); // i
  state[i1] = mul(phase, state[i1]);
}

// Phase T Kernel: T = [[1, 0], [0, e^(i*pi/4)]]
__global__ void kPhaseT(cuDoubleComplex *state, int num_qubits, int target) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim)
    return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  // e^(i*pi/4) = cos(pi/4) + i*sin(pi/4) = (1 + i) / sqrt(2)
  cuDoubleComplex phase =
      make_cuDoubleComplex(0.70710678118654752440, 0.70710678118654752440);
  state[i1] = mul(phase, state[i1]);
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

// Toffoli Kernel (CCX): Flip target if both controls are set
__global__ void kToffoli(cuDoubleComplex *state, int num_qubits, int control1,
                         int control2, int target) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim)
    return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  // Check if both control bits are set in i0
  if (((i0 >> control1) & 1) && ((i0 >> control2) & 1)) {
    cuDoubleComplex temp = state[i0];
    state[i0] = state[i1];
    state[i1] = temp;
  }
}

// Probability computation kernel: |state[i]|^2
__global__ void kComputeProbabilities(const cuDoubleComplex *state,
                                      double *probs, int dim) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= dim)
    return;

  double re = cuCreal(state[idx]);
  double im = cuCimag(state[idx]);
  probs[idx] = re * re + im * im;
}

// --- Launch Functions ---

namespace qe {
namespace cuda {

static void launchKernel1Q(void (*kernel)(cuDoubleComplex *, int, int),
                           void *deviceState, int num_qubits, int target) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kernel<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState, num_qubits,
                                   target);
  cudaDeviceSynchronize();
}

void launchHadamard(void *deviceState, int num_qubits, int target) {
  launchKernel1Q(kHadamard, deviceState, num_qubits, target);
}

void launchapplyX(void *deviceState, int num_qubits, int target) {
  launchKernel1Q(kApplyX, deviceState, num_qubits, target);
}

void launchapplyY(void *deviceState, int num_qubits, int target) {
  launchKernel1Q(kApplyY, deviceState, num_qubits, target);
}

void launchapplyZ(void *deviceState, int num_qubits, int target) {
  launchKernel1Q(kApplyZ, deviceState, num_qubits, target);
}

void launchPhaseS(void *deviceState, int num_qubits, int target) {
  launchKernel1Q(kPhaseS, deviceState, num_qubits, target);
}

void launchPhaseT(void *deviceState, int num_qubits, int target) {
  launchKernel1Q(kPhaseT, deviceState, num_qubits, target);
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

void launchRotationZ(void *deviceState, int num_qubits, int target,
                     double angle) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kRotationZ<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState,
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

void launchToffoli(void *deviceState, int num_qubits, int control1,
                   int control2, int target) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kToffoli<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState, num_qubits,
                                     control1, control2, target);
  cudaDeviceSynchronize();
}

void launchComputeProbabilities(const void *deviceState, double *deviceProbs,
                                int dim) {
  int blockSize = 256;
  int numBlocks = (dim + blockSize - 1) / blockSize;
  kComputeProbabilities<<<numBlocks, blockSize>>>(
      (const cuDoubleComplex *)deviceState, deviceProbs, dim);
  cudaDeviceSynchronize();
}

} // namespace cuda
} // namespace qe