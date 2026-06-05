// Unit Tests for QuantumJIT — Gate Compilation, Fusion & Optimization
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "../src/QuantumJIT.hpp"
#include "../src/QuantumRegister.hpp"
#include <Eigen/Dense>
#include <array>
#include <gtest/gtest.h>

using namespace qubit_engine::jit;

// Helper to construct the 4x4 unitary matrix of a vector of CompiledGate
static Eigen::Matrix4cd get_unitary_from_compiled(const std::vector<CompiledGate> &gates) {
    Eigen::Matrix4cd U = Eigen::Matrix4cd::Zero();
    for (int k = 0; k < 4; ++k) {
        qubit_engine::QuantumRegister q(2);
        if (k & 2) q.applyX(1);
        if (k & 1) q.applyX(0);
        for (const auto &g : gates) {
            std::vector<size_t> targets;
            for (int t : g.target_qubits) targets.push_back((size_t)t);
            if (!g.fused_unitary.empty()) {
                q.applyDenseUnitary(targets, g.fused_unitary);
            } else if (g.type == CompiledGate::SINGLE_QUBIT) {
                std::vector<Complex> matrix(g.single_matrix.begin(), g.single_matrix.end());
                q.applyDenseUnitary(targets, matrix);
            } else if (g.type == CompiledGate::TWO_QUBIT) {
                std::vector<Complex> matrix(g.two_matrix.begin(), g.two_matrix.end());
                q.applyDenseUnitary(targets, matrix);
            }
        }
        auto state = q.getStateVector();
        for (int row = 0; row < 4; ++row) {
            U(row, k) = state[row];
        }
    }
    return U;
}

// Helper to construct the 4x4 unitary matrix of a vector of input gates
static Eigen::Matrix4cd get_unitary_from_input(const std::vector<std::pair<std::string, std::vector<int>>> &gates, const std::vector<double> &params) {
    Eigen::Matrix4cd U = Eigen::Matrix4cd::Zero();
    for (int k = 0; k < 4; ++k) {
        qubit_engine::QuantumRegister q(2);
        if (k & 2) q.applyX(1);
        if (k & 1) q.applyX(0);
        for (size_t i = 0; i < gates.size(); ++i) {
            const auto &g = gates[i];
            double p = i < params.size() ? params[i] : 0.0;
            if (g.first == "H") q.applyHadamard(g.second[0]);
            else if (g.first == "X") q.applyX(g.second[0]);
            else if (g.first == "Y") q.applyY(g.second[0]);
            else if (g.first == "Z") q.applyZ(g.second[0]);
            else if (g.first == "S") q.applyPhaseS(g.second[0]);
            else if (g.first == "T") q.applyPhaseT(g.second[0]);
            else if (g.first == "RX") q.applyRotationX(g.second[0], p);
            else if (g.first == "RY") q.applyRotationY(g.second[0], p);
            else if (g.first == "RZ") q.applyRotationZ(g.second[0], p);
            else if (g.first == "CNOT" || g.first == "CX") q.applyCNOT(g.second[0], g.second[1]);
            else if (g.first == "CZ") q.applyCZ(g.second[0], g.second[1]);
            else if (g.first == "SWAP") q.applySWAP(g.second[0], g.second[1]);
        }
        auto state = q.getStateVector();
        for (int row = 0; row < 4; ++row) {
            U(row, k) = state[row];
        }
    }
    return U;
}

// Helper to verify unitary equivalence up to a global phase
static bool are_unitaries_equivalent(const Eigen::Matrix4cd &U1, const Eigen::Matrix4cd &U2) {
    double trace_norm = std::abs((U1.adjoint() * U2).trace());
    return std::abs(trace_norm - 4.0) < 1e-8;
}

static int count_cnots(const std::vector<CompiledGate> &gates) {
    int count = 0;
    for (const auto &g : gates) {
        if (g.type == CompiledGate::TWO_QUBIT) {
            count++;
        }
    }
    return count;
}

// ===== Basic Compilation =====

