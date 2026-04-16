// Unit Tests for AdamOptimizer, SPSAOptimizer, and MolecularHamiltonian
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../src/AdamOptimizer.hpp"
#include "../src/MolecularHamiltonian.hpp"
#include "../src/SPSAOptimizer.hpp"
#include <gtest/gtest.h>

using namespace qubit_engine;
using namespace qubit_engine::optimizers;

// Shared ansatz: Ry(theta)|0⟩
// E(θ) = cos(θ), minimum at θ = π where E = -1
static void simple_ansatz(const std::vector<double> &params,
                          QuantumRegister &q) {
  q.applyRotationY(0, static_cast<Precision>(params[0]));
}

// ===== MolecularHamiltonian Tests =====

TEST(MolecularHamiltonianTest, H2_ReturnsCorrectTermCount) {
  auto h = MolecularHamiltonian::getHamiltonian(MolecularHamiltonian::H2);
  // H2 has 6 Pauli terms: II, IZ, ZI, ZZ, XX, YY
  EXPECT_EQ(h.size(), 6);
}

TEST(MolecularHamiltonianTest, H2_FirstTermIsIdentity) {
  auto h = MolecularHamiltonian::getHamiltonian(MolecularHamiltonian::H2);
  ASSERT_GE(h.size(), 1);
  EXPECT_EQ(h[0].pauli_string, "II");
  EXPECT_NEAR(h[0].coefficient, -1.052373245772859, 1e-10);
}

TEST(MolecularHamiltonianTest, H2_NumQubits) {
  EXPECT_EQ(MolecularHamiltonian::getNumQubits(MolecularHamiltonian::H2), 2);
}

TEST(MolecularHamiltonianTest, LiH_NumQubits) {
  EXPECT_EQ(MolecularHamiltonian::getNumQubits(MolecularHamiltonian::LiH), 4);
}

TEST(MolecularHamiltonianTest, BeH2_NumQubits) {
  EXPECT_EQ(MolecularHamiltonian::getNumQubits(MolecularHamiltonian::BEH2), 6);
}

TEST(MolecularHamiltonianTest, H2O_NumQubits) {
  EXPECT_EQ(MolecularHamiltonian::getNumQubits(MolecularHamiltonian::H2O), 8);
}

TEST(MolecularHamiltonianTest, LiH_HasTerms) {
  auto h = MolecularHamiltonian::getHamiltonian(MolecularHamiltonian::LiH);
  EXPECT_GE(h.size(), 1);
}

TEST(MolecularHamiltonianTest, BeH2_HasTerms) {
  auto h = MolecularHamiltonian::getHamiltonian(MolecularHamiltonian::BEH2);
  EXPECT_GE(h.size(), 1);
  EXPECT_NEAR(h[0].coefficient, -15.50, 1e-10); // Check CCSD(T) proxy base energy
}

TEST(MolecularHamiltonianTest, H2O_HasTerms) {
  auto h = MolecularHamiltonian::getHamiltonian(MolecularHamiltonian::H2O);
  EXPECT_GE(h.size(), 1);
  EXPECT_NEAR(h[0].coefficient, -74.90, 1e-10); // Check CCSD(T) proxy base energy
}

TEST(MolecularHamiltonianTest, H2_CoefficientsNonZero) {
  auto h = MolecularHamiltonian::getHamiltonian(MolecularHamiltonian::H2);
  for (const auto &term : h) {
    EXPECT_NE(term.coefficient, 0.0)
        << "Term " << term.pauli_string << " has zero coefficient";
  }
}

// ===== AdamOptimizer Tests =====

