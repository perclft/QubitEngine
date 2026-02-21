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
void launchRotationZ(void *deviceState, int num_qubits, int target,
                     double angle);

// --- Measurement Helpers ---
void launchComputeProbabilities(const void *deviceState, double *deviceProbs,
                                int dim);

} // namespace cuda
} // namespace qe
