#include "../src/backends/MockHardwareBackend.hpp"
#include <gtest/gtest.h>

using namespace qubit_engine;

// ===== Mock Hardware Backend Tests =====

TEST(MockHardwareBackendTest, Initialization) {
  MockHardwareBackend backend(2);
  // Should not throw when executing a core gate simulation routine
  EXPECT_NO_THROW({ backend.applyHadamard(0); });
}

TEST(MockHardwareBackendTest, GetStatePopulatesStateWithNoise) {
  int num_qubits = 2;
  MockHardwareBackend backend(num_qubits);

  auto state = backend.getStateVector();

  // Check that the returned "noisy" state vector matches 2^N dimensions
  EXPECT_EQ(state.size(), 1 << num_qubits);

  // The peak probability should be roughly at index 0
  // due to the stochastic noise profile (0.9 + noise) in the mock
  double max_prob = 0.0;
  int max_idx = -1;

  for (int i = 0; i < state.size(); ++i) {
    auto &c = state[i];
    double prob = std::norm(c); // std::norm(Complex) gets the squared magnitude
    if (prob > max_prob) {
      max_prob = prob;
      max_idx = i;
    }
  }

  // In overwhelmingly most cases, the noise normal dist (mean 0, stddev 0.05)
  // shouldn't overtake the 0.9 baseline defined in the Mock
  EXPECT_EQ(max_idx, 0);
}

TEST(MockHardwareBackendTest, RespectsStateLimitCapping) {
  // The backend limits state visualization to 1024 amplitudes maximum
  int num_qubits = 12; // 2^12 = 4096 (exceeds cap)
  MockHardwareBackend backend(num_qubits);

  auto state = backend.getStateVector();
  EXPECT_EQ(state.size(), 1024);
}