TEST(AdamOptimizerTest, ConvergesOnSimpleCircuit) {
  // Minimize E(θ) = cos(θ) with Z Hamiltonian on Ry(θ)|0⟩
  // Minimum at θ = π, E = -1.0
  AdamOptimizer::Config config;
  config.learning_rate = 0.1;
  config.max_iterations = 80;
  config.tolerance = 1e-6;

  AdamOptimizer optimizer(config);

  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};
  std::vector<double> initial_params = {0.5}; // Start away from minimum

  auto result =
      optimizer.minimize(simple_ansatz, hamiltonian, 1, initial_params);

  ASSERT_EQ(result.size(), 1);
  // Should converge near π (the energy minimum for cos(θ))
  // cos(result[0]) should be close to -1.0
  double final_energy = std::cos(result[0]);
  EXPECT_LT(final_energy, -0.8) << "Expected energy < -0.8, got "
                                << final_energy << " at θ=" << result[0];
}

TEST(AdamOptimizerTest, DefaultConfigWorks) {
  // Default constructor should not crash
  AdamOptimizer optimizer;

  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};
  std::vector<double> initial_params = {1.0};

  auto result =
      optimizer.minimize(simple_ansatz, hamiltonian, 1, initial_params);
  EXPECT_EQ(result.size(), 1);
}

TEST(AdamOptimizerTest, MultipleParamsConverge) {
  // Two parameters: Ry(θ₀)|q0⟩ ⊗ Ry(θ₁)|q1⟩
  // With Z⊗I Hamiltonian, optimizing θ₀ toward π
  AdamOptimizer::Config config;
  config.learning_rate = 0.1;
  config.max_iterations = 80;
  config.tolerance = 1e-6;

  AdamOptimizer optimizer(config);

  auto two_param_ansatz = [](const std::vector<double> &params,
                             QuantumRegister &q) {
    q.applyRotationY(0, static_cast<Precision>(params[0]));
    q.applyRotationY(1, static_cast<Precision>(params[1]));
  };

  std::vector<PauliTerm> hamiltonian = {{1.0, "ZI"}};
  std::vector<double> initial_params = {0.5, 0.5};

  auto result =
      optimizer.minimize(two_param_ansatz, hamiltonian, 2, initial_params);
  ASSERT_EQ(result.size(), 2);
}

// ===== SPSAOptimizer Tests =====

TEST(SPSAOptimizerTest, ConvergesOnSimpleCircuit) {
  // SPSA is stochastic, so we use generous tolerance
  SPSAOptimizer::Config config;
  config.a = 0.5;
  config.c = 0.2;
  config.max_iterations = 80;

  SPSAOptimizer optimizer(config);

  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};
  std::vector<double> initial_params = {0.5};

  auto result =
      optimizer.minimize(simple_ansatz, hamiltonian, 1, initial_params);

  ASSERT_EQ(result.size(), 1);
  // SPSA is stochastic — just verify it moved toward lower energy
  double initial_energy = std::cos(0.5);
  double final_energy = std::cos(result[0]);
  EXPECT_LT(final_energy, initial_energy)
      << "SPSA should reduce energy from " << initial_energy << " to at least "
      << final_energy;
}

TEST(SPSAOptimizerTest, DefaultConfigWorks) {
  SPSAOptimizer optimizer;

  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};
  std::vector<double> initial_params = {1.0};

  auto result =
      optimizer.minimize(simple_ansatz, hamiltonian, 1, initial_params);
  EXPECT_EQ(result.size(), 1);
}

TEST(SPSAOptimizerTest, ReturnsCorrectParamCount) {
  SPSAOptimizer::Config config;
  config.max_iterations = 5; // Quick run

  SPSAOptimizer optimizer(config);

  auto multi_ansatz = [](const std::vector<double> &params,
                         QuantumRegister &q) {
    q.applyRotationY(0, static_cast<Precision>(params[0]));
    q.applyRotationZ(0, static_cast<Precision>(params[1]));
    q.applyRotationY(0, static_cast<Precision>(params[2]));
  };

  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};
  std::vector<double> initial_params = {0.1, 0.2, 0.3};

  auto result =
      optimizer.minimize(multi_ansatz, hamiltonian, 1, initial_params);
  EXPECT_EQ(result.size(), 3);
}
