#include <gtest/gtest.h>
#include "QuantumRegister.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>

using namespace qubit_engine;
using json = nlohmann::json;

namespace {

std::string goldenFilePath(const std::string& circuit_name) {
    const std::string filename = circuit_name + ".json";
    std::vector<std::string> candidates;
#ifdef QUBIT_ENGINE_GOLDEN_DIR
    candidates.emplace_back(std::string(QUBIT_ENGINE_GOLDEN_DIR) + "/" + filename);
#endif
    // Manual runs from backend/ or repo root
    candidates.emplace_back("tests/validation/golden/" + filename);
    candidates.emplace_back("../../backend/tests/validation/golden/" + filename);
    for (const auto& path : candidates) {
        std::ifstream probe(path);
        if (probe.is_open()) {
            return path;
        }
    }
    return candidates.empty() ? filename : candidates.back();
}

}  // namespace

void ValidateAgainstGolden(const std::string& circuit_name, const QuantumRegister& reg) {
    const std::string path = goldenFilePath(circuit_name);
    std::ifstream f(path);
    if (!f.is_open()) {
        GTEST_SKIP() << "Golden file not found (tried CMake path and CWD fallbacks). "
                        "Run: python scripts/generate_golden_vectors.py";
        return;
    }
    
    json j;
    f >> j;
    
    auto qe_state = reg.getStateVector();
    auto ref_state = j["state_vector"];
    
    ASSERT_EQ(qe_state.size(), ref_state.size());
    
    double fidelity = 0.0;
    Complex inner_product(0,0);
    
    for (size_t i = 0; i < qe_state.size(); ++i) {
        Complex ref_val(ref_state[i][0].get<double>(), ref_state[i][1].get<double>());
        inner_product += qe_state[i] * std::conj(ref_val);
    }
    
    fidelity = std::norm(inner_product);
    EXPECT_NEAR(fidelity, 1.0, 1e-10) << "Failed for circuit: " << circuit_name;
}

TEST(ValidationTest, CrossSimulatorBell) {
    QuantumRegister reg(2, true);
    reg.applyHadamard(0);
    reg.applyCNOT(0, 1);
    ValidateAgainstGolden("bell", reg);
}

TEST(ValidationTest, CrossSimulatorGHZ4) {
    QuantumRegister reg(4, true);
    reg.applyHadamard(0);
    reg.applyCNOT(0, 1);
    reg.applyCNOT(1, 2);
    reg.applyCNOT(2, 3);
    ValidateAgainstGolden("ghz_4q", reg);
}

TEST(ValidationTest, CrossSimulatorQFT4) {
    QuantumRegister reg(4, true);
    // QFT implementation matching Qiskit's output format
    for (int j = 0; j < 4; ++j) {
        reg.applyHadamard(j);
        for (int k = j + 1; k < 4; ++k) {
            // controlled phase: apply CZ with angle? 
            // Phase is diag(1, e^{i theta}). Controlled phase:
            // Since we don't have CPhase directly, we could use decomposition
            // CP(theta) = Rz(theta/2) on target, Rz(theta/2) on control, CNOT(c,t), Rz(-theta/2) on target, CNOT(c,t)
            double theta = M_PI / (1 << (k - j));
            reg.applyRotationZ(k, theta / 2.0);
            reg.applyRotationZ(j, theta / 2.0);
            reg.applyCNOT(j, k);
            reg.applyRotationZ(k, -theta / 2.0);
            reg.applyCNOT(j, k);
        }
    }
    for (int i = 0; i < 2; ++i) {
        reg.applySWAP(i, 3 - i);
    }
    ValidateAgainstGolden("qft_4q", reg);
}
