#include <gtest/gtest.h>
#include "backends/MPSBackend.hpp"
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

TEST(MPSBackendTest, NonAdjacentTwoQubitGatesThrow) {
    MPSBackend mps(3);
    // CNOT on non-adjacent qubits requires SWAP network which is unimplemented
    EXPECT_THROW(mps.applyCNOT(0, 2), std::runtime_error);
}

TEST(MPSBackendTest, TooLargeStateVectorThrows) {
    MPSBackend mps(31);
    EXPECT_THROW(mps.getStateVector(), std::runtime_error);
}

// --- API Surface Existence (Stubs / Incomplete) ---
// Since the prototype returns stubbed 0s or empty vectors for observation
// methods, we just test that invoking them doesn't segfault.

TEST(MPSBackendTest, ObservationMethodsStubbed) {
    MPSBackend mps(3);
    
    // measure() returns 0
    EXPECT_EQ(mps.measure(0), 0);
    
    // expectationValue() returns 0.0
    EXPECT_EQ(mps.expectationValue("Z"), 0.0);
    
    // getProbabilities() returns empty
    EXPECT_TRUE(mps.getProbabilities().empty());
    
    // getStateVector() returns empty for <= 30 qubits
    EXPECT_TRUE(mps.getStateVector().empty());
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
