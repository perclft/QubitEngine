#include "../src/MolecularHamiltonian.hpp"
#include "../src/QuantumDifferentiator.hpp"
#include "../src/QuantumRegister.hpp"
using qubit_engine::QuantumRegister;
#include "Types.hpp"
#include "../src/Exceptions.hpp"
#include "../src/backends/CpuBackend.hpp"
#include "../src/backends/MPSBackend.hpp"
#include "../src/backends/StabilizerBackend.hpp"
#include "../src/backends/CloudBackend.hpp"
#ifdef ENABLE_CUDA
#include "../src/backends/CudaBackend.hpp"
#endif
#include <complex>
#include <gtest/gtest.h>

using namespace qubit_engine;

TEST(QuantumTest, Initialization) {
  // Test that a 2-qubit register initializes to |00>
  QuantumRegister q(2);
  auto state = q.getStateVector();

  // Size should be 2^2 = 4
  ASSERT_EQ(state.size(), 4);
  // |00> (index 0) should have probability amplitude 1.0 + 0.0i, all other states 0
  EXPECT_EQ(state[0], Complex(1.0, 0.0));
  EXPECT_EQ(state[1], Complex(0.0, 0.0));
  EXPECT_EQ(state[2], Complex(0.0, 0.0));
  EXPECT_EQ(state[3], Complex(0.0, 0.0));

  double norm_sq = 0.0;
  for (const auto& a : state) norm_sq += std::norm(a);
  EXPECT_NEAR(norm_sq, 1.0, 1e-12);
}

TEST(QuantumTest, PauliXGate) {
  // Test X gate (NOT gate)
  QuantumRegister q(1); // Start |0>
  q.applyX(0);          // Apply X -> |1>

  auto state = q.getStateVector();
  ASSERT_EQ(state.size(), 2);
  EXPECT_EQ(state[0], Complex(0.0, 0.0));
  EXPECT_EQ(state[1], Complex(1.0, 0.0));
}

TEST(QuantumTest, HadamardGate) {
  // Test H gate (Superposition)
  QuantumRegister q(1); // |0>
  q.applyHadamard(0);   // |+> = (|0> + |1>) / sqrt(2)

  auto state = q.getStateVector();
  ASSERT_EQ(state.size(), 2);
  Precision inv_sqrt_2 = 1.0 / std::sqrt(2.0);

  EXPECT_NEAR(state[0].real(), inv_sqrt_2, 1e-6);
  EXPECT_NEAR(state[0].imag(), 0.0, 1e-6);
  EXPECT_NEAR(state[1].real(), inv_sqrt_2, 1e-6);
  EXPECT_NEAR(state[1].imag(), 0.0, 1e-6);

  double norm_sq = std::norm(state[0]) + std::norm(state[1]);
  EXPECT_NEAR(norm_sq, 1.0, 1e-12);
}

TEST(QuantumTest, BellState) {
  // Test Entanglement: H(0) -> CNOT(0, 1)
  QuantumRegister q(2); // |00>

  q.applyHadamard(0); // Superposition
  q.applyCNOT(0, 1);  // Entangle

  auto state = q.getStateVector();
  ASSERT_EQ(state.size(), 4);
  Precision inv_sqrt_2 = 1.0 / std::sqrt(2.0);

  // Expecting 1/sqrt(2) * (|00> + |11>)
  EXPECT_NEAR(state[0].real(), inv_sqrt_2, 1e-6); // |00>
  EXPECT_NEAR(state[0].imag(), 0.0, 1e-6);
  EXPECT_NEAR(state[1].real(), 0.0, 1e-6);        // |01>
  EXPECT_NEAR(state[1].imag(), 0.0, 1e-6);
  EXPECT_NEAR(state[2].real(), 0.0, 1e-6);        // |10>
  EXPECT_NEAR(state[2].imag(), 0.0, 1e-6);
  EXPECT_NEAR(state[3].real(), inv_sqrt_2, 1e-6); // |11>
  EXPECT_NEAR(state[3].imag(), 0.0, 1e-6);

  double norm_sq = 0.0;
  for (const auto& a : state) norm_sq += std::norm(a);
  EXPECT_NEAR(norm_sq, 1.0, 1e-12);
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
  ASSERT_EQ(state.size(), 4);

  EXPECT_NEAR(state[0].real(), 0.0, 1e-6);
  EXPECT_NEAR(state[0].imag(), 0.0, 1e-6);
  EXPECT_NEAR(state[1].real(), 0.0, 1e-6);
  EXPECT_NEAR(state[1].imag(), 0.0, 1e-6);
  EXPECT_NEAR(state[2].real(), 0.0, 1e-6);
  EXPECT_NEAR(state[2].imag(), 0.0, 1e-6);
  EXPECT_NEAR(state[3].real(), 1.0, 1e-6);
  EXPECT_NEAR(state[3].imag(), 0.0, 1e-6);

  double norm_sq = 0.0;
  for (const auto& a : state) norm_sq += std::norm(a);
  EXPECT_NEAR(norm_sq, 1.0, 1e-12);
}

