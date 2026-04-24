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

// Rotation X Kernel
// Rx(theta) = [[cos(θ/2), -i·sin(θ/2)], [-i·sin(θ/2), cos(θ/2)]]
__global__ void kRotationX(cuDoubleComplex *state, int num_qubits, int target,
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

  // -i·sin(θ/2)
  cuDoubleComplex neg_is = make_cuDoubleComplex(0.0, -s);

  state[i0] = add(scale(v0, c), mul(neg_is, v1));
  state[i1] = add(mul(neg_is, v0), scale(v1, c));
}

// SWAP Kernel: Swap amplitudes between qubit1 and qubit2
__global__ void kSWAP(cuDoubleComplex *state, int num_qubits, int qubit1,
                      int qubit2) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int dim = 1 << num_qubits;
  if (idx >= dim)
    return;

  // Only process indices where qubit1=0, qubit2=1 (swap with qubit1=1,
  // qubit2=0)
  int bit1 = (idx >> qubit1) & 1;
  int bit2 = (idx >> qubit2) & 1;

  if (bit1 == 0 && bit2 == 1) {
    // Swap this index with the one where bits are flipped
    int swapped = idx ^ (1 << qubit1) ^ (1 << qubit2);
    cuDoubleComplex temp = state[idx];
    state[idx] = state[swapped];
    state[swapped] = temp;
  }
}

// CZ Kernel: Apply phase flip (-1) when both control and target are |1⟩
__global__ void kCZ(cuDoubleComplex *state, int num_qubits, int control,
                    int target) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int dim = 1 << num_qubits;
  if (idx >= dim)
    return;

  // If both control and target bits are set, negate amplitude
  if (((idx >> control) & 1) && ((idx >> target) & 1)) {
    state[idx] = scale(state[idx], -1.0);
  }
}

// Apply a 2x2 Kraus matrix and scale by inv_norm
__global__ void kApplyKraus1Q(cuDoubleComplex *state, int num_qubits, int target,
                              cuDoubleComplex m00, cuDoubleComplex m01, 
                              cuDoubleComplex m10, cuDoubleComplex m11, 
                              double inv_norm) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim)
    return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  cuDoubleComplex a = state[i0];
  cuDoubleComplex b = state[i1];

  cuDoubleComplex out0 = add(mul(m00, a), mul(m01, b));
  cuDoubleComplex out1 = add(mul(m10, a), mul(m11, b));

  state[i0] = scale(out0, inv_norm);
  state[i1] = scale(out1, inv_norm);
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
  // cudaDeviceSynchronize(); // Phase 4: Async kernels
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
  // cudaDeviceSynchronize(); // Phase 4: Async kernels
}

void launchRotationZ(void *deviceState, int num_qubits, int target,
                     double angle) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kRotationZ<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState,
                                       num_qubits, target, angle);
  // cudaDeviceSynchronize(); // Phase 4: Async kernels
}

void launchCNOT(void *deviceState, int num_qubits, int control, int target) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kCNOT<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState, num_qubits,
                                  control, target);
  // cudaDeviceSynchronize(); // Phase 4: Async kernels
}

void launchToffoli(void *deviceState, int num_qubits, int control1,
                   int control2, int target) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kToffoli<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState, num_qubits,
                                     control1, control2, target);
  // cudaDeviceSynchronize(); // Phase 4: Async kernels
}

void launchRotationX(void *deviceState, int num_qubits, int target,
                     double angle) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kRotationX<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState,
                                       num_qubits, target, angle);
  // cudaDeviceSynchronize(); // Phase 4: Async kernels
}

void launchSWAP(void *deviceState, int num_qubits, int qubit1, int qubit2) {
  int dim = 1 << num_qubits;
  int blockSize = 256;
  int numBlocks = (dim + blockSize - 1) / blockSize;
  kSWAP<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState, num_qubits,
                                  qubit1, qubit2);
  // cudaDeviceSynchronize(); // Phase 4: Async kernels
}

void launchCZ(void *deviceState, int num_qubits, int control, int target) {
  int dim = 1 << num_qubits;
  int blockSize = 256;
  int numBlocks = (dim + blockSize - 1) / blockSize;
  kCZ<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState, num_qubits,
                                control, target);
  // cudaDeviceSynchronize(); // Phase 4: Async kernels
}

void launchComputeProbabilities(const void *deviceState, double *deviceProbs,
                                int dim) {
  int blockSize = 256;
  int numBlocks = (dim + blockSize - 1) / blockSize;
  kComputeProbabilities<<<numBlocks, blockSize>>>(
      (const cuDoubleComplex *)deviceState, deviceProbs, dim);
  // cudaDeviceSynchronize(); // Phase 4: Async kernels
}

void launchApplyKraus1Q(void *deviceState, int num_qubits, int target,
                        const void *matrix, double inv_norm) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  
  const cuDoubleComplex* m = (const cuDoubleComplex*)matrix;
  kApplyKraus1Q<<<numBlocks, blockSize>>>((cuDoubleComplex *)deviceState, num_qubits, target,
                                          m[0], m[1], m[2], m[3], inv_norm);
}

// --- Memory Helpers ---
void* allocateDeviceState(size_t size_bytes) {
  void* ptr;
  cudaMalloc(&ptr, size_bytes);
  return ptr;
}

void freeDeviceState(void* ptr) {
  cudaFree(ptr);
}

void copyDeviceToDevice(void* dst, const void* src, size_t size_bytes) {
  cudaMemcpy(dst, src, size_bytes, cudaMemcpyDeviceToDevice);
}

void setDeviceStateZero(void* ptr, size_t size_bytes) {
  cudaMemset(ptr, 0, size_bytes);
}

