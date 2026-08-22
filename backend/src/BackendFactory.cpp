#include "BackendFactory.hpp"
#include "ConfigManager.hpp"
#include "backends/CpuBackend.hpp"
#ifdef ENABLE_CUDA
#include "backends/CudaBackend.hpp"
#endif
#ifdef ENABLE_METAL
#include "backends/MetalBackend.hpp"
#endif
#include "backends/MPSBackend.hpp"
#include "backends/CloudBackend.hpp"
#include <spdlog/spdlog.h>

namespace qubit_engine {

std::unique_ptr<IQuantumBackend> BackendFactory::create(size_t num_qubits,
                                                         bool force_local) {
  bool use_local = force_local || ConfigManager::Instance().forceLocalExecution();

  // Priority 1: Cloud Offloading
  auto cloud_url = ConfigManager::Instance().getCloudUrl();
  if (cloud_url.has_value() && !use_local) {
    spdlog::info("BackendFactory: Using CloudBackend (Remote: {})", cloud_url.value());
    return std::make_unique<CloudBackend>(num_qubits, cloud_url.value());
  }

  // Priority 2: Tensor Network (MPS) for large qubit counts
  int mps_threshold = ConfigManager::Instance().getMpsThreshold();
  if (num_qubits >= static_cast<size_t>(mps_threshold) && !use_local) {
    int bond_dim = ConfigManager::Instance().getMpsBondDimension();
    spdlog::info("BackendFactory: Using MPSBackend (bond_dim={})", bond_dim);
    return std::make_unique<MPSBackend>(static_cast<int>(num_qubits), bond_dim);
  }

  // Priority 3: CUDA GPU
#ifdef ENABLE_CUDA
  if (!use_local) {
    try {
      auto backend = std::make_unique<CudaBackend>(num_qubits);
      spdlog::info("BackendFactory: Using CudaBackend (GPU)");
      return backend;
    } catch (...) {
      spdlog::error("BackendFactory: CudaBackend failed. Falling back to CPU.");
    }
  }
#endif

  // Priority 3b: Metal GPU (macOS)
#ifdef ENABLE_METAL
  if (!use_local) {
    try {
      auto backend = std::make_unique<MetalBackend>(num_qubits);
      spdlog::info("BackendFactory: Using MetalBackend (Apple Silicon GPU)");
      return backend;
    } catch (const std::exception& e) {
      spdlog::error("BackendFactory: MetalBackend initialization failed: {}. Falling back to CPU.", e.what());
      const char* strict_accel = std::getenv("QUBIT_STRICT_HARDWARE_ACCEL");
      if (strict_accel && std::string(strict_accel) == "1") {
        throw std::runtime_error(std::string("Strict hardware acceleration required (QUBIT_STRICT_HARDWARE_ACCEL=1) but MetalBackend failed: ") + e.what());
      }
    } catch (...) {
      spdlog::error("BackendFactory: MetalBackend failed with unknown error. Falling back to CPU.");
      const char* strict_accel = std::getenv("QUBIT_STRICT_HARDWARE_ACCEL");
      if (strict_accel && std::string(strict_accel) == "1") {
        throw std::runtime_error("Strict hardware acceleration required (QUBIT_STRICT_HARDWARE_ACCEL=1) but MetalBackend failed with unknown error");
      }
    }
  }
#endif

  // Default: CPU
  return std::make_unique<CpuBackend>(num_qubits, use_local);
}

} // namespace qubit_engine
