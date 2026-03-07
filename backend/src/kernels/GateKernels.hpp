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
struct cuDoubleComplex;
void launchFusedUnitary(void *deviceState, int num_qubits, const int *d_targets,
                        int k, const cuDoubleComplex *h_unitary);

} // namespace cuda
} // namespace qe
