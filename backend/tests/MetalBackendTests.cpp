#include <gtest/gtest.h>
#include "../src/QuantumRegister.hpp"
#include "../src/NoiseModel.hpp"
#include "../src/Types.hpp"
#include <cmath>
#include <thread>
#include <vector>
#include <iostream>
#include <complex>

#ifdef __APPLE__
#include "../src/backends/MetalBackend.hpp"
#include "../src/backends/CpuBackend.hpp"

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

// ============================================================================
// STRESS TESTS FOR SETBYTES UAF & LIFETIME FIX
// ============================================================================

TEST_F(MetalBackendTest, DenseUnitary1Q_Stress) {
    if (!backend) return;
    const int num_iterations = 10000;
    std::cout << "[STRESS TEST] Starting 1Q Dense Unitary Stress Test (" << num_iterations << " iterations)..." << std::endl;

    for (int i = 0; i < num_iterations; ++i) {
        double theta = (i * 0.001);
        double c = std::cos(theta);
        double s = std::sin(theta);
        // 1Q Rotation matrix U(theta)
        std::vector<Complex> m = {
            Complex(c, 0.0), Complex(-s, 0.0),
            Complex(s, 0.0), Complex(c, 0.0)
        };
        
        backend->applyDenseUnitary({0}, m);

        if (i % 2500 == 2499 || i == num_iterations - 1) {
            auto sv = backend->getStateVector();
            double norm_sq = 0.0;
            for (const auto& amp : sv) norm_sq += std::norm(amp);
            double norm = std::sqrt(norm_sq);

            std::cout << "[STRESS NUMERICS] Iteration " << (i + 1)
                      << " | State Vector Norm: " << norm 
                      << " (diff from 1.0: " << std::abs(norm - 1.0) << ")"
                      << " | c[0]: (" << sv[0].real() << ", " << sv[0].imag() << ")"
                      << " | c[1]: (" << sv[1].real() << ", " << sv[1].imag() << ")"
                      << std::endl;
            
            // Tolerance of 1e-3 for float32 precision after thousands of matrix multiplies
            EXPECT_NEAR(1.0, norm, 1e-3);
        }
    }
}

TEST_F(MetalBackendTest, DenseUnitary2Q_Stress) {
    if (!backend) return;
    const int num_iterations = 10000;
    std::cout << "[STRESS TEST] Starting 2Q Dense Unitary Stress Test (" << num_iterations << " iterations)..." << std::endl;

    for (int i = 0; i < num_iterations; ++i) {
        double theta = (i * 0.0005);
        double c = std::cos(theta);
        double s = std::sin(theta);
        // 2Q Controlled-Rotation matrix
        std::vector<Complex> m(16, Complex(0.0, 0.0));
        m[0] = Complex(1.0, 0.0);
        m[5] = Complex(1.0, 0.0);
        m[10] = Complex(c, 0.0);   m[11] = Complex(-s, 0.0);
        m[14] = Complex(s, 0.0);   m[15] = Complex(c, 0.0);

        backend->applyDenseUnitary({0, 1}, m);

        if (i % 2500 == 2499 || i == num_iterations - 1) {
            auto sv = backend->getStateVector();
            double norm_sq = 0.0;
            for (const auto& amp : sv) norm_sq += std::norm(amp);
            double norm = std::sqrt(norm_sq);

            std::cout << "[STRESS NUMERICS 2Q] Iteration " << (i + 1)
                      << " | State Vector Norm: " << norm 
                      << " (diff from 1.0: " << std::abs(norm - 1.0) << ")"
                      << " | c[0]: (" << sv[0].real() << ", " << sv[0].imag() << ")"
                      << " | c[3]: (" << sv[3].real() << ", " << sv[3].imag() << ")"
                      << std::endl;

            EXPECT_NEAR(1.0, norm, 1e-3);
        }
    }
}

TEST_F(MetalBackendTest, NoiseChannel1Q_Stress) {
    if (!backend) return;
    const int num_iterations = 10000;
    std::cout << "[STRESS TEST] Starting 1Q Noise Channel Stress Test (" << num_iterations << " iterations)..." << std::endl;

    auto channel_depol = makeDepolarizingChannel1Q(0.01);
    auto channel_amp = makeAmplitudeDampingChannel(0.01);
    auto channel_phase = makePhaseDampingChannel(0.01);

    for (int i = 0; i < num_iterations; ++i) {
        backend->applyHadamard(i % 3);
        if (i % 3 == 0) {
            backend->applyNoiseChannel1Q(channel_depol, 0);
        } else if (i % 3 == 1) {
            backend->applyNoiseChannel1Q(channel_amp, 1);
        } else {
            backend->applyNoiseChannel1Q(channel_phase, 2);
        }

        if (i % 2500 == 2499 || i == num_iterations - 1) {
            auto sv = backend->getStateVector();
            double norm_sq = 0.0;
            for (const auto& amp : sv) norm_sq += std::norm(amp);
            double norm = std::sqrt(norm_sq);

            std::cout << "[STRESS NUMERICS NOISE1Q] Iteration " << (i + 1)
                      << " | State Vector Norm: " << norm 
                      << " (diff from 1.0: " << std::abs(norm - 1.0) << ")"
                      << " | c[0]: (" << sv[0].real() << ", " << sv[0].imag() << ")"
                      << std::endl;

            EXPECT_NEAR(1.0, norm, 1e-3);
        }
    }
}

