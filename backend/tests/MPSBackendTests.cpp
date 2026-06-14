#include <gtest/gtest.h>
#include "backends/MPSBackend.hpp"
#include "NoiseModel.hpp"
#include <stdexcept>

using namespace qubit_engine;

// --- Backend Construction ---

TEST(MPSBackendTest, InitializationDoesNotThrow) {
    EXPECT_NO_THROW({
        MPSBackend mps(3);
    });
}

// --- Expected Exceptions (Unimplemented Features) ---

TEST(MPSBackendTest, ToffoliNotSupported) {
    MPSBackend mps(3);
    EXPECT_THROW(mps.applyToffoli(0, 1, 2), std::runtime_error);
}

TEST(MPSBackendTest, NonAdjacentTwoQubitGatesWork) {
    MPSBackend mps(3);
    // Apply H(0)
    mps.applyHadamard(0);
    // CNOT(0, 2): Control 0, Target 2. Qubits 0 and 2 are non-adjacent.
    mps.applyCNOT(0, 2);
    
    // State should be (|000> + |101>)/sqrt(2)
    auto probs = mps.getProbabilities();
    ASSERT_EQ(probs.size(), 8);
    EXPECT_NEAR(probs[0], 0.5, 1e-6); // |000>
    EXPECT_NEAR(probs[5], 0.5, 1e-6); // |101> (binary 101 represents qubit 2 = 1, qubit 1 = 0, qubit 0 = 1)
    EXPECT_NEAR(probs[1], 0.0, 1e-6);
    EXPECT_NEAR(probs[2], 0.0, 1e-6);
    EXPECT_NEAR(probs[3], 0.0, 1e-6);
    EXPECT_NEAR(probs[4], 0.0, 1e-6);
    EXPECT_NEAR(probs[6], 0.0, 1e-6);
    EXPECT_NEAR(probs[7], 0.0, 1e-6);
}

TEST(MPSBackendTest, TooLargeStateVectorThrows) {
    MPSBackend mps(31);
    EXPECT_THROW(mps.getStateVector(), std::runtime_error);
}

TEST(MPSBackendTest, MPS_MeasureBasicStates) {
    MPSBackend mps(1);
    EXPECT_EQ(mps.measure(0), 0);
    MPSBackend mps2(1);
    mps2.applyX(0);
    EXPECT_EQ(mps2.measure(0), 1);
}

TEST(MPSBackendTest, MPS_GetProbabilities) {
    MPSBackend mps(1);
    mps.applyHadamard(0);
    auto probs = mps.getProbabilities();
    ASSERT_EQ(probs.size(), 2);
    EXPECT_NEAR(probs[0], 0.5, 1e-6);
    EXPECT_NEAR(probs[1], 0.5, 1e-6);
}

TEST(MPSBackendTest, MPS_ZeroNoisePreservesState) {
    MPSBackend mps(1);
    mps.applyHadamard(0);
    mps.applyDepolarizingNoise(0.0);
    auto probs = mps.getProbabilities();
    EXPECT_NEAR(probs[0], 0.5, 1e-6);
    EXPECT_NEAR(probs[1], 0.5, 1e-6);
}

TEST(MPSBackendTest, MPS_ADPhysicalCorrectness) {
    MPSBackend mps(1);
    auto ad = makeAmplitudeDampingChannel(0.8);
    for (int i = 0; i < 100; ++i) {
        mps.applyNoiseChannel1Q(ad, 0);
    }
    auto probs = mps.getProbabilities();
    EXPECT_NEAR(probs[0], 1.0, 1e-6);
    EXPECT_NEAR(probs[1], 0.0, 1e-6);
}

TEST(MPSBackendTest, MPS_DepolarizingNormalization) {
    MPSBackend mps(2);
    mps.applyHadamard(0);
    mps.applyCNOT(0, 1);
    for (int i = 0; i < 10; ++i) {
        mps.applyDepolarizingNoise(0.1);
    }
    auto probs = mps.getProbabilities();
    double sum = 0.0;
    for (double p : probs) sum += p;
    EXPECT_NEAR(sum, 1.0, 1e-6);
}

// --- Gate Execution (No-Throw Verification) ---
// Since internal `nodes` are private and observation is stubbed out,
// we just verify the math functions applying the gates don't crash.

TEST(MPSBackendTest, SingleQubitGatesExecuteWithoutCrash) {
    MPSBackend mps(1);
    
    EXPECT_NO_THROW({
        mps.applyHadamard(0);
        mps.applyX(0);
        mps.applyY(0);
        mps.applyZ(0);
        mps.applyPhaseS(0);
        mps.applyPhaseT(0);
        mps.applyRotationX(0, 1.23);
        mps.applyRotationY(0, 0.78);
        mps.applyRotationZ(0, 2.1);
    });
}

TEST(MPSBackendTest, AdjacentTwoQubitGatesExecuteWithoutCrash) {
    MPSBackend mps(2);
    
    EXPECT_NO_THROW({
        mps.applyCNOT(0, 1);
        mps.applyCZ(0, 1);
        mps.applySWAP(0, 1);
    });
}
