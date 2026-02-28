#include "../src/backends/CpuBackend.hpp"
#include <gtest/gtest.h>
#define _USE_MATH_DEFINES
#include <cmath>

using namespace qubit_engine;

// ===== CPU Backend Instantiation Tests =====

TEST(CpuBackendTest, InitializationToZeroState) {
  CpuBackend backend(2);
  auto state = backend.getStateVector();

  EXPECT_EQ(state.size(), 4);
  EXPECT_EQ(state[0], Complex(1.0, 0.0));
  EXPECT_EQ(state[1], Complex(0.0, 0.0));
  EXPECT_EQ(state[2], Complex(0.0, 0.0));
  EXPECT_EQ(state[3], Complex(0.0, 0.0));
}

// ===== Logic Gate Direct Execution =====

TEST(CpuBackendTest, ApplyPauliXFlipsState) {
  CpuBackend backend(1);
  backend.applyX(0);

  auto state = backend.getStateVector();
  EXPECT_EQ(state[0], Complex(0.0, 0.0));
  EXPECT_EQ(state[1], Complex(1.0, 0.0));
}

TEST(CpuBackendTest, ApplyHadamardCreatesSuperposition) {
  CpuBackend backend(1);
  backend.applyHadamard(0);

  auto state = backend.getStateVector();
  Precision inv_sqrt_2 = 1.0 / std::sqrt(2.0);

  EXPECT_NEAR(state[0].real(), inv_sqrt_2, 1e-6);
  EXPECT_NEAR(state[1].real(), inv_sqrt_2, 1e-6);
}

TEST(CpuBackendTest, ApplyCNOTEntanglement) {
  CpuBackend backend(2);
  // Create |+0> by applying H to Qubit 0
  backend.applyHadamard(0);
  // Entangle Q0 and Q1: Output should be Bell State (|00> + |11>)/sqrt(2)
  backend.applyCNOT(0, 1);

  auto state = backend.getStateVector();
  Precision inv_sqrt_2 = 1.0 / std::sqrt(2.0);

  EXPECT_NEAR(state[0].real(), inv_sqrt_2, 1e-6); // |00>
  EXPECT_NEAR(std::abs(state[1]), 0.0, 1e-6);     // |01>
  EXPECT_NEAR(std::abs(state[2]), 0.0, 1e-6);     // |10>
  EXPECT_NEAR(state[3].real(), inv_sqrt_2, 1e-6); // |11>
}

// ===== Advanced Rotations =====

TEST(CpuBackendTest, ApplyRotationY) {
  CpuBackend backend(1);
  // Apply Ry(pi) ~ effectively Pauli X up to a global phase
  backend.applyRotationY(0, M_PI);

  auto state = backend.getStateVector();
  EXPECT_NEAR(state[0].real(), 0.0, 1e-6);
  EXPECT_NEAR(state[1].real(), 1.0, 1e-6);
}

// ===== Expectation Values =====

TEST(CpuBackendTest, ExpectationValueZ) {
  CpuBackend backend(1);
  // Ground state |0> should have <Z> = 1.0
  double expZ = backend.expectationValue("Z");
  EXPECT_NEAR(expZ, 1.0, 1e-6);

  // Excited state |1> should have <Z> = -1.0
  backend.applyX(0);
  double expZ_excited = backend.expectationValue("Z");
  EXPECT_NEAR(expZ_excited, -1.0, 1e-6);
}

TEST(CpuBackendTest, ProbabilitiesExtraction) {
  CpuBackend backend(2);
  backend.applyHadamard(0);

  // State is 0.5|00> + 0.5|01> + 0.0|10> + 0.0|11>
  auto probs = backend.getProbabilities();
  EXPECT_EQ(probs.size(), 4);
  EXPECT_NEAR(probs[0], 0.5, 1e-6);
  EXPECT_NEAR(probs[1], 0.5, 1e-6);
  EXPECT_NEAR(probs[2], 0.0, 1e-6);
  EXPECT_NEAR(probs[3], 0.0, 1e-6);
}
