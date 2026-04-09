#include <gtest/gtest.h>
#include "backends/StabilizerBackend.hpp"
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

// --- Expected Exceptions (Noise) ---

TEST(StabilizerBackendTest, DepolarizingNoiseThrows) {
    StabilizerBackend stab(1);
    EXPECT_THROW(stab.applyDepolarizingNoise(0.1), std::runtime_error);
}
