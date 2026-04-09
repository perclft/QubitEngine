#include <gtest/gtest.h>
#include "../src/QuantumRegister.hpp"
#include "../src/backends/Types.hpp"
#include <cmath>

#ifdef __APPLE__
#include "../src/backends/MetalBackend.hpp"

using namespace qubit_engine;

class MetalBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Run tests only if Metal initialization succeeds (e.g. running on an actual Mac)
        try {
            backend = std::make_unique<MetalBackend>(3); // 3 qubits
        } catch (const std::exception& e) {
            GTEST_SKIP() << "Skipping Metal tests, no supported hardware found: " << e.what();
        }
    }

    std::unique_ptr<MetalBackend> backend;
    
    // Helper to check probability bounds
    void ExpectProbClose(double expected, double actual) {
        EXPECT_NEAR(expected, actual, 1e-4);
    }
};

TEST_F(MetalBackendTest, InitialState) {
    if (!backend) return;
    auto probs = backend->getProbabilities();
    ExpectProbClose(1.0, probs[0]);
    for (size_t i = 1; i < probs.size(); ++i) {
        ExpectProbClose(0.0, probs[i]);
    }
}

TEST_F(MetalBackendTest, HadamardGate) {
    if (!backend) return;
    backend->applyHadamard(0);
    auto probs = backend->getProbabilities();
    ExpectProbClose(0.5, probs[0]); // |000>
    ExpectProbClose(0.5, probs[1]); // |001>
}

TEST_F(MetalBackendTest, PauliXGate) {
    if (!backend) return;
    backend->applyX(1);
    auto probs = backend->getProbabilities();
    ExpectProbClose(1.0, probs[2]); // |010>
    ExpectProbClose(0.0, probs[0]);
}

TEST_F(MetalBackendTest, CNOTGate) {
    if (!backend) return;
    backend->applyX(0);           // |001>
    backend->applyCNOT(0, 1);     // |011>
    auto probs = backend->getProbabilities();
    ExpectProbClose(1.0, probs[3]);
}

TEST_F(MetalBackendTest, ExpectationValueZ) {
    if (!backend) return;
    backend->applyX(0); // Qubit 0 is |1> (Z = -1)
    // Qubits 1, 2 are |0> (Z = +1)
    
    double expZ0 = backend->expectationValue("ZII");
    double expZ1 = backend->expectationValue("IZI");
    double expZZ = backend->expectationValue("ZZI");
    
    EXPECT_NEAR(-1.0, expZ0, 1e-4);
    EXPECT_NEAR(1.0, expZ1, 1e-4);
    EXPECT_NEAR(-1.0, expZZ, 1e-4);
}

TEST_F(MetalBackendTest, MeasureQubit) {
    if (!backend) return;
    backend->applyX(2); // |100>
    int outcome = backend->measure(2);
    EXPECT_EQ(1, outcome);
    
    auto probs = backend->getProbabilities();
    ExpectProbClose(1.0, probs[4]);
}

TEST_F(MetalBackendTest, RotationGates) {
    if (!backend) return;
    backend->applyRotationX(0, M_PI / 2.0);
    auto probs = backend->getProbabilities();
    ExpectProbClose(0.5, probs[0]);
    ExpectProbClose(0.5, probs[1]);
    
    backend->applyRotationY(1, M_PI);
    probs = backend->getProbabilities();
    ExpectProbClose(0.5, probs[2]);
    ExpectProbClose(0.5, probs[3]);
    ExpectProbClose(0.0, probs[0]);
}

#else
// Dummy test for non-Apple platforms to avoid warning of empty test file
TEST(MetalBackendTest, NotSupported) {
    SUCCEED();
}
#endif
