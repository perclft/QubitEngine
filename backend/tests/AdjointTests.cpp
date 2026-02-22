#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../src/MolecularHamiltonian.hpp"
#include "../src/QuantumDifferentiator.hpp"
#include "../src/QuantumRegister.hpp"
#include "../src/Types.hpp"
#include <gtest/gtest.h>
#include <vector>

using Precision = qubit_engine::Precision;

// ============================================================================
// Test Suite: Adjoint Differentiation Method
// ============================================================================

// --- Basic single-parameter tests ---

TEST(AdjointTest, SingleRY_GradientAtPiOver2) {
  // Ry(π/2)|0⟩ with H = Z
  // d⟨Z⟩/dθ at θ=π/2 should be -sin(π/2) = -1.0
  int num_qubits = 1;
  std::vector<double> params = {M_PI / 2.0};
  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  AnsatzFunction ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, static_cast<Precision>(p[0]));
  };

  auto grads = QuantumDifferentiator::calculateGradientsAdjoint(
      num_qubits, params, ansatz, hamiltonian);

  EXPECT_EQ(grads.size(), 1u);
  EXPECT_NEAR(grads[0], -1.0, 1e-5);
}

TEST(AdjointTest, SingleRY_GradientAtZero) {
  // d⟨Z⟩/dθ at θ=0 for Ry gate is -sin(0) = 0.0
  int num_qubits = 1;
  std::vector<double> params = {0.0};
  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  AnsatzFunction ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, static_cast<Precision>(p[0]));
  };

  auto grads = QuantumDifferentiator::calculateGradientsAdjoint(
      num_qubits, params, ansatz, hamiltonian);

  EXPECT_EQ(grads.size(), 1u);
  EXPECT_NEAR(grads[0], 0.0, 1e-5);
}

TEST(AdjointTest, SingleRY_GradientAtPi) {
  // d⟨Z⟩/dθ at θ=π for Ry gate is -sin(π) = 0.0
  int num_qubits = 1;
  std::vector<double> params = {M_PI};
  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  AnsatzFunction ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, static_cast<Precision>(p[0]));
  };

  auto grads = QuantumDifferentiator::calculateGradientsAdjoint(
      num_qubits, params, ansatz, hamiltonian);

  EXPECT_EQ(grads.size(), 1u);
  EXPECT_NEAR(grads[0], 0.0, 1e-5);
}

// --- Multi-parameter tests ---

TEST(AdjointTest, TwoRY_IndependentQubits) {
  // Ry(θ0) on qubit 0, Ry(θ1) on qubit 1, H = ZI + IZ
  // d⟨ZI⟩/dθ0 = -sin(θ0), d⟨ZI⟩/dθ1 = 0
  // d⟨IZ⟩/dθ0 = 0, d⟨IZ⟩/dθ1 = -sin(θ1)
  // Total: grad[0] = -sin(θ0), grad[1] = -sin(θ1)
  int num_qubits = 2;
  std::vector<double> params = {M_PI / 2.0, M_PI / 4.0};
  std::vector<PauliTerm> hamiltonian = {{1.0, "ZI"}, {1.0, "IZ"}};

  AnsatzFunction ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, static_cast<Precision>(p[0]));
    q.applyRotationY(1, static_cast<Precision>(p[1]));
  };

  auto grads = QuantumDifferentiator::calculateGradientsAdjoint(
      num_qubits, params, ansatz, hamiltonian);

  EXPECT_EQ(grads.size(), 2u);
  EXPECT_NEAR(grads[0], -std::sin(M_PI / 2.0), 1e-5);
  EXPECT_NEAR(grads[1], -std::sin(M_PI / 4.0), 1e-5);
}

// --- Mixed circuit tests (non-parameterized + parameterized gates) ---

TEST(AdjointTest, HadamardThenRY) {
  // H(0) then Ry(θ, 0) with H = Z
  // H|0⟩ = |+⟩, then Ry(θ)|+⟩
  int num_qubits = 1;
  std::vector<double> params = {M_PI / 2.0};
  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  AnsatzFunction ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyHadamard(0);
    q.applyRotationY(0, static_cast<Precision>(p[0]));
  };

  // Compare with parameter shift
  auto grads_adjoint = QuantumDifferentiator::calculateGradientsAdjoint(
      num_qubits, params, ansatz, hamiltonian);
  auto grads_psr = QuantumDifferentiator::calculateGradients(
      num_qubits, params, ansatz, hamiltonian);

  EXPECT_EQ(grads_adjoint.size(), 1u);
  EXPECT_NEAR(grads_adjoint[0], grads_psr[0], 1e-5);
}

TEST(AdjointTest, EntangledCircuit_AdjointMatchesPSR) {
  // Bell-like ansatz: H(0), CNOT(0,1), Ry(θ, 0), H = ZZ
  int num_qubits = 2;
  std::vector<double> params = {0.7};
  std::vector<PauliTerm> hamiltonian = {{1.0, "ZZ"}};

  AnsatzFunction ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyHadamard(0);
    q.applyCNOT(0, 1);
    q.applyRotationY(0, static_cast<Precision>(p[0]));
  };

  auto grads_adjoint = QuantumDifferentiator::calculateGradientsAdjoint(
      num_qubits, params, ansatz, hamiltonian);
  auto grads_psr = QuantumDifferentiator::calculateGradients(
      num_qubits, params, ansatz, hamiltonian);

  EXPECT_EQ(grads_adjoint.size(), 1u);
  EXPECT_NEAR(grads_adjoint[0], grads_psr[0], 1e-4);
}

// --- Edge cases ---

TEST(AdjointTest, EmptyHamiltonian_ReturnsZeros) {
  int num_qubits = 1;
  std::vector<double> params = {M_PI / 2.0};
  std::vector<PauliTerm> hamiltonian = {};

  AnsatzFunction ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, static_cast<Precision>(p[0]));
  };

  auto grads = QuantumDifferentiator::calculateGradientsAdjoint(
      num_qubits, params, ansatz, hamiltonian);

  EXPECT_EQ(grads.size(), 1u);
  EXPECT_NEAR(grads[0], 0.0, 1e-10);
}

TEST(AdjointTest, NoParameters_ReturnsEmpty) {
  int num_qubits = 1;
  std::vector<double> params = {};
  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  AnsatzFunction ansatz = [](const std::vector<double> &, QuantumRegister &q) {
    q.applyHadamard(0);
  };

  auto grads = QuantumDifferentiator::calculateGradientsAdjoint(
      num_qubits, params, ansatz, hamiltonian);

  EXPECT_TRUE(grads.empty());
}

// --- Cross-validation: Adjoint must match PSR for various angles ---

TEST(AdjointTest, CrossValidation_MultipleAngles) {
  int num_qubits = 1;
  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  AnsatzFunction ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, static_cast<Precision>(p[0]));
  };

  // Test at several angles
  std::vector<double> test_angles = {0.0,  0.3,        M_PI / 4.0, M_PI / 2.0,
                                     M_PI, 1.5 * M_PI, 2.0 * M_PI};

  for (double angle : test_angles) {
    std::vector<double> params = {angle};

    auto grads_adjoint = QuantumDifferentiator::calculateGradientsAdjoint(
        num_qubits, params, ansatz, hamiltonian);
    auto grads_psr = QuantumDifferentiator::calculateGradients(
        num_qubits, params, ansatz, hamiltonian);

    EXPECT_NEAR(grads_adjoint[0], grads_psr[0], 1e-4)
        << "Mismatch at angle=" << angle;
  }
}
