#pragma once

#include "backends/IQuantumBackend.hpp"
#include <memory>
#include <cstddef>
#include "qubit_engine_export.h"

namespace qubit_engine {

/// @brief Factory responsible for selecting and constructing the appropriate
/// quantum simulation backend based on system capabilities and configuration.
///
/// Extracting this logic from QuantumRegister's constructor enables:
/// - Dependency injection for unit testing (inject mock backends)
/// - Cleaner separation of concerns (register doesn't know about backends)
/// - Centralized backend selection policy
class QUBIT_ENGINE_EXPORT BackendFactory {
public:
  /// @brief Creates the optimal backend for the given qubit count and constraints.
  ///
  /// Selection priority:
  /// 1. CloudBackend (if QUBIT_ENGINE_CLOUD_URL is configured and not forced local)
  /// 2. MPSBackend (if qubit count >= MPS threshold)
  /// 3. CudaBackend (if CUDA is enabled and GPU is available)
  /// 4. CpuBackend (default fallback)
  ///
  /// @param num_qubits Number of qubits to simulate
  /// @param force_local If true, skip Cloud and use local backend only
  /// @return A unique_ptr to the selected backend
  static std::unique_ptr<IQuantumBackend> create(size_t num_qubits,
                                                  bool force_local = false);
};

} // namespace qubit_engine
