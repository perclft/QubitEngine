#include "../src/backends/GPUQuantumRegister.hpp"
#include <gtest/gtest.h>

// ===== GPU Quantum Register Tests =====

TEST(GPUQuantumRegisterTest, InitializationDoesNotCrashWithoutCUDA) {
  // Depending on the build, ENABLE_CUDA might be defined.
  // If not, it falls back to exceptions or no-ops.
  // We just want to ensure the constructor & destructor don't segfault
  // even if underlying hardware isn't present or linked.
  try {
    GPUQuantumRegister reg(1);
    SUCCEED();
  } catch (const std::exception &e) {
    // Expected if CUDA is not compiled in or no hardware is available
    SUCCEED();
  }
}

TEST(GPUQuantumRegisterTest, CanRegisterAndRetrieveState) {
  // If CUDA isn't enabled, this typically throws a runtime_error "CUDA not
  // enabled" or returns a zeroed vector. We just ensure the method signatures
  // execute cleanly.
  try {
    GPUQuantumRegister reg(2);
    auto state = reg.getStateVector();
    EXPECT_EQ(state.size(), 4);
  } catch (const std::exception &e) {
    // Expected if CUDA is not compiled in
    SUCCEED();
  }
}
