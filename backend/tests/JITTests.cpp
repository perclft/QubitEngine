// Unit Tests for QuantumJIT — Gate Compilation, Fusion & Optimization
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "../src/QuantumJIT.hpp"
#include <array>
#include <gtest/gtest.h>

using namespace qubit_engine::jit;

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
  EXPECT_NEAR(std::abs(m[0] - inv_sqrt2), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(m[1] - inv_sqrt2), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(m[2] - inv_sqrt2), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(m[3] + inv_sqrt2), 0.0, 1e-10);
}

TEST(JITTest, PauliXMatrixValues) {
  QuantumJIT jit(QuantumJIT::O0);
  std::vector<std::pair<std::string, std::vector<int>>> gates = {{"X", {0}}};
  auto ir = jit.compile(1, gates);

  ASSERT_EQ(ir.gates.size(), 1);
  auto &m = ir.gates[0].single_matrix;

  // X = [[0, 1], [1, 0]]
  EXPECT_NEAR(std::abs(m[0]), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(m[1] - 1.0), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(m[2] - 1.0), 0.0, 1e-10);
  EXPECT_NEAR(std::abs(m[3]), 0.0, 1e-10);
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
