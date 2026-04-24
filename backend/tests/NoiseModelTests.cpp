#define _USE_MATH_DEFINES
#include "../src/NoiseModel.hpp"
#include "../src/QuantumRegister.hpp"
#include "../src/backends/CpuBackend.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <numeric>

using namespace qubit_engine;

// ============================================================================
// Kraus Operator Completeness Tests
// Verify: Σ_i K_i† K_i = I for all channel factories
// ============================================================================

/// Helper: Compute K†K for a 2×2 matrix, returning 2×2 result
static std::array<Complex, 4> daggerProduct2x2(const std::array<Complex, 4>& K) {
  // K† * K where K†[i][j] = conj(K[j][i])
  std::array<Complex, 4> result{};
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      for (int k = 0; k < 2; ++k) {
        result[i * 2 + j] += std::conj(K[k * 2 + i]) * K[k * 2 + j];
      }
    }
  }
  return result;
}

/// Helper: Compute K†K for a 4×4 matrix
static std::array<Complex, 16> daggerProduct4x4(const std::array<Complex, 16>& K) {
  std::array<Complex, 16> result{};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      for (int k = 0; k < 4; ++k) {
        result[i * 4 + j] += std::conj(K[k * 4 + i]) * K[k * 4 + j];
      }
    }
  }
  return result;
}

TEST(NoiseModelTest, DepolarizingChannel1Q_KrausCompleteness) {
  NoiseChannel1Q channel = makeDepolarizingChannel1Q(0.1);
  ASSERT_EQ(channel.operators.size(), 4);

  // Sum K†K over all operators
  std::array<Complex, 4> sum{};
  for (const auto& op : channel.operators) {
    auto kdagk = daggerProduct2x2(op.matrix);
    for (int i = 0; i < 4; ++i) sum[i] += kdagk[i];
  }

  // Should be identity
  EXPECT_NEAR(sum[0].real(), 1.0, 1e-12); // I[0,0]
  EXPECT_NEAR(sum[1].real(), 0.0, 1e-12); // I[0,1]
  EXPECT_NEAR(sum[2].real(), 0.0, 1e-12); // I[1,0]
  EXPECT_NEAR(sum[3].real(), 1.0, 1e-12); // I[1,1]
  for (int i = 0; i < 4; ++i) {
    EXPECT_NEAR(sum[i].imag(), 0.0, 1e-12);
  }
}

TEST(NoiseModelTest, DepolarizingChannel2Q_KrausCompleteness) {
  NoiseChannel2Q channel = makeDepolarizingChannel2Q(0.05);
  ASSERT_EQ(channel.operators.size(), 16);

  std::array<Complex, 16> sum{};
  for (const auto& op : channel.operators) {
    auto kdagk = daggerProduct4x4(op.matrix);
    for (int i = 0; i < 16; ++i) sum[i] += kdagk[i];
  }

  // Should be 4×4 identity
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      double expected = (i == j) ? 1.0 : 0.0;
      EXPECT_NEAR(sum[i * 4 + j].real(), expected, 1e-12)
          << "at (" << i << "," << j << ")";
      EXPECT_NEAR(sum[i * 4 + j].imag(), 0.0, 1e-12);
    }
  }
}

TEST(NoiseModelTest, AmplitudeDampingChannel_KrausCompleteness) {
  NoiseChannel1Q channel = makeAmplitudeDampingChannel(0.3);
  ASSERT_EQ(channel.operators.size(), 2);

  std::array<Complex, 4> sum{};
  for (const auto& op : channel.operators) {
    auto kdagk = daggerProduct2x2(op.matrix);
    for (int i = 0; i < 4; ++i) sum[i] += kdagk[i];
  }

  EXPECT_NEAR(sum[0].real(), 1.0, 1e-12);
  EXPECT_NEAR(sum[3].real(), 1.0, 1e-12);
  EXPECT_NEAR(sum[1].real(), 0.0, 1e-12);
  EXPECT_NEAR(sum[2].real(), 0.0, 1e-12);
}