TEST(JITTest, CompileSimpleCircuit_O0) {
  QuantumJIT jit(QuantumJIT::O0);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"H", {0}}, {"X", {1}}, {"CNOT", {0, 1}}};

  auto ir = jit.compile(2, gates);

  EXPECT_EQ(ir.num_qubits, 2);
  EXPECT_EQ(ir.stats.original_gates, 3);
  // At O0, no optimization should occur
  EXPECT_EQ(ir.stats.optimized_gates, 3);
  EXPECT_EQ(ir.gates.size(), 3);
}

TEST(JITTest, CompileSingleGate) {
  QuantumJIT jit(QuantumJIT::O0);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {{"H", {0}}};

  auto ir = jit.compile(1, gates);

  EXPECT_EQ(ir.num_qubits, 1);
  ASSERT_EQ(ir.gates.size(), 1);
  EXPECT_EQ(ir.gates[0].type, CompiledGate::SINGLE_QUBIT);
  ASSERT_EQ(ir.gates[0].target_qubits.size(), 1);
  EXPECT_EQ(ir.gates[0].target_qubits[0], 0);
}

TEST(JITTest, CompileWithRotationParams) {
  QuantumJIT jit(QuantumJIT::O0);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {{"RZ", {0}},
                                                                 {"RY", {1}}};
  std::vector<double> params = {M_PI / 4.0, M_PI / 2.0};

  auto ir = jit.compile(2, gates, params);

  EXPECT_EQ(ir.stats.original_gates, 2);
  EXPECT_EQ(ir.gates.size(), 2);
}

TEST(JITTest, CompileTwoQubitGate) {
  QuantumJIT jit(QuantumJIT::O0);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"CNOT", {0, 1}}};

  auto ir = jit.compile(2, gates);

  ASSERT_EQ(ir.gates.size(), 1);
  EXPECT_EQ(ir.gates[0].type, CompiledGate::TWO_QUBIT);
  ASSERT_EQ(ir.gates[0].target_qubits.size(), 2);
}

TEST(JITTest, CompileSWAPGate) {
  QuantumJIT jit(QuantumJIT::O0);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"SWAP", {0, 1}}};

  auto ir = jit.compile(2, gates);
  ASSERT_EQ(ir.gates.size(), 1);
  EXPECT_EQ(ir.gates[0].type, CompiledGate::TWO_QUBIT);
}

TEST(JITTest, CompileCZGate) {
  QuantumJIT jit(QuantumJIT::O0);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"CZ", {0, 1}}};

  auto ir = jit.compile(2, gates);
  ASSERT_EQ(ir.gates.size(), 1);
  EXPECT_EQ(ir.gates[0].type, CompiledGate::TWO_QUBIT);
}

// ===== Hadamard Matrix Correctness (via compile) =====

TEST(JITTest, HadamardMatrixValues) {
  QuantumJIT jit(QuantumJIT::O0);
  std::vector<std::pair<std::string, std::vector<int>>> gates = {{"H", {0}}};
  auto ir = jit.compile(1, gates);

  ASSERT_EQ(ir.gates.size(), 1);
  auto &m = ir.gates[0].single_matrix;

  // H = 1/sqrt(2) * [[1, 1], [1, -1]]
  double inv_sqrt2 = 1.0 / std::sqrt(2.0);
  EXPECT_NEAR(std::abs(Complex(m[0]) - Complex(inv_sqrt2, 0.0)), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(Complex(m[1]) - Complex(inv_sqrt2, 0.0)), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(Complex(m[2]) - Complex(inv_sqrt2, 0.0)), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(Complex(m[3]) + Complex(inv_sqrt2, 0.0)), 0.0, 1e-10);
}

TEST(JITTest, PauliXMatrixValues) {
  QuantumJIT jit(QuantumJIT::O0);
  std::vector<std::pair<std::string, std::vector<int>>> gates = {{"X", {0}}};
  auto ir = jit.compile(1, gates);

  ASSERT_EQ(ir.gates.size(), 1);
  auto &m = ir.gates[0].single_matrix;

  // X = [[0, 1], [1, 0]]
  EXPECT_NEAR(std::abs(Complex(m[0])), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(Complex(m[1]) - Complex(1.0, 0.0)), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(Complex(m[2]) - Complex(1.0, 0.0)), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(Complex(m[3])), 0.0, 1e-10);
}

// ===== Optimization: Adjacent Gate Cancellation =====

