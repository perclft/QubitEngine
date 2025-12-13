#include "../src/MolecularHamiltonian.hpp"
#include "../src/QuantumDifferentiator.hpp"
#include "../src/QuantumRegister.hpp"
#include <complex>
#include <gtest/gtest.h>

TEST(QuantumTest, Initialization) {
  // Test that a 2-qubit register initializes to |00>
  QuantumRegister q(2);
  auto state = q.getStateVector();

  // Size should be 2^2 = 4
  EXPECT_EQ(state.size(), 4);
  // |00> (index 0) should have probability amplitude 1.0
  EXPECT_EQ(state[0], std::complex<double>(1.0, 0.0));
  EXPECT_EQ(state[1], std::complex<double>(0.0, 0.0));
}

TEST(QuantumTest, PauliXGate) {
  // Test X gate (NOT gate)
  QuantumRegister q(1); // Start |0>
  q.applyX(0);          // Apply X -> |1>

  auto state = q.getStateVector();
  EXPECT_EQ(state[0], std::complex<double>(0.0, 0.0));
  EXPECT_EQ(state[1], std::complex<double>(1.0, 0.0));
}

TEST(QuantumTest, HadamardGate) {
  // Test H gate (Superposition)
  QuantumRegister q(1); // |0>
  q.applyHadamard(0);   // |+> = (|0> + |1>) / sqrt(2)

  auto state = q.getStateVector();
  double inv_sqrt_2 = 1.0 / std::sqrt(2.0);

  EXPECT_NEAR(state[0].real(), inv_sqrt_2, 1e-9);
  EXPECT_NEAR(state[1].real(), inv_sqrt_2, 1e-9);
}

TEST(QuantumTest, BellState) {
  // Test Entanglement: H(0) -> CNOT(0, 1)
  QuantumRegister q(2); // |00>

  q.applyHadamard(0); // Superposition
  q.applyCNOT(0, 1);  // Entangle

  auto state = q.getStateVector();
  double inv_sqrt_2 = 1.0 / std::sqrt(2.0);

  // Expecting 1/sqrt(2) * (|00> + |11>)
  EXPECT_NEAR(state[0].real(), inv_sqrt_2, 1e-9); // |00>
  EXPECT_NEAR(std::abs(state[1]), 0.0, 1e-9);     // |01>
  EXPECT_NEAR(std::abs(state[2]), 0.0, 1e-9);     // |10>
  EXPECT_NEAR(state[3].real(), inv_sqrt_2, 1e-9); // |11>
}
TEST(QuantumTest, ReverseCNOT) {
  // Test Control(1) -> Target(0) (Control > Target)
  // This often breaks in stride-based implementations
  QuantumRegister q(2);

  // Set input to |10> (Qubit 1 = 1, Qubit 0 = 0)
  q.applyX(1);

  // Apply CNOT(1, 0). Since Q1 is 1, Q0 should flip to 1.
  // Result should be |11>
  q.applyCNOT(1, 0);

  auto state = q.getStateVector();

  // Index 2 is |10>, Index 3 is |11>
  EXPECT_NEAR(std::abs(state[2]), 0.0, 1e-9); // Old state empty
  EXPECT_NEAR(state[3].real(), 1.0, 1e-9);    // New state occupied
}

TEST(QuantumTest, LogicValidation) {
  // Test Self-Control Error
  QuantumRegister q(2);
  EXPECT_THROW(q.applyCNOT(0, 0), std::invalid_argument);
}

TEST(QuantumTest, GradientDescentTest) {
  // Simple rotation circuit: Ry(theta)|0>
  // Energy = <psi|Z|psi>. |psi> = cos(t/2)|0> + sin(t/2)|1>
  // <Z> = cos^2(t/2) - sin^2(t/2) = cos(t)
  // dE/dt = -sin(t)

  // Set theta = pi/2. E = 0. Grad = -1.

  int num_qubits = 1;
  std::vector<double> params = {M_PI / 2.0};
  // PauliTerm struct is defined in MolecularHamiltonian.hpp
  // But MolecularHamiltonian.hpp is included tests/QuantumTests.cpp.
  // Wait, PauliTerm is likely in MolecularHamiltonian.hpp

  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  AnsatzFunction ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, p[0]);
  };

  auto grads = QuantumDifferentiator::calculateGradients(num_qubits, params,
                                                         ansatz, hamiltonian);

  EXPECT_EQ(grads.size(), 1);
  EXPECT_NEAR(grads[0], -1.0, 1e-6);
}