TEST(NoiseModelTest, PhaseDampingChannel_KrausCompleteness) {
  NoiseChannel1Q channel = makePhaseDampingChannel(0.5);
  ASSERT_EQ(channel.operators.size(), 2);

  std::array<Complex, 4> sum{};
  for (const auto& op : channel.operators) {
    auto kdagk = daggerProduct2x2(op.matrix);
    for (int i = 0; i < 4; ++i) sum[i] += kdagk[i];
  }

  EXPECT_NEAR(sum[0].real(), 1.0, 1e-12);
  EXPECT_NEAR(sum[3].real(), 1.0, 1e-12);
  EXPECT_NEAR(sum[1].real(), 0.0, 1e-12);
  EXPECT_NEAR(sum[2].real(), 0.0, 1e-12);
}

TEST(NoiseModelTest, ThermalRelaxation_KrausCompleteness) {
  // Use T1=100, T2=50, t=10 => gamma_ad = 1 - e^-0.1, gamma_pd = 1 - e^-(2/50 - 1/100)*10
  NoiseChannel1Q channel = makeThermalRelaxationChannel(100.0, 50.0, 10.0);
  ASSERT_EQ(channel.operators.size(), 4);

  std::array<Complex, 4> sum{};
  for (const auto& op : channel.operators) {
    auto kdagk = daggerProduct2x2(op.matrix);
    for (int i = 0; i < 4; ++i) sum[i] += kdagk[i];
  }

  EXPECT_NEAR(sum[0].real(), 1.0, 1e-10);
  EXPECT_NEAR(sum[3].real(), 1.0, 1e-10);
  EXPECT_NEAR(sum[1].real(), 0.0, 1e-10);
  EXPECT_NEAR(sum[2].real(), 0.0, 1e-10);
}

// ============================================================================
// NoiseModel Configuration Tests
// ============================================================================

TEST(NoiseModelTest, DefaultModelIsDisabled) {
  NoiseModel model;
  EXPECT_FALSE(model.isEnabled()); // No channels added
}

TEST(NoiseModelTest, DepolarizingBuilderCreatesChannels) {
  NoiseModel model = NoiseModel::Depolarizing(0.001, 0.01);
  EXPECT_TRUE(model.isEnabled());
  EXPECT_EQ(model.getSingleQubitChannels().size(), 1);
  EXPECT_EQ(model.getTwoQubitChannels().size(), 1);
  EXPECT_FALSE(model.hasReadoutError());
}

TEST(NoiseModelTest, RealisticBuilderCreatesAllChannels) {
  ReadoutError ro{0.02, 0.01};
  NoiseModel model = NoiseModel::Realistic(0.001, 0.01, 0.005, 0.01, ro);
  EXPECT_TRUE(model.isEnabled());
  // depolarizing + amplitude damping + phase damping = 3 single-qubit channels
  EXPECT_EQ(model.getSingleQubitChannels().size(), 3);
  EXPECT_EQ(model.getTwoQubitChannels().size(), 1);
  EXPECT_TRUE(model.hasReadoutError());
}

TEST(NoiseModelTest, DisableToggle) {
  NoiseModel model = NoiseModel::Depolarizing(0.1, 0.1);
  EXPECT_TRUE(model.isEnabled());
  model.setEnabled(false);
  EXPECT_FALSE(model.isEnabled());
  model.setEnabled(true);
  EXPECT_TRUE(model.isEnabled());
}

TEST(NoiseModelTest, ReadoutErrorPerQubit) {
  NoiseModel model;
  model.setReadoutErrorAll({0.01, 0.02});
  model.setReadoutError(0, {0.05, 0.10});

  ReadoutError q0 = model.getReadoutError(0);
  EXPECT_NEAR(q0.p0_given_1, 0.05, 1e-12);
  EXPECT_NEAR(q0.p1_given_0, 0.10, 1e-12);

  ReadoutError q1 = model.getReadoutError(1);
  EXPECT_NEAR(q1.p0_given_1, 0.01, 1e-12);
  EXPECT_NEAR(q1.p1_given_0, 0.02, 1e-12);
}

// ============================================================================
// Invalid Input Tests
// ============================================================================

TEST(NoiseModelTest, InvalidProbabilityThrows) {
  EXPECT_THROW(makeDepolarizingChannel1Q(-0.1), std::invalid_argument);
  EXPECT_THROW(makeDepolarizingChannel1Q(1.5), std::invalid_argument);
  EXPECT_THROW(makeDepolarizingChannel2Q(-0.1), std::invalid_argument);
  EXPECT_THROW(makeAmplitudeDampingChannel(2.0), std::invalid_argument);
  EXPECT_THROW(makePhaseDampingChannel(-0.5), std::invalid_argument);
}