TEST_F(MetalBackendTest, CpuBackendNoiseChannel1Q_Comparison) {
    auto cpu_backend = std::make_unique<CpuBackend>(3);
    const int num_iterations = 10000;
    std::cout << "[CPU COMPARISON] Starting 1Q Noise Channel Test on CpuBackend (" << num_iterations << " iterations)..." << std::endl;

    auto channel_depol = makeDepolarizingChannel1Q(0.01);
    auto channel_amp = makeAmplitudeDampingChannel(0.01);
    auto channel_phase = makePhaseDampingChannel(0.01);

    for (int i = 0; i < num_iterations; ++i) {
        cpu_backend->applyHadamard(i % 3);
        if (i % 3 == 0) {
            cpu_backend->applyNoiseChannel1Q(channel_depol, 0);
        } else if (i % 3 == 1) {
            cpu_backend->applyNoiseChannel1Q(channel_amp, 1);
        } else {
            cpu_backend->applyNoiseChannel1Q(channel_phase, 2);
        }

        if (i % 2500 == 2499 || i == num_iterations - 1) {
            auto sv = cpu_backend->getStateVector();
            double norm_sq = 0.0;
            for (const auto& amp : sv) norm_sq += std::norm(amp);
            double norm = std::sqrt(norm_sq);

            std::cout << "[CPU NUMERICS NOISE1Q] Iteration " << (i + 1)
                      << " | State Vector Norm: " << norm 
                      << " (diff from 1.0: " << std::abs(norm - 1.0) << ")"
                      << " | c[0]: (" << sv[0].real() << ", " << sv[0].imag() << ")"
                      << std::endl;

            EXPECT_NEAR(1.0, norm, 1e-5);
        }
    }
}

TEST_F(MetalBackendTest, NoiseChannel2Q_Stress) {
    if (!backend) return;
    const int num_iterations = 10000;
    std::cout << "[STRESS TEST] Starting 2Q Noise Channel Stress Test (" << num_iterations << " iterations)..." << std::endl;

    auto channel_depol2q = makeDepolarizingChannel2Q(0.01);

    for (int i = 0; i < num_iterations; ++i) {
        backend->applyNoiseChannel2Q(channel_depol2q, 0, 1);

        if (i % 2500 == 2499 || i == num_iterations - 1) {
            auto sv = backend->getStateVector();
            double norm_sq = 0.0;
            for (const auto& amp : sv) norm_sq += std::norm(amp);
            double norm = std::sqrt(norm_sq);

            std::cout << "[STRESS NUMERICS NOISE2Q] Iteration " << (i + 1)
                      << " | State Vector Norm: " << norm 
                      << " | c[0]: (" << sv[0].real() << ", " << sv[0].imag() << ")"
                      << std::endl;

            EXPECT_TRUE(std::isfinite(norm));
            EXPECT_GT(norm, 0.0);
        }
    }
}

TEST_F(MetalBackendTest, ConcurrentStress) {
    const int num_threads = 4;
    const int iterations_per_thread = 2500; // 4 * 2500 = 10,000 total operations across threads
    std::cout << "[STRESS TEST] Starting Concurrent Multi-threaded Stress Test (" 
              << num_threads << " threads x " << iterations_per_thread << " ops = 10,000 total ops)..." << std::endl;

    std::vector<std::thread> threads;
    std::vector<double> final_norms(num_threads, 0.0);
    std::vector<bool> thread_success(num_threads, false);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, iterations_per_thread, &final_norms, &thread_success]() {
            try {
                auto thread_backend = std::make_unique<MetalBackend>(3);
                auto ch1q = makeDepolarizingChannel1Q(0.005);
                auto ch2q = makeDepolarizingChannel2Q(0.005);

                for (int i = 0; i < iterations_per_thread; ++i) {
                    double theta = (i * 0.001);
                    double c = std::cos(theta);
                    double s = std::sin(theta);
                    std::vector<Complex> m1 = {
                        Complex(c, 0.0), Complex(-s, 0.0),
                        Complex(s, 0.0), Complex(c, 0.0)
                    };
                    thread_backend->applyDenseUnitary({0}, m1);
                    thread_backend->applyNoiseChannel1Q(ch1q, 0);
                    thread_backend->applyNoiseChannel2Q(ch2q, 0, 1);
                }

                auto sv = thread_backend->getStateVector();
                double norm_sq = 0.0;
                for (const auto& amp : sv) norm_sq += std::norm(amp);
                final_norms[t] = std::sqrt(norm_sq);
                thread_success[t] = true;
            } catch (const std::exception& e) {
                std::cerr << "[CONCURRENT STRESS ERROR] Thread " << t << " exception: " << e.what() << std::endl;
                thread_success[t] = false;
            }
        });
    }

    for (int t = 0; t < num_threads; ++t) {
        threads[t].join();
        EXPECT_TRUE(thread_success[t]);
        std::cout << "[CONCURRENT NUMERICS] Thread " << t 
                  << " Finished | Final State Vector Norm: " << final_norms[t] << std::endl;
        EXPECT_TRUE(std::isfinite(final_norms[t]));
        EXPECT_GT(final_norms[t], 0.0);
    }
}