TEST(JITTest, CancelAdjacentGates_HH) {
  // H * H = I → should cancel
  QuantumJIT jit(QuantumJIT::O1);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {{"H", {0}},
                                                                 {"H", {0}}};

  auto ir = jit.compile(1, gates);

  EXPECT_EQ(ir.stats.original_gates, 2);
  // The two H gates cancel to identity — optimized away
  EXPECT_LT(ir.stats.optimized_gates, 2);
}

TEST(JITTest, CancelAdjacentGates_XX) {
  // X * X = I
  QuantumJIT jit(QuantumJIT::O1);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {{"X", {0}},
                                                                 {"X", {0}}};

  auto ir = jit.compile(1, gates);
  EXPECT_LT(ir.stats.optimized_gates, 2);
}

TEST(JITTest, NoCancellation_DifferentQubits) {
  // H(0) * H(1) should NOT cancel
  QuantumJIT jit(QuantumJIT::O1);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {{"H", {0}},
                                                                 {"H", {1}}};

  auto ir = jit.compile(2, gates);
  EXPECT_EQ(ir.stats.optimized_gates, 2);
}

// ===== Optimization: Gate Fusion =====

TEST(JITTest, FuseConsecutiveSingleQubitGates) {
  // H(0) * X(0) * Y(0) should fuse into single gate at O2
  QuantumJIT jit(QuantumJIT::O2);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"H", {0}}, {"X", {0}}, {"Y", {0}}};

  auto ir = jit.compile(1, gates);

  EXPECT_EQ(ir.stats.original_gates, 3);
  // 3 single-qubit gates on same qubit should fuse into fewer gates
  EXPECT_LE(ir.stats.optimized_gates, 2);
}

TEST(JITTest, NoFusion_DifferentQubits) {
  // Gates on different qubits should not fuse
  QuantumJIT jit(QuantumJIT::O2);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {{"H", {0}},
                                                                 {"X", {1}}};

  auto ir = jit.compile(2, gates);
  // Can't fuse different qubits — should stay as 2
  EXPECT_EQ(ir.stats.optimized_gates, 2);
}

// ===== Optimization Stats =====

TEST(JITTest, OptimizationStats_Populated) {
  QuantumJIT jit(QuantumJIT::O2);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"H", {0}}, {"H", {0}}, {"X", {1}}, {"CNOT", {0, 1}}};

  auto ir = jit.compile(2, gates);

  EXPECT_EQ(ir.stats.original_gates, 4);
  EXPECT_GE(ir.stats.compilation_time_ms, 0.0);
  EXPECT_GE(ir.stats.expected_speedup, 0.0);
}

TEST(JITTest, EmptyCircuit) {
  QuantumJIT jit(QuantumJIT::O2);

  std::vector<std::pair<std::string, std::vector<int>>> gates = {};

  auto ir = jit.compile(2, gates);

  EXPECT_EQ(ir.stats.original_gates, 0);
  EXPECT_EQ(ir.stats.optimized_gates, 0);
  EXPECT_EQ(ir.gates.size(), 0);
}

// ===== Optimization: O4 Adjacent Two-Qubit Gate Fusion =====

TEST(JITTest, FuseAdjacentTwoQubitGates_SameOrder_O4) {
  QuantumJIT jit(QuantumJIT::O4);
  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"CNOT", {0, 1}}, {"CZ", {0, 1}}};

  auto ir = jit.compile(2, gates);

  EXPECT_EQ(ir.stats.original_gates, 2);
  EXPECT_LE(count_cnots(ir.gates), 3);
  
  Eigen::Matrix4cd U_orig = get_unitary_from_input(gates, {});
  Eigen::Matrix4cd U_comp = get_unitary_from_compiled(ir.gates);
  std::cout << "\n--- UNITARY DEBUG SameOrder_O4 ---\n";
  std::cout << "U_orig:\n" << U_orig << "\n";
  std::cout << "U_comp:\n" << U_comp << "\n";
  std::cout << "U_orig * U_comp^\\dagger:\n" << (U_orig * U_comp.adjoint()) << "\n";
  EXPECT_TRUE(are_unitaries_equivalent(U_orig, U_comp)) << "Compiled unitary does not match original for SameOrder_O4";
}

