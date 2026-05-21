#include <gtest/gtest.h>
#include "QuantumRegister.hpp"
#include <random>

using namespace qubit_engine;

// Generate a random circuit of given depth and then apply its inverse.
// Verification: The final state should return to |0...0> with high fidelity.
TEST(ValidationTest, MirrorCircuit) {
    int num_qubits = 6;
    int depth = 20;
    
    QuantumRegister reg(num_qubits, true);
    
    std::mt19937 gen(42); // fixed seed for reproducibility
    std::uniform_int_distribution<> gate_dist(0, 5);
    std::uniform_int_distribution<> q_dist(0, num_qubits - 1);
    std::uniform_real_distribution<> angle_dist(0.0, 6.28318530718);
    
    // Store applied gates so we can invert them
    struct GateRecord {
        int type;
        int q1, q2;
        double angle;
    };
    std::vector<GateRecord> history;
    
    for (int d = 0; d < depth; ++d) {
        for (int q = 0; q < num_qubits; ++q) {
            int g = gate_dist(gen);
            int t = q_dist(gen);
            double a = angle_dist(gen);
            if (g == 0) {
                reg.applyHadamard(t);
                history.push_back({0, t, -1, 0.0});
            } else if (g == 1) {
                reg.applyRotationX(t, a);
                history.push_back({1, t, -1, a});
            } else if (g == 2) {
                reg.applyRotationY(t, a);
                history.push_back({2, t, -1, a});
            } else if (g == 3) {
                int c = q_dist(gen);
                if (c != t) {
                    reg.applyCNOT(c, t);
                    history.push_back({3, c, t, 0.0});
                }
            } else if (g == 4) {
                reg.applyPhaseS(t);
                history.push_back({4, t, -1, 0.0});
            } else if (g == 5) {
                int c = q_dist(gen);
                if (c != t) {
                    reg.applyCZ(c, t);
                    history.push_back({5, c, t, 0.0});
                }
            }
        }
    }
    
    // Reverse the circuit
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        if (it->type == 0) reg.applyHadamard(it->q1); // H is its own inverse
        else if (it->type == 1) reg.applyRotationX(it->q1, -it->angle);
        else if (it->type == 2) reg.applyRotationY(it->q1, -it->angle);
        else if (it->type == 3) reg.applyCNOT(it->q1, it->q2); // CNOT is its own inverse
        else if (it->type == 4) {
            // S inverse is S^3 or Z*S (since S = diag(1, i), S_inv = diag(1, -i))
            // Apply Z then S to effectively get -i
            reg.applyRotationZ(it->q1, -1.57079632679); // -pi/2
        }
        else if (it->type == 5) reg.applyCZ(it->q1, it->q2); // CZ is its own inverse
    }
    
    auto probs = reg.getProbabilities();
    // Prob of measuring |0...0> should be 1.0
    EXPECT_NEAR(probs[0], 1.0, 1e-10);
}
