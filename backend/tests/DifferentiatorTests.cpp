// Unit Tests for QuantumDifferentiator — Parameter Shift Rule Gradients
#include "../src/MolecularHamiltonian.hpp"
#include "../src/QuantumDifferentiator.hpp"
#include "../src/QuantumRegister.hpp"
#include "Types.hpp"
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace qubit_engine;

// ===== Gradient Tests =====

TEST(DifferentiatorTest, Gradient_ZeroAngle) {
  // Ansatz: Ry(θ)|0>, Hamiltonian: Z
  // E(θ) = cos(θ), so dE/dθ = -sin(θ)
  // At θ=0, gradient should be ≈ 0
  int num_qubits = 1;
  std::vector<double> params = {0.0};

  auto ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, p[0]);
  };

  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  auto gradients = QuantumDifferentiator::calculateGradients(
      num_qubits, params, ansatz, hamiltonian);

  ASSERT_EQ(gradients.size(), 1);
  // dE/dθ at θ=0 is -sin(0) = 0
  EXPECT_NEAR(gradients[0], 0.0, 1e-4);
}

TEST(DifferentiatorTest, Gradient_AtPiOver2) {
  // E(θ) = cos(θ), so dE/dθ = -sin(θ)
  // At θ=π/2, gradient should be ≈ -1.0
  int num_qubits = 1;
  std::vector<double> params = {M_PI / 2.0};

  auto ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, p[0]);
  };

  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  auto gradients = QuantumDifferentiator::calculateGradients(
      num_qubits, params, ansatz, hamiltonian);

  ASSERT_EQ(gradients.size(), 1);
  // dE/dθ at θ=π/2 is -sin(π/2) = -1.0
  EXPECT_NEAR(gradients[0], -1.0, 0.15);
}

TEST(DifferentiatorTest, Gradient_AtPi) {
  // E(θ) = cos(θ), so dE/dθ = -sin(θ)
  // At θ=π, gradient should be ≈ 0
  int num_qubits = 1;
  std::vector<double> params = {M_PI};

  auto ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, p[0]);
  };

  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  auto gradients = QuantumDifferentiator::calculateGradients(
      num_qubits, params, ansatz, hamiltonian);

  ASSERT_EQ(gradients.size(), 1);
  EXPECT_NEAR(gradients[0], 0.0, 0.15);
}

TEST(DifferentiatorTest, Gradient_MultipleParams) {
  // Two-parameter ansatz: Ry(θ₀)|q0> ⊗ Ry(θ₁)|q1>
  // With Z⊗I Hamiltonian, only θ₀ affects the energy
  int num_qubits = 2;
  std::vector<double> params = {M_PI / 2.0, 0.0};

  auto ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, p[0]);
    q.applyRotationY(1, p[1]);
  };

  // Only measures qubit 0
  std::vector<PauliTerm> hamiltonian = {{1.0, "ZI"}};

  auto gradients = QuantumDifferentiator::calculateGradients(
      num_qubits, params, ansatz, hamiltonian);

  ASSERT_EQ(gradients.size(), 2);
  // Gradient of param 0 should be non-zero (~-1.0)
  EXPECT_NEAR(std::abs(gradients[0]), 1.0, 0.2);
  // Gradient of param 1 should be ~0 (doesn't affect Z⊗I)
  EXPECT_NEAR(gradients[1], 0.0, 0.15);
}

// ===== Edge Cases =====

TEST(DifferentiatorTest, EmptyHamiltonian) {
  int num_qubits = 1;
  std::vector<double> params = {M_PI / 4.0};

  auto ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, p[0]);
  };

  std::vector<PauliTerm> hamiltonian = {}; // Empty

  auto gradients = QuantumDifferentiator::calculateGradients(
      num_qubits, params, ansatz, hamiltonian);

  ASSERT_EQ(gradients.size(), 1);
  EXPECT_NEAR(gradients[0], 0.0, 1e-10);
}

TEST(DifferentiatorTest, GradientSize_MatchesParams) {
  int num_qubits = 1;
  std::vector<double> params = {0.1, 0.2, 0.3};

  auto ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, p[0]);
    q.applyRotationZ(0, p[1]);
    q.applyRotationY(0, p[2]);
  };

  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  auto gradients = QuantumDifferentiator::calculateGradients(
      num_qubits, params, ansatz, hamiltonian);

  // Should have one gradient per parameter
  ASSERT_EQ(gradients.size(), 3);
}
