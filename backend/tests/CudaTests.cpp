#include "../src/Types.hpp"
#include <cmath>
#include <complex>
#include <gtest/gtest.h>
#include <vector>


// We only compile these tests if ENABLE_CUDA is defined
#ifdef ENABLE_CUDA
#include "../src/backends/CudaBackend.hpp"

using namespace qubit_engine;

// Test Fixture for CudaBackend
class CudaBackendTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize 2-qubit backend
    // We use strict 2 qubits for these basic tests
    backend = new CudaBackend(2);
  }

  void TearDown() override { delete backend; }

  CudaBackend *backend;
};

TEST_F(CudaBackendTest, Initialization) {
  // Should initiate to |00> -> [1, 0, 0, 0]
  auto state = backend->getStateVector();
  EXPECT_EQ(state.size(), 4);
  EXPECT_EQ(state[0], Complex(1.0, 0.0));
  EXPECT_EQ(state[1], Complex(0.0, 0.0));
  EXPECT_EQ(state[2], Complex(0.0, 0.0));
  EXPECT_EQ(state[3], Complex(0.0, 0.0));
}

TEST_F(CudaBackendTest, ApplyHadamard) {
  // Apply H to qubit 0: |00> -> (|00> + |01>) / sqrt(2)
  // Note: Qubit 0 corresponds to the least significant bit (stride 1)

  backend->applyHadamard(0);
  auto state = backend->getStateVector();

  double is2 = 1.0 / std::sqrt(2.0);

  // Indices: 0 (|00>) and 1 (|01>) should be non-zero
  EXPECT_NEAR(state[0].real(), is2, 1e-6);
  EXPECT_NEAR(state[1].real(), is2, 1e-6);
  EXPECT_NEAR(std::abs(state[2]), 0.0, 1e-6);
  EXPECT_NEAR(std::abs(state[3]), 0.0, 1e-6);
}

TEST_F(CudaBackendTest, ApplyX) {
  // |00> -> X(0) -> |01>
  backend->applyX(0);
  auto state = backend->getStateVector();

  EXPECT_NEAR(std::abs(state[0]), 0.0, 1e-6); // |00>
  EXPECT_NEAR(state[1].real(), 1.0, 1e-6);    // |01>
}

TEST_F(CudaBackendTest, ApplyCNOT) {
  // |00> -> H(0) -> |+0> = (|00> + |01>)/sqrt(2)
  // CNOT(0, 1): Control 0, Target 1.
  // If Q0 is 1, Flip Q1.
  // State |00>: Q0=0 -> No Flip -> |00>
  // State |01>: Q0=1 -> Flip Q1 -> |11> (Index 3)
  // Result: (|00> + |11>)/sqrt(2) -> Bell State

  backend->applyHadamard(0);
  backend->applyCNOT(0, 1);

  auto state = backend->getStateVector();
  double is2 = 1.0 / std::sqrt(2.0);

  EXPECT_NEAR(state[0].real(), is2, 1e-6);    // |00>
  EXPECT_NEAR(state[3].real(), is2, 1e-6);    // |11>
  EXPECT_NEAR(std::abs(state[1]), 0.0, 1e-6); // |01>
  EXPECT_NEAR(std::abs(state[2]), 0.0, 1e-6); // |10>
}

TEST_F(CudaBackendTest, ExpectationValue) {
  // |00> -> H(0) -> X expectation on qubit 0
  // <+|X|+> = 1.0
  backend->applyHadamard(0);

  // "XI" -> Q0=X, Q1=I
  double val = backend->expectationValue("XI");
  EXPECT_NEAR(val, 1.0, 1e-5);

  // Z expectation on qubit 0 -> <+|Z|+> = 0
  val = backend->expectationValue("ZI");
  EXPECT_NEAR(val, 0.0, 1e-5);
}

#endif // ENABLE_CUDA