TEST(JITTest, FuseAdjacentTwoQubitGates_SwappedOrder_O4) {
  QuantumJIT jit(QuantumJIT::O4);
  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"CNOT", {0, 1}}, {"CZ", {1, 0}}};

  auto ir = jit.compile(2, gates);

  EXPECT_EQ(ir.stats.original_gates, 2);
  EXPECT_LE(count_cnots(ir.gates), 3);

  Eigen::Matrix4cd U_orig = get_unitary_from_input(gates, {});
  Eigen::Matrix4cd U_comp = get_unitary_from_compiled(ir.gates);
  EXPECT_TRUE(are_unitaries_equivalent(U_orig, U_comp)) << "Compiled unitary does not match original for SwappedOrder_O4";
}

TEST(JITTest, FuseAdjacentTwoQubitGates_Commute_O4) {
  QuantumJIT jit(QuantumJIT::O4);
  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"CNOT", {0, 1}}, {"X", {2}}, {"CZ", {0, 1}}};

  auto ir = jit.compile(3, gates);

  EXPECT_EQ(ir.stats.original_gates, 3);
  
  int cnots = count_cnots(ir.gates);
  EXPECT_LE(cnots, 3);
  
  std::vector<std::pair<std::string, std::vector<int>>> sub_gates = {
      {"CNOT", {0, 1}}, {"CZ", {0, 1}}};
  auto ir_sub = jit.compile(2, sub_gates);
  Eigen::Matrix4cd U_orig = get_unitary_from_input(sub_gates, {});
  Eigen::Matrix4cd U_comp = get_unitary_from_compiled(ir_sub.gates);
  EXPECT_TRUE(are_unitaries_equivalent(U_orig, U_comp)) << "Subsystem 0,1 compiled unitary does not match original";
}

TEST(JITTest, IdentityCancellation_O4) {
  QuantumJIT jit(QuantumJIT::O4);
  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"CNOT", {0, 1}}, {"CNOT", {0, 1}}};

  auto ir = jit.compile(2, gates);

  EXPECT_EQ(ir.stats.original_gates, 2);
  // CNOT * CNOT = I, so they should be completely removed
  EXPECT_EQ(ir.stats.optimized_gates, 0);
  EXPECT_EQ(ir.gates.size(), 0);
}

#ifdef ENABLE_CUDA
#include "../src/backends/CudaBackend.hpp"

TEST(JITTest, CudaApplyDenseUnitary_JIT) {
  QuantumJIT jit(QuantumJIT::O4);
  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"H", {0}},
      {"CNOT", {0, 1}},
      {"RZ", {1}},
      {"CNOT", {0, 1}}
  };
  std::vector<double> params = {0.5};

  auto ir = jit.compile(2, gates, params);
  
  bool has_fused = false;
  for (const auto& g : ir.gates) {
    if (!g.fused_unitary.empty()) {
      has_fused = true;
      break;
    }
  }
  EXPECT_TRUE(has_fused);

  qubit_engine::CudaBackend cuda_backend(2);
  
  for (const auto& g : ir.gates) {
    std::vector<size_t> targets;
    for (int t : g.target_qubits) targets.push_back(static_cast<size_t>(t));
    
    if (!g.fused_unitary.empty()) {
      EXPECT_NO_THROW(cuda_backend.applyDenseUnitary(targets, g.fused_unitary));
    } else if (g.type == CompiledGate::SINGLE_QUBIT) {
      std::vector<qubit_engine::Complex> matrix(g.single_matrix.begin(), g.single_matrix.end());
      EXPECT_NO_THROW(cuda_backend.applyDenseUnitary(targets, matrix));
    } else if (g.type == CompiledGate::TWO_QUBIT) {
      std::vector<qubit_engine::Complex> matrix(g.two_matrix.begin(), g.two_matrix.end());
      EXPECT_NO_THROW(cuda_backend.applyDenseUnitary(targets, matrix));
    }
  }

  auto state = cuda_backend.getStateVector();
  double norm = 0.0;
  for (auto c : state) {
    norm += std::norm(c);
  }
  EXPECT_NEAR(norm, 1.0, 1e-9);
}
#endif