// --- Deriv Kernels ---

__global__ void kDerivativeRY(cuDoubleComplex* out, const cuDoubleComplex* in, int num_qubits, int target, double angle) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim) return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  cuDoubleComplex v0 = in[i0];
  cuDoubleComplex v1 = in[i1];

  double c = cos(angle / 2.0);
  double s = sin(angle / 2.0);
  double h = 0.5;

  cuDoubleComplex out0 = out[i0];
  cuDoubleComplex out1 = out[i1];

  out[i0] = add(out0, scale(sub(scale(v0, -s), scale(v1, c)), h));
  out[i1] = add(out1, scale(sub(scale(v0, c), scale(v1, s)), h));
}

__global__ void kDerivativeRX(cuDoubleComplex* out, const cuDoubleComplex* in, int num_qubits, int target, double angle) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim) return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  cuDoubleComplex v0 = in[i0];
  cuDoubleComplex v1 = in[i1];

  double c = cos(angle / 2.0);
  double s = sin(angle / 2.0);

  cuDoubleComplex neg_ic = make_cuDoubleComplex(0, -c * 0.5);
  double neg_s_half = -s * 0.5;

  out[i0] = add(out[i0], add(scale(v0, neg_s_half), mul(neg_ic, v1)));
  out[i1] = add(out[i1], add(mul(neg_ic, v0), scale(v1, neg_s_half)));
}

__global__ void kDerivativeRZ(cuDoubleComplex* out, const cuDoubleComplex* in, int num_qubits, int target, double angle) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int half_dim = 1 << (num_qubits - 1);
  if (idx >= half_dim) return;

  int i0 = ((idx >> target) << (target + 1)) | (idx & ((1 << target) - 1));
  int i1 = i0 | (1 << target);

  cuDoubleComplex d0_phase = make_cuDoubleComplex(cos(-angle/2.0), sin(-angle/2.0));
  cuDoubleComplex d0 = mul(scale(make_cuDoubleComplex(0, -1), 0.5), d0_phase);
  
  cuDoubleComplex d1_phase = make_cuDoubleComplex(cos(angle/2.0), sin(angle/2.0));
  cuDoubleComplex d1 = mul(scale(make_cuDoubleComplex(0, 1), 0.5), d1_phase);

  out[i0] = add(out[i0], mul(d0, in[i0]));
  out[i1] = add(out[i1], mul(d1, in[i1]));
}

__global__ void kAdjointInnerProduct(const cuDoubleComplex* dpsi, const cuDoubleComplex* lambda, double* grad_out, int dim) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if(idx < dim) {
    cuDoubleComplex dp = dpsi[idx];
    cuDoubleComplex lam = lambda[idx];
    double re = cuCreal(lam) * cuCreal(dp) + cuCimag(lam) * cuCimag(dp); // Re(conj(lam) * dp)
    atomicAdd(grad_out, 2.0 * re);
  }
}

__global__ void kApplyPauliTerm(cuDoubleComplex* out, const cuDoubleComplex* in, int num_qubits, const int* d_pauli_ops, double coeff) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int dim = 1 << num_qubits;
  if (idx >= dim) return;

  int j = idx;
  cuDoubleComplex c = make_cuDoubleComplex(coeff, 0);

  for (int q = 0; q < num_qubits; ++q) {
    int op = d_pauli_ops[q];
    if (op == 0) continue; // I
    
    int bit_set = (idx >> q) & 1;
    if (op == 1) { // X
      j ^= (1 << q);
    } else if (op == 2) { // Y
      j ^= (1 << q);
      c = mul(c, bit_set ? make_cuDoubleComplex(0, -1) : make_cuDoubleComplex(0, 1));
    } else if (op == 3) { // Z
      if (bit_set) c = scale(c, -1.0);
    }
  }

  out[j] = add(out[j], mul(c, in[idx]));
}

void launchDerivativeRY(void* out, const void* in, int num_qubits, int target, double angle) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kDerivativeRY<<<numBlocks, blockSize>>>((cuDoubleComplex*)out, (const cuDoubleComplex*)in, num_qubits, target, angle);
}

void launchDerivativeRX(void* out, const void* in, int num_qubits, int target, double angle) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kDerivativeRX<<<numBlocks, blockSize>>>((cuDoubleComplex*)out, (const cuDoubleComplex*)in, num_qubits, target, angle);
}

void launchDerivativeRZ(void* out, const void* in, int num_qubits, int target, double angle) {
  int half_dim = 1 << (num_qubits - 1);
  int blockSize = 256;
  int numBlocks = (half_dim + blockSize - 1) / blockSize;
  kDerivativeRZ<<<numBlocks, blockSize>>>((cuDoubleComplex*)out, (const cuDoubleComplex*)in, num_qubits, target, angle);
}

void launchAdjointInnerProduct(const void* dpsi, const void* lambda, double* grad_out, int dim) {
  int blockSize = 256;
  int numBlocks = (dim + blockSize - 1) / blockSize;
  kAdjointInnerProduct<<<numBlocks, blockSize>>>((const cuDoubleComplex*)dpsi, (const cuDoubleComplex*)lambda, grad_out, dim);
}

void launchApplyPauliTerm(void* out, const void* in, int num_qubits, const int* d_pauli_ops, double coeff) {
  int dim = 1 << num_qubits;
  int blockSize = 256;
  int numBlocks = (dim + blockSize - 1) / blockSize;
  kApplyPauliTerm<<<numBlocks, blockSize>>>((cuDoubleComplex*)out, (const cuDoubleComplex*)in, num_qubits, d_pauli_ops, coeff);
}

} // namespace cuda
} // namespace qe