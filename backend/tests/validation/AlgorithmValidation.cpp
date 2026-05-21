#include <gtest/gtest.h>
#include "QuantumRegister.hpp"
#include "NoiseModel.hpp"
#include <cmath>
#include <iostream>

using namespace qubit_engine;

// --- Bernstein-Vazirani ---
// Tests deterministic bit string discovery
TEST(AlgorithmValidationTest, BernsteinVazirani) {
    int num_qubits = 5;
    QuantumRegister reg(num_qubits, true);
    
    // Hidden string s = 1011 (binary 11)
    int s = 11;
    
    // Setup
    for (int i = 0; i < num_qubits - 1; ++i) {
        reg.applyHadamard(i);
    }
    reg.applyX(num_qubits - 1);
    reg.applyHadamard(num_qubits - 1);
    
    // Oracle
    for (int i = 0; i < num_qubits - 1; ++i) {
        if ((s >> i) & 1) {
            reg.applyCNOT(i, num_qubits - 1);
        }
    }
    
    // Uncompute
    for (int i = 0; i < num_qubits - 1; ++i) {
        reg.applyHadamard(i);
    }
    
    // Measure
    int measured_s = 0;
    for (int i = 0; i < num_qubits - 1; ++i) {
        if (reg.measure(i) == 1) {
            measured_s |= (1 << i);
        }
    }
    
    EXPECT_EQ(measured_s, s);
}

// --- Deutsch-Jozsa ---
TEST(AlgorithmValidationTest, DeutschJozsa) {
    int num_qubits = 4;
    
    // Constant Oracle (always 0)
    {
        QuantumRegister reg(num_qubits, true);
        for (int i = 0; i < num_qubits - 1; ++i) reg.applyHadamard(i);
        reg.applyX(num_qubits - 1);
        reg.applyHadamard(num_qubits - 1);
        
        // Oracle: do nothing (f(x) = 0)
        
        for (int i = 0; i < num_qubits - 1; ++i) reg.applyHadamard(i);
        
        int measure_sum = 0;
        for (int i = 0; i < num_qubits - 1; ++i) {
            measure_sum += reg.measure(i);
        }
        EXPECT_EQ(measure_sum, 0); // All 0s for constant
    }
    
    // Balanced Oracle (CNOTs from all data to target)
    {
        QuantumRegister reg(num_qubits, true);
        for (int i = 0; i < num_qubits - 1; ++i) reg.applyHadamard(i);
        reg.applyX(num_qubits - 1);
        reg.applyHadamard(num_qubits - 1);
        
        // Oracle: balanced
        for (int i = 0; i < num_qubits - 1; ++i) {
            reg.applyCNOT(i, num_qubits - 1);
        }
        
        for (int i = 0; i < num_qubits - 1; ++i) reg.applyHadamard(i);
        
        int measure_sum = 0;
        for (int i = 0; i < num_qubits - 1; ++i) {
            measure_sum += reg.measure(i);
        }
        EXPECT_GT(measure_sum, 0); // At least one 1 for balanced
    }
}

// --- GHZ State Validation ---
TEST(AlgorithmValidationTest, GHZStateAmplitudes) {
    int num_qubits = 4;
    QuantumRegister reg(num_qubits, true);
    
    reg.applyHadamard(0);
    for (int i = 0; i < num_qubits - 1; ++i) {
        reg.applyCNOT(i, i + 1);
    }
    
    auto probs = reg.getProbabilities();
    ASSERT_EQ(probs.size(), 1ULL << num_qubits);
    
    // Only |0000> and |1111> should have probability ~0.5
    EXPECT_NEAR(probs[0], 0.5, 1e-10);
    EXPECT_NEAR(probs.back(), 0.5, 1e-10);
    
    // All others should be 0
    for (size_t i = 1; i < probs.size() - 1; ++i) {
        EXPECT_NEAR(probs[i], 0.0, 1e-10);
    }
}

// --- Quantum Teleportation ---
TEST(AlgorithmValidationTest, QuantumTeleportation) {
    QuantumRegister reg(3, true);
    
    // Prepare q0 in |+> state
    reg.applyHadamard(0);
    
    // Create Bell pair between q1 and q2
    reg.applyHadamard(1);
    reg.applyCNOT(1, 2);
    
    // Alice performs Bell measurement on q0 and q1
    reg.applyCNOT(0, 1);
    reg.applyHadamard(0);
    
    // We can't do dynamic conditionals directly yet if no IR supports it natively,
    // but we can compute the probabilities for q2 conditioned on q0/q1 outcomes,
    // or just apply the deferred measurement principle (use CNOT/CZ directly).
    reg.applyCNOT(1, 2);
    reg.applyCZ(0, 2);
    
    // State of q2 should now be |+>
    // We can measure q2 in X basis (apply H then measure)
    reg.applyHadamard(2);
    auto probs = reg.getProbabilities();
    
    // In computational basis, the full state |x>|y>|0> has equal probability for all x,y.
    // So if we trace out q0,q1, the prob of q2=0 is 1.0 (since it's |+> before H, so |0> after H).
    double prob_q2_is_0 = 0.0;
    for (size_t i = 0; i < probs.size(); ++i) {
        if ((i & 4) == 0) { // q2 is 0
            prob_q2_is_0 += probs[i];
        }
    }
    EXPECT_NEAR(prob_q2_is_0, 1.0, 1e-10);
}