TEST_F(MetalBackendTest, ThermalRelaxationAndAllChannelsAudit) {
    if (!backend) return;
    std::cout << "[CHANNEL AUDIT] Auditing all noise channel classifications..." << std::endl;

    auto depol1q = makeDepolarizingChannel1Q(0.01);
    auto depol2q = makeDepolarizingChannel2Q(0.01);
    auto amp = makeAmplitudeDampingChannel(0.01);
    auto phase = makePhaseDampingChannel(0.01);
    auto thermal = makeThermalRelaxationChannel(50.0, 30.0, 0.02);

    auto check1Q = [](const NoiseChannel1Q& ch) {
        int scaled_unitary_count = 0;
        for (const auto& op : ch.operators) {
            float d0 = std::norm(op.matrix[0]) + std::norm(op.matrix[2]);
            float d1 = std::norm(op.matrix[1]) + std::norm(op.matrix[3]);
            Complex off = op.matrix[0] * std::conj(op.matrix[1]) + op.matrix[2] * std::conj(op.matrix[3]);
            float prob = static_cast<float>(op.probability);
            bool is_su = (std::abs(d0 - prob) < 1e-4f && std::abs(d1 - prob) < 1e-4f && std::abs(off) < 1e-4f);
            if (is_su) scaled_unitary_count++;
        }
        std::cout << "  Channel '" << ch.name << "' (total ops: " << ch.operators.size() 
                  << ") -> Scaled Unitary ops: " << scaled_unitary_count 
                  << ", Non-Unitary ops: " << (ch.operators.size() - scaled_unitary_count) << std::endl;
        return scaled_unitary_count;
    };

    std::cout << "[1Q Depolarizing]";
    int depol_su = check1Q(depol1q);
    EXPECT_EQ(depol_su, 4); // All 4 Pauli operators are scaled unitary

    std::cout << "[Amplitude Damping]";
    int amp_su = check1Q(amp);
    EXPECT_EQ(amp_su, 0); // Both K0 and K1 are non-unitary

    std::cout << "[Phase Damping]";
    int phase_su = check1Q(phase);
    EXPECT_EQ(phase_su, 0); // Both K0 and K1 are non-unitary

    std::cout << "[Thermal Relaxation (T1=50us, T2=30us)]";
    int thermal_su = check1Q(thermal);
    EXPECT_LE(thermal_su, 1); // Non-zero operators (K0K0, K0K1, K1K0) are non-unitary; K1K1 is all-zeros (0 prob)

    // Thermal relaxation 10,000-iteration stress run
    const int num_iterations = 10000;
    std::cout << "[STRESS TEST] Thermal Relaxation " << num_iterations << " iterations on MetalBackend..." << std::endl;
    for (int i = 0; i < num_iterations; ++i) {
        backend->applyNoiseChannel1Q(thermal, 0);

        if (i % 2500 == 2499 || i == num_iterations - 1) {
            auto sv = backend->getStateVector();
            double norm_sq = 0.0;
            for (const auto& amp_val : sv) norm_sq += std::norm(amp_val);
            double norm = std::sqrt(norm_sq);
            std::cout << "[STRESS NUMERICS THERMAL] Iteration " << (i + 1)
                      << " | State Vector Norm: " << norm 
                      << " (diff from 1.0: " << std::abs(norm - 1.0) << ")"
                      << " | c[0]: (" << sv[0].real() << ", " << sv[0].imag() << ")"
                      << std::endl;
            EXPECT_NEAR(1.0, norm, 1e-3);
        }
    }
}

#else
// Dummy test for non-Apple platforms to avoid warning of empty test file
TEST(MetalBackendTest, NotSupported) {
    SUCCEED();
}
#endif