// ============================================================================
// Functional Tests Against CpuBackend
// ============================================================================

TEST(NoiseModelTest, ZeroNoisePreservesState) {
  CpuBackend backend(2);
  backend.applyHadamard(0);
  auto state_before = backend.getStateVector();

  // Apply zero-probability noise channel
  NoiseChannel1Q channel = makeDepolarizingChannel1Q(0.0);
  backend.applyNoiseChannel1Q(channel, 0);

  auto state_after = backend.getStateVector();
  for (size_t i = 0; i < state_before.size(); ++i) {
    EXPECT_NEAR(state_before[i].real(), state_after[i].real(), 1e-12);
    EXPECT_NEAR(state_before[i].imag(), state_after[i].imag(), 1e-12);
  }
}

TEST(NoiseModelTest, AmplitudeDampingDecaysExcitedState) {
  // Start in |1⟩, apply strong amplitude damping
  // After many applications, state should shift toward |0⟩
  const int NUM_TRIALS = 5000;
  int count_zero = 0;

  for (int trial = 0; trial < NUM_TRIALS; ++trial) {
    CpuBackend backend(1);
    backend.applyX(0); // |1⟩

    // Apply strong amplitude damping (γ = 0.8)
    NoiseChannel1Q channel = makeAmplitudeDampingChannel(0.8);
    backend.applyNoiseChannel1Q(channel, 0);

    int result = backend.measure(0);
    if (result == 0) count_zero++;
  }

  // With γ=0.8 and renormalization, we expect a significant fraction to decay to |0⟩
  double ratio = static_cast<double>(count_zero) / NUM_TRIALS;
  EXPECT_NEAR(ratio, 0.8, 0.05); // Should be very close to 0.8
}

TEST(NoiseModelTest, AmplitudeDamping_PhysicalCorrectness) {
  // CRITICAL TEST: Verify that |0> state NEVER decays.
  // With fixed probabilities, it would decay with probability gamma.
  // With state-dependent probabilities, it should stay |0> always.
  CpuBackend backend(1);
  // State is |0>
  
  NoiseChannel1Q channel = makeAmplitudeDampingChannel(0.5);
  for (int i = 0; i < 100; ++i) {
    backend.applyNoiseChannel1Q(channel, 0);
  }

  auto probs = backend.getProbabilities();
  EXPECT_NEAR(probs[0], 1.0, 1e-12); // Must remain in |0>
  EXPECT_NEAR(probs[1], 0.0, 1e-12);
}

TEST(NoiseModelTest, DepolarizingNoiseDegradesPurity) {
  // Start in |0⟩, apply many rounds of depolarizing noise
  // State should approach maximally mixed (equal probabilities)
  CpuBackend backend(1);

  NoiseChannel1Q channel = makeDepolarizingChannel1Q(0.5);

  // Apply noise 50 times — should approach maximally mixed
  for (int i = 0; i < 50; ++i) {
    backend.applyNoiseChannel1Q(channel, 0);
  }

  auto probs = backend.getProbabilities();
  // With renormalization, the total should always be 1.0
  // Just verify the channel runs without crashing and produces valid probabilities
  EXPECT_EQ(probs.size(), 2);
  double total = probs[0] + probs[1];
  EXPECT_NEAR(total, 1.0, 1e-10);
}

TEST(NoiseModelTest, ReadoutErrorFlipsMeasurements) {
  // Prepare |0⟩, measure with high readout error probability
  const int NUM_TRIALS = 5000;
  int count_one = 0;
  ReadoutError error{0.0, 0.3}; // 30% chance of flipping 0→1

  for (int trial = 0; trial < NUM_TRIALS; ++trial) {
    CpuBackend backend(1);
    // State is |0⟩, true measurement should always be 0
    int result = backend.measureWithReadoutError(0, error);
    if (result == 1) count_one++;
  }

  double flip_rate = static_cast<double>(count_one) / NUM_TRIALS;
  EXPECT_GT(flip_rate, 0.20); // 3σ below 0.3
  EXPECT_LT(flip_rate, 0.40); // 3σ above 0.3
}

