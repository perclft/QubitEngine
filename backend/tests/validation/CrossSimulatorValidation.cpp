#include <gtest/gtest.h>
#include "QuantumRegister.hpp"
#include "backends/CpuBackend.hpp"
#include "backends/MPSBackend.hpp"

#ifdef ENABLE_CUDA
#include "backends/CudaBackend.hpp"
#endif

#ifdef ENABLE_METAL
#include "backends/MetalBackend.hpp"
#endif

#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <random>

using namespace qubit_engine;
using json = nlohmann::json;

namespace {

std::string goldenFilePath(const std::string& circuit_name) {
    const std::string filename = circuit_name + ".json";
    std::vector<std::string> candidates;
#ifdef QUBIT_ENGINE_GOLDEN_DIR
    candidates.emplace_back(std::string(QUBIT_ENGINE_GOLDEN_DIR) + "/" + filename);
#endif
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
    
    Complex inner_product(0,0);
    for (size_t i = 0; i < qe_state.size(); ++i) {
        Complex ref_val(ref_state[i][0].get<double>(), ref_state[i][1].get<double>());
        inner_product += qe_state[i] * std::conj(ref_val);
    }
    
    double fidelity = std::norm(inner_product);
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
    for (int j = 0; j < 4; ++j) {
        reg.applyHadamard(j);
        for (int k = j + 1; k < 4; ++k) {
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

// ============================================================================
// Parameterized Multi-Backend Cross-Consistency Validation Test Suite
// ============================================================================

class CrossBackendNumericalTest : public ::testing::TestWithParam<size_t> {
protected:
    void SetUp() override {
        num_qubits_ = GetParam();
    }

    size_t num_qubits_;
};

INSTANTIATE_TEST_SUITE_P(
    MultiQubitCircuits,
    CrossBackendNumericalTest,
    ::testing::Values(3, 4, 5)
);

TEST_P(CrossBackendNumericalTest, CompareStateVectorsAcrossBackends) {
    size_t n = GetParam();
    CpuBackend cpu_backend(n);
    MPSBackend mps_backend(n, 64); // Max bond dim 64 to ensure exact state

#ifdef ENABLE_CUDA
    std::unique_ptr<CudaBackend> cuda_backend;
    try {
        cuda_backend = std::make_unique<CudaBackend>(n);
    } catch (const std::exception& e) {
        std::cout << "[CrossBackendTest] CUDA unavailable: " << e.what() << std::endl;
    }
#endif

#ifdef ENABLE_METAL
    std::unique_ptr<MetalBackend> metal_backend;
    try {
        metal_backend = std::make_unique<MetalBackend>(n);
    } catch (const std::exception& e) {
        std::cout << "[CrossBackendTest] Metal unavailable: " << e.what() << std::endl;
    }
#endif

    // Build deterministic pseudo-random circuit
    std::mt19937 rng(42 + static_cast<unsigned int>(n));
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    std::uniform_int_distribution<size_t> qubit_dist(0, n - 1);

    for (int layer = 0; layer < 5; ++layer) {
        for (size_t q = 0; q < n; ++q) {
            double theta = angle_dist(rng);
            cpu_backend.applyHadamard(q);
            mps_backend.applyHadamard(q);
            cpu_backend.applyRotationY(q, theta);
            mps_backend.applyRotationY(q, theta);

#ifdef ENABLE_CUDA
            if (cuda_backend) {
                cuda_backend->applyHadamard(q);
                cuda_backend->applyRotationY(q, theta);
            }
#endif
#ifdef ENABLE_METAL
            if (metal_backend) {
                metal_backend->applyHadamard(q);
                metal_backend->applyRotationY(q, theta);
            }
#endif
        }

        // CNOT layers
        for (size_t q = 0; q + 1 < n; q += 2) {
            cpu_backend.applyCNOT(q, q + 1);
            mps_backend.applyCNOT(q, q + 1);

#ifdef ENABLE_CUDA
            if (cuda_backend) cuda_backend->applyCNOT(q, q + 1);
#endif
#ifdef ENABLE_METAL
            if (metal_backend) metal_backend->applyCNOT(q, q + 1);
#endif
        }
    }

    auto v_cpu = cpu_backend.getStateVector();
    auto v_mps = mps_backend.getStateVector();

    ASSERT_EQ(v_cpu.size(), v_mps.size());
    for (size_t i = 0; i < v_cpu.size(); ++i) {
        EXPECT_NEAR(v_mps[i].real(), v_cpu[i].real(), 1e-5) << "MPS real mismatch at index " << i;
        EXPECT_NEAR(v_mps[i].imag(), v_cpu[i].imag(), 1e-5) << "MPS imag mismatch at index " << i;
    }

#ifdef ENABLE_CUDA
    if (cuda_backend) {
        auto v_cuda = cuda_backend->getStateVector();
        ASSERT_EQ(v_cpu.size(), v_cuda.size());
        for (size_t i = 0; i < v_cpu.size(); ++i) {
            EXPECT_NEAR(v_cuda[i].real(), v_cpu[i].real(), 1e-5) << "CUDA real mismatch at index " << i;
            EXPECT_NEAR(v_cuda[i].imag(), v_cpu[i].imag(), 1e-5) << "CUDA imag mismatch at index " << i;
        }
        std::cout << "[CrossBackendTest] CUDA vs CPU state vector match verified within 1e-5 (" << n << " qubits)" << std::endl;
    }
#endif

#ifdef ENABLE_METAL
    if (metal_backend) {
        auto v_metal = metal_backend->getStateVector();
        ASSERT_EQ(v_cpu.size(), v_metal.size());
        for (size_t i = 0; i < v_cpu.size(); ++i) {
            EXPECT_NEAR(v_metal[i].real(), v_cpu[i].real(), 1e-5) << "Metal real mismatch at index " << i;
            EXPECT_NEAR(v_metal[i].imag(), v_cpu[i].imag(), 1e-5) << "Metal imag mismatch at index " << i;
        }
        std::cout << "[CrossBackendTest] Metal vs CPU state vector match verified within 1e-5 (" << n << " qubits)" << std::endl;
    }
#endif
}
