// Unit Tests for CircuitOptimizer — Gate Cancellation & Tape Optimization
// Migrated from legacy src/CircuitOptimizerTest.cpp (assert-based) to GTest
#include "../src/CircuitOptimizer.hpp"
#include <gtest/gtest.h>

using namespace qubit_engine;
using Gate = QuantumRegister::RecordedGate;

// Helper to build a gate
static Gate makeGate(Gate::Type t, std::vector<size_t> qubits,
                     std::vector<double> params = {}) {
  return {t, qubits, params};
}

// ===== Self-Inverse Cancellation =====

TEST(CircuitOptimizerTest, CancelHH) {
  std::vector<Gate> tape = {
      makeGate(Gate::H, {0}),
      makeGate(Gate::H, {0}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_TRUE(tape.empty()) << "H-H should cancel to identity";
}

TEST(CircuitOptimizerTest, CancelXX) {
  std::vector<Gate> tape = {
      makeGate(Gate::X, {0}),
      makeGate(Gate::X, {0}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_TRUE(tape.empty()) << "X-X should cancel to identity";
}

TEST(CircuitOptimizerTest, CancelYY) {
  std::vector<Gate> tape = {
      makeGate(Gate::Y, {0}),
      makeGate(Gate::Y, {0}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_TRUE(tape.empty()) << "Y-Y should cancel to identity";
}

TEST(CircuitOptimizerTest, CancelZZ) {
  std::vector<Gate> tape = {
      makeGate(Gate::Z, {0}),
      makeGate(Gate::Z, {0}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_TRUE(tape.empty()) << "Z-Z should cancel to identity";
}

TEST(CircuitOptimizerTest, CancelCNOT) {
  std::vector<Gate> tape = {
      makeGate(Gate::CNOT, {0, 1}),
      makeGate(Gate::CNOT, {0, 1}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_TRUE(tape.empty()) << "CNOT-CNOT should cancel to identity";
}

TEST(CircuitOptimizerTest, CancelToffoli) {
  std::vector<Gate> tape = {
      makeGate(Gate::TOFFOLI, {0, 1, 2}),
      makeGate(Gate::TOFFOLI, {0, 1, 2}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_TRUE(tape.empty()) << "Toffoli-Toffoli should cancel to identity";
}

TEST(CircuitOptimizerTest, CancelSWAP) {
  std::vector<Gate> tape = {
      makeGate(Gate::SWAP, {0, 1}),
      makeGate(Gate::SWAP, {0, 1}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_TRUE(tape.empty()) << "SWAP-SWAP should cancel to identity";
}

TEST(CircuitOptimizerTest, CancelCZ) {
  std::vector<Gate> tape = {
      makeGate(Gate::CZ, {0, 1}),
      makeGate(Gate::CZ, {0, 1}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_TRUE(tape.empty()) << "CZ-CZ should cancel to identity";
}

// ===== Mixed Cancellation =====

TEST(CircuitOptimizerTest, MixedCancellation_HXXH) {
  // H - X - X - H -> H - I - H -> H - H -> I
  std::vector<Gate> tape = {
      makeGate(Gate::H, {0}),
      makeGate(Gate::X, {0}),
      makeGate(Gate::X, {0}),
      makeGate(Gate::H, {0}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_TRUE(tape.empty()) << "H-X-X-H should fully cancel";
}

// ===== Non-Cancellation =====

TEST(CircuitOptimizerTest, NoCancellation_DifferentGates) {
  std::vector<Gate> tape = {
      makeGate(Gate::H, {0}),
      makeGate(Gate::X, {0}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_EQ(tape.size(), 2) << "H-X should not cancel";
}

TEST(CircuitOptimizerTest, NoCancellation_DifferentQubits) {
  std::vector<Gate> tape = {
      makeGate(Gate::H, {0}),
      makeGate(Gate::H, {1}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_EQ(tape.size(), 2) << "H(0)-H(1) should not cancel";
}

TEST(CircuitOptimizerTest, NoCancellation_CNOT_DifferentQubits) {
  std::vector<Gate> tape = {
      makeGate(Gate::CNOT, {0, 1}),
      makeGate(Gate::CNOT, {1, 0}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_EQ(tape.size(), 2) << "CNOT(0,1)-CNOT(1,0) should not cancel";
}

// ===== Edge Cases =====

TEST(CircuitOptimizerTest, EmptyTape) {
  std::vector<Gate> tape;
  CircuitOptimizer::optimize(tape);
  EXPECT_TRUE(tape.empty());
}

TEST(CircuitOptimizerTest, SingleGate) {
  std::vector<Gate> tape = {makeGate(Gate::H, {0})};
  CircuitOptimizer::optimize(tape);
  EXPECT_EQ(tape.size(), 1);
}

TEST(CircuitOptimizerTest, ThreeGates_OnlyMiddlePairCancels) {
  // H - X - X -> only X-X cancel, H remains
  std::vector<Gate> tape = {
      makeGate(Gate::H, {0}),
      makeGate(Gate::X, {0}),
      makeGate(Gate::X, {0}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_EQ(tape.size(), 1);
  EXPECT_EQ(tape[0].type, Gate::H);
}

TEST(CircuitOptimizerTest, RotationGates_DontCancel) {
  // RY(0.5) - RY(0.5) are NOT self-inverse (RY^-1 = RY(-θ))
  std::vector<Gate> tape = {
      makeGate(Gate::RY, {0}, {0.5}),
      makeGate(Gate::RY, {0}, {0.5}),
  };
  CircuitOptimizer::optimize(tape);
  EXPECT_EQ(tape.size(), 2) << "RY-RY should not cancel (not self-inverse)";
}