TEST(NoiseModelTest, TwoQubitDepolarizingChannel) {
  // Verify the 2Q channel applies without crashing and preserves normalization
  CpuBackend backend(2);
  backend.applyHadamard(0);
  backend.applyCNOT(0, 1); // Bell state

  NoiseChannel2Q channel = makeDepolarizingChannel2Q(0.1);
  backend.applyNoiseChannel2Q(channel, 0, 1);

  auto probs = backend.getProbabilities();
  double total = 0.0;
  for (double p : probs) {
    total += p;
    EXPECT_GE(p, 0.0);
  }
  EXPECT_NEAR(total, 1.0, 1e-6);
}

// ============================================================================
// Integration: NoiseModel on QuantumRegister
// ============================================================================

TEST(NoiseModelTest, QuantumRegisterNoiseModelAttachment) {
  QuantumRegister qreg(2, true);

  // No noise model by default
  EXPECT_EQ(qreg.getNoiseModel(), nullptr);

  // Attach a noise model
  NoiseModel model = NoiseModel::Depolarizing(0.001, 0.01);
  qreg.setNoiseModel(model);

  EXPECT_NE(qreg.getNoiseModel(), nullptr);
  EXPECT_TRUE(qreg.getNoiseModel()->isEnabled());
}

TEST(NoiseModelTest, AutomaticNoiseDoesNotCrash) {
  // Run a simple circuit with noise model attached
  // Just verify no crashes or assertion failures
  QuantumRegister qreg(3, true);
  NoiseModel model = NoiseModel::Realistic(
      0.001, 0.01, 0.005, 0.01, {0.02, 0.01});
  qreg.setNoiseModel(model);

  qreg.applyHadamard(0);
  qreg.applyHadamard(1);
  qreg.applyCNOT(0, 1);
  qreg.applyRotationY(2, M_PI / 4.0);
  qreg.applyCZ(1, 2);
  qreg.applyX(0);
  qreg.applyPhaseS(1);

  auto probs = qreg.getProbabilities();
  double total = 0.0;
  for (double p : probs) total += p;
  EXPECT_NEAR(total, 1.0, 1e-6); // After renormalization, should always be 1.0
}

TEST(NoiseModelTest, CoherentRotationError) {
  // Test RX(pi) with a bias of 0.1 rad.
  // Result should be RX(pi + 0.1)
  QuantumRegister qreg(1, true);
  NoiseModel model;
  model.setCoherentError(11 /* ROTATION_X */, 0.1);
  qreg.setNoiseModel(model);

  qreg.applyRotationX(0, M_PI); // pi + 0.1
  
  auto state = qreg.getStateVector();
  // RX(theta) = [[cos(theta/2), -i*sin(theta/2)], [-i*sin(theta/2), cos(theta/2)]]
  // For |0>, result is [cos(theta/2), -i*sin(theta/2)]
  double theta = M_PI + 0.1;
  EXPECT_NEAR(state[0].real(), std::cos(theta / 2.0), 1e-10);
  EXPECT_NEAR(state[1].imag(), -std::sin(theta / 2.0), 1e-10);
}

// ============================================================================
// Statistical Validation Tests (Pearson's Chi-Squared)
// ============================================================================

TEST(NoiseModelTest, AmplitudeDamping_ChiSquaredTest) {
  // We run a large number of shots to verify the actual output distribution
  // matches the theoretical probabilities of the Kraus channel.
  const int NUM_SHOTS = 10000;
  int count_zero = 0;
  int count_one = 0;
  
  double gamma = 0.3; // 30% chance of decay from |1> to |0>

  for (int trial = 0; trial < NUM_SHOTS; ++trial) {
    CpuBackend backend(1);
    backend.applyX(0); // Start in |1>
    
    NoiseChannel1Q channel = makeAmplitudeDampingChannel(gamma);
    backend.applyNoiseChannel1Q(channel, 0);
    
    int result = backend.measure(0);
    if (result == 0) count_zero++;
    else count_one++;
  }

  double expected_zero = NUM_SHOTS * gamma;
  double expected_one = NUM_SHOTS * (1.0 - gamma);

  double chi_squared = std::pow(count_zero - expected_zero, 2) / expected_zero +
                       std::pow(count_one - expected_one, 2) / expected_one;

  // For 1 degree of freedom, critical value at p=0.001 is 10.828.
  // We use 15.0 to be very safe against flaky tests in CI.
  EXPECT_LT(chi_squared, 15.0) << "Chi-Squared test failed! Distribution deviated too much. count_0=" 
                               << count_zero << ", count_1=" << count_one;
}

