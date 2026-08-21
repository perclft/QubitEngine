#include <gtest/gtest.h>
#include "backends/StabilizerBackend.hpp"
#include "NoiseModel.hpp"
#include <cmath>
#include <stdexcept>

using namespace qubit_engine;

// --- Backend Construction ---

TEST(StabilizerBackendTest, InitializationDoesNotThrow) {
    EXPECT_NO_THROW({
        StabilizerBackend stab(100);
    });
}

// --- Supported Clifford Gates (No-Throw Verification) ---
// The internal tableau is private and accessors throw, so we can only 
// verify that applying these gates does not crash.

TEST(StabilizerBackendTest, CliffordGatesExecuteWithoutCrash) {
    StabilizerBackend stab(3);
    
    EXPECT_NO_THROW({
        stab.applyHadamard(0);
        stab.applyX(0);
        stab.applyY(0);
        stab.applyZ(0);
        stab.applyPhaseS(0);
        stab.applyCNOT(0, 1);
        stab.applyCZ(1, 2);
        stab.applySWAP(0, 2);
    });
}

// --- Expected Exceptions (Non-Clifford Operations) ---

TEST(StabilizerBackendTest, NonCliffordGatesThrow) {
    StabilizerBackend stab(3);
    
    EXPECT_THROW(stab.applyToffoli(0, 1, 2), std::runtime_error);
    EXPECT_THROW(stab.applyPhaseT(0), std::runtime_error);
    EXPECT_THROW(stab.applyRotationX(0, 1.0), std::runtime_error);
    EXPECT_THROW(stab.applyRotationY(0, 1.0), std::runtime_error);
    EXPECT_THROW(stab.applyRotationZ(0, 1.0), std::runtime_error);
}

// --- Expected Exceptions (Observation & Analysis Blocks) ---
// The prototype explicitly blocks exponential scaling lookups and 
// advanced stabilizer projective measurements.

TEST(StabilizerBackendTest, ObservationMethodsThrow) {
    StabilizerBackend stab(3);
    
    // measure(0) is supported, verify it returns 0 or 1
    int outcome = -1;
    EXPECT_NO_THROW({
        outcome = stab.measure(0);
    });
    EXPECT_TRUE(outcome == 0 || outcome == 1);
    
    EXPECT_THROW(stab.getProbabilities(), std::runtime_error);
    EXPECT_THROW(stab.expectationValue("Z"), std::runtime_error);
    EXPECT_THROW(stab.getStateVector(), std::runtime_error);
}

// --- Noise Simulation & Statistical Fidelity Verification ---

TEST(StabilizerBackendTest, DepolarizingNoiseDoesNotThrow) {
    StabilizerBackend stab(1);
    EXPECT_NO_THROW(stab.applyDepolarizingNoise(0.1));
}

TEST(StabilizerBackendTest, DepolarizingNoise1Q_StatisticalFlipRate) {
    const double p = 0.30;
    const double expected_flip_rate = 2.0 * p / 3.0; // 0.20: X and Y flip |0> to |1>, Z leaves |0> unchanged
    const int num_shots = 10000;
    const double std_error = std::sqrt(expected_flip_rate * (1.0 - expected_flip_rate) / num_shots); // ~0.004

    auto channel = makeDepolarizingChannel1Q(p);
    int ones_count = 0;

    for (int shot = 0; shot < num_shots; ++shot) {
        StabilizerBackend stab(1); // Starts in |0>
        stab.applyNoiseChannel1Q(channel, 0);
        if (stab.measure(0) == 1) {
            ones_count++;
        }
    }

    double observed_flip_rate = static_cast<double>(ones_count) / num_shots;
    // Assert observed rate falls within 4 standard errors of theoretical rate (>99.99% confidence)
    EXPECT_NEAR(observed_flip_rate, expected_flip_rate, 4.0 * std_error)
        << "Observed flip rate: " << observed_flip_rate 
        << ", expected: " << expected_flip_rate 
        << ", 4-sigma bound: " << (4.0 * std_error);
}

TEST(StabilizerBackendTest, DepolarizingNoise2Q_StatisticalFlipRate) {
    const double p = 0.30;
    // In 2Q depolarizing channel (16 Pauli products), 8 operators flip q0:
    // (X⊗I, X⊗X, X⊗Y, X⊗Z, Y⊗I, Y⊗X, Y⊗Y, Y⊗Z), each with weight p/15.
    // Expected flip rate on q0 = 8 * (p / 15) = 8 * 0.30 / 15 = 0.160.
    const double expected_flip_rate = 8.0 * p / 15.0; // 0.160
    const int num_shots = 10000;
    const double std_error = std::sqrt(expected_flip_rate * (1.0 - expected_flip_rate) / num_shots); // ~0.00367

    auto channel = makeDepolarizingChannel2Q(p);
    int q0_flips = 0;
    int q1_flips = 0;

    for (int shot = 0; shot < num_shots; ++shot) {
        StabilizerBackend stab(2); // Starts in |00>
        stab.applyNoiseChannel2Q(channel, 0, 1);
        if (stab.measure(0) == 1) q0_flips++;
        if (stab.measure(1) == 1) q1_flips++;
    }

    double observed_rate_q0 = static_cast<double>(q0_flips) / num_shots;
    double observed_rate_q1 = static_cast<double>(q1_flips) / num_shots;

    EXPECT_NEAR(observed_rate_q0, expected_flip_rate, 4.0 * std_error)
        << "Qubit 0 flip rate: " << observed_rate_q0 
        << ", expected: " << expected_flip_rate;
    EXPECT_NEAR(observed_rate_q1, expected_flip_rate, 4.0 * std_error)
        << "Qubit 1 flip rate: " << observed_rate_q1 
        << ", expected: " << expected_flip_rate;
}