TEST(QuantumTest, LogicValidation) {
  // Test Self-Control Error
  QuantumRegister q(2);
  EXPECT_THROW(q.applyCNOT(0, 0), InvalidArgumentException);
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
    q.applyRotationY(0, static_cast<Precision>(p[0]));
  };

  auto grads = QuantumDifferentiator::calculateGradients(num_qubits, params,
                                                         ansatz, hamiltonian);

  EXPECT_EQ(grads.size(), 1);
  EXPECT_NEAR(grads[0], -1.0, 1e-5);
}

TEST(QuantumTest, AdjointGradientTest) {
  // Same circuit as GradientDescentTest: Ry(theta)|0> with theta=pi/2
  // But using Adjoint Method (O(1) Memory).

  int num_qubits = 1;
  std::vector<double> params = {M_PI / 2.0};
  std::vector<PauliTerm> hamiltonian = {{1.0, "Z"}};

  AnsatzFunction ansatz = [](const std::vector<double> &p, QuantumRegister &q) {
    q.applyRotationY(0, static_cast<Precision>(p[0]));
  };

  auto grads = QuantumDifferentiator::calculateGradientsAdjoint(
      num_qubits, params, ansatz, hamiltonian);

  EXPECT_EQ(grads.size(), 1);
  EXPECT_NEAR(grads[0], -1.0, 1e-5);
}

TEST(QuantumTest, DefaultBackendSelectionName) {
  QuantumRegister q(2);
#ifdef ENABLE_METAL
  EXPECT_TRUE(q.getBackendName() == "Metal" || q.getBackendName() == "CPU");
#elif defined(ENABLE_CUDA)
  EXPECT_TRUE(q.getBackendName() == "CUDA" || q.getBackendName() == "CPU");
#else
  EXPECT_EQ(q.getBackendName(), "CPU");
#endif
}

TEST(BackendInvariantTest, GetNumQubitsNonZeroForAllBackends) {
  size_t n = 4;
  std::vector<std::pair<std::string, std::unique_ptr<IQuantumBackend>>> backends;
  backends.emplace_back("CPU", std::make_unique<CpuBackend>(n));
  backends.emplace_back("MPS", std::make_unique<MPSBackend>(n));
  backends.emplace_back("Stabilizer", std::make_unique<StabilizerBackend>(n));
  backends.emplace_back("Cloud", std::make_unique<CloudBackend>(n, "http://localhost:50051"));
#ifdef ENABLE_CUDA
  try {
    backends.emplace_back("CUDA", std::make_unique<CudaBackend>(n));
  } catch (...) {}
#endif

  for (const auto &[name, backend] : backends) {
    EXPECT_EQ(backend->getNumQubits(), n)
        << "Backend " << name << " failed getNumQubits() invariant";
  }
}

TEST(BackendInvariantTest, ProbabilityDistributionSumsToOne) {
  size_t n = 3;
  std::vector<std::pair<std::string, std::unique_ptr<IQuantumBackend>>> backends;
  backends.emplace_back("CPU", std::make_unique<CpuBackend>(n));
  backends.emplace_back("MPS", std::make_unique<MPSBackend>(n));
#ifdef ENABLE_CUDA
  try {
    backends.emplace_back("CUDA", std::make_unique<CudaBackend>(n));
  } catch (...) {}
#endif

  for (auto &[name, backend] : backends) {
    backend->applyHadamard(0);
    backend->applyCNOT(0, 1);
    backend->applyRotationY(1, 0.75);
    backend->applyHadamard(2);

    auto probs = backend->getProbabilities();
    ASSERT_EQ(probs.size(), 1ULL << n);
    double sum = 0.0;
    for (double p : probs) {
      EXPECT_GE(p, -1e-12) << "Backend " << name << " produced negative probability";
      sum += p;
    }
    EXPECT_NEAR(sum, 1.0, 1e-6)
        << "Backend " << name << " distribution does not sum to 1.0";
  }
}


