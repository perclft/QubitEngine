// Unit Tests for CircuitOptimizer — Gate Cancellation & Tape Optimization
// Migrated from legacy src/CircuitOptimizerTest.cpp (assert-based) to GTest
#include "../src/CircuitOptimizer.hpp"
#include "../src/Exceptions.hpp"
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

// ===== Clifford Transpiler Tests =====

TEST(CircuitOptimizerTest, TranspilerStrictValidation_Throws) {
  std::vector<Gate> tape = {
      makeGate(Gate::H, {0}),
      makeGate(Gate::RZ, {0}, {0.4}) // Non-Clifford
  };
  EXPECT_THROW(CircuitOptimizer::transpileToClifford(tape, false), NonCliffordGateException);
}

TEST(CircuitOptimizerTest, TranspilerStrictValidation_NoThrow_CliffordOnly) {
  std::vector<Gate> tape = {
      makeGate(Gate::H, {0}),
      makeGate(Gate::CNOT, {0, 1}),
      makeGate(Gate::PHASE_S, {1})
  };
  EXPECT_NO_THROW(CircuitOptimizer::transpileToClifford(tape, false));
  EXPECT_EQ(tape.size(), 3);
}

TEST(CircuitOptimizerTest, TranspilerApproximation_SnappingRotations) {
  // RZ(0.1) -> 0 -> deleted
  // RZ(1.4) -> pi/2 -> PHASE_S
  // RZ(3.0) -> pi -> Z
  // RZ(4.5) -> 3*pi/2 = -pi/2 -> Z + PHASE_S
  std::vector<Gate> tape = {
      makeGate(Gate::RZ, {0}, {0.1}),
      makeGate(Gate::RZ, {0}, {1.4}),
      makeGate(Gate::RZ, {0}, {3.0}),
      makeGate(Gate::RZ, {0}, {4.5}),
  };
  CircuitOptimizer::transpileToClifford(tape, true, false);

  // Exclude deleted RZ(0.1)
  // RZ(1.4) -> PHASE_S
  // RZ(3.0) -> Z
  // RZ(4.5) -> Z, PHASE_S
  ASSERT_EQ(tape.size(), 4);
  EXPECT_EQ(tape[0].type, Gate::PHASE_S);
  EXPECT_EQ(tape[1].type, Gate::Z);
  EXPECT_EQ(tape[2].type, Gate::Z);
  EXPECT_EQ(tape[3].type, Gate::PHASE_S);
}

TEST(CircuitOptimizerTest, TranspilerApproximation_StochasticHalfway) {
  // RZ(pi/4) is exactly halfway (0.5 steps) between 0 and pi/2.
  // In round-half-to-even mode, it rounds down to 0. 
  // RZ(3*pi/4) is exactly halfway (1.5 steps) between pi/2 and pi.
  // In round-half-to-even mode, it rounds up to 2 (Z).
  std::vector<Gate> tape_det = { makeGate(Gate::RZ, {0}, {3.141592653589793 * 3.0 / 4.0}) };
  CircuitOptimizer::transpileToClifford(tape_det, true, false);
  ASSERT_EQ(tape_det.size(), 1);
  EXPECT_EQ(tape_det[0].type, Gate::Z);

  // In stochastic mode, over 100 runs, it should result in roughly 50% PHASE_S and 50% empty.
  int phase_s_count = 0;
  for (int i = 0; i < 100; ++i) {
    std::vector<Gate> tape_stoch = { makeGate(Gate::RZ, {0}, {3.141592653589793 / 4.0}) };
    CircuitOptimizer::transpileToClifford(tape_stoch, true, true);
    if (!tape_stoch.empty()) {
      EXPECT_EQ(tape_stoch.size(), 1);
      EXPECT_EQ(tape_stoch[0].type, Gate::PHASE_S);
      phase_s_count++;
    }
  }
  EXPECT_GE(phase_s_count, 20);
  EXPECT_LE(phase_s_count, 80);
}

TEST(CircuitOptimizerTest, TranspilerApproximation_Toffoli) {
  std::vector<Gate> tape = { makeGate(Gate::TOFFOLI, {0, 1, 2}) };
  CircuitOptimizer::transpileToClifford(tape, true, false);
  
  // Toffoli decomposes to 15 gates. Some of these are T/T† (which snap to S/I).
  // Let's verify that the output contains only Clifford gates.
  EXPECT_FALSE(tape.empty());
  for (const auto& g : tape) {
    EXPECT_TRUE(g.type == Gate::H || g.type == Gate::CNOT || g.type == Gate::PHASE_S || g.type == Gate::Z);
  }
}

