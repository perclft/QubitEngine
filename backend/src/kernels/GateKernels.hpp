#pragma once

#include <cstddef>

namespace qe {
namespace cuda {

// Launchers for CUDA Kernels
// These functions wrap the <<<grid, block>>> calls

void launchHadamard(void *deviceState, int num_qubits, int target);
void launchapplyX(void *deviceState, int num_qubits, int target);
void launchapplyY(void *deviceState, int num_qubits, int target);
void launchapplyZ(void *deviceState, int num_qubits, int target);
void launchRotationY(void *deviceState, int num_qubits, int target,
                     double angle);

} // namespace cuda
} // namespace qe
