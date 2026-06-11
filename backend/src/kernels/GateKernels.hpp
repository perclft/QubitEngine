#pragma once

#include <cstddef>

namespace qe {
namespace cuda {

// Launchers for CUDA Kernels
// These functions wrap the <<<grid, block>>> calls

// --- Core Gates ---
void launchHadamard(void *deviceState, int num_qubits, int target);
void launchapplyX(void *deviceState, int num_qubits, int target);
void launchapplyY(void *deviceState, int num_qubits, int target);
void launchapplyZ(void *deviceState, int num_qubits, int target);
void launchCNOT(void *deviceState, int num_qubits, int control, int target);

// --- Advanced Gates ---
void launchToffoli(void *deviceState, int num_qubits, int control1,
                   int control2, int target);
void launchPhaseS(void *deviceState, int num_qubits, int target);
void launchPhaseT(void *deviceState, int num_qubits, int target);
void launchRotationY(void *deviceState, int num_qubits, int target,
                     double angle);
void launchRotationX(void *deviceState, int num_qubits, int target,
                     double angle);
void launchRotationZ(void *deviceState, int num_qubits, int target,
                     double angle);
void launchSWAP(void *deviceState, int num_qubits, int qubit1, int qubit2);
void launchCZ(void *deviceState, int num_qubits, int control, int target);

// --- Measurement Helpers ---
void launchComputeProbabilities(const void *deviceState, double *deviceProbs,
                                int dim);

// --- Device-Side Expectation Value Reduction ---
// pauli_ops: device array, encoding per qubit: 0=I, 1=X, 2=Y, 3=Z
// d_result: device pointer to a single double (output)
void launchPauliExpectation(const void *deviceState, int num_qubits,
                            const int *d_pauli_ops, double *d_result);

// --- JIT Fused Unitary Dispatch ---
// Applies a 2^k × 2^k unitary to k target qubits (k ≤ 3)
// d_targets: device array of k target qubit indices
// h_unitary: HOST pointer to the unitary matrix (copied to constant memory)
void launchFusedUnitary(void *deviceState, int num_qubits, const int *d_targets,
                        int k, const void *h_unitary);

// --- Kraus Channels ---
void launchApplyKraus1Q(void *deviceState, int num_qubits, int target,
                        const void *matrix, double inv_norm);
void launchApplyKraus2Q(void *deviceState, int num_qubits, int q1, int q2,
                        const void *matrix, double inv_norm);

// --- Memory Management Helpers ---
void* allocateDeviceState(size_t size_bytes);
void freeDeviceState(void* ptr);
void copyDeviceToDevice(void* dst, const void* src, size_t size_bytes);
void setDeviceStateZero(void* ptr, size_t size_bytes);

// --- Adjoint Helpers ---
void launchDerivativeRY(void* out, const void* in, int num_qubits, int target, double angle);
void launchDerivativeRX(void* out, const void* in, int num_qubits, int target, double angle);
void launchDerivativeRZ(void* out, const void* in, int num_qubits, int target, double angle);

void launchAdjointInnerProduct(const void* dpsi, const void* lambda, double* grad_out, int dim);
void launchApplyPauliTerm(void* out, const void* in, int num_qubits, const int* d_pauli_ops, double coeff);

} // namespace cuda
} // namespace qe
