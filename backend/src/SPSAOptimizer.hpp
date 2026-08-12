#pragma once

#include "QuantumDifferentiator.hpp"
#include <cmath>
#include <spdlog/spdlog.h>
#include <random>
#include <vector>
#include "qubit_engine_export.h"

namespace qubit_engine {
namespace optimizers {

class SPSAOptimizer {
public:
  struct Config {
    double a = 0.628;     // Step size parameter
    double c = 0.1;       // Perturbation size parameter
    double A = 0.0;       // Stability constant (often 10% of max_iter)
    double alpha = 0.602; // Decay rate for a
    double gamma = 0.101; // Decay rate for c
    int max_iterations = 100;
    uint32_t seed = 0;    // 0 = random_device hardware seed, >0 = deterministic seed
  };

  explicit SPSAOptimizer(Config config) : config_(config) {}
  SPSAOptimizer() : config_() {}

  OptimizationResult
  minimize(std::function<void(const std::vector<double> &, QuantumRegister &)>
               applyAnsatz,
           const std::vector<PauliTerm> &hamiltonian, int num_qubits,
           std::vector<double> initial_params) {

    // Fix signature issue manually: AnsatzFunction is std::function<...>
    using AnsatzFunction =
        std::function<void(const std::vector<double> &, QuantumRegister &)>;
    AnsatzFunction ansatz = applyAnsatz;

    std::vector<double> params = initial_params;
    size_t num_params = params.size();

    std::mt19937 gen;
    if (config_.seed != 0) {
      gen.seed(config_.seed);
    } else {
      std::random_device rd;
      gen.seed(rd());
    }
    std::bernoulli_distribution d(0.5); // For +/- 1

    int iterations_run = 0;
    for (int k = 0; k < config_.max_iterations; ++k) {
      iterations_run = k + 1;
      // 1. Decay step sizes
      double a_k = config_.a / std::pow(k + 1 + config_.A, config_.alpha);
      double c_k = config_.c / std::pow(k + 1, config_.gamma);

      // 2. Generate Perturbation Vector Delta (Bernoulli +/- 1)
      std::vector<double> delta(num_params);
      for (size_t i = 0; i < num_params; ++i) {
        delta[i] = d(gen) ? 1.0 : -1.0;
      }

      // 3. Evaluation (+ c_k * delta)
      std::vector<double> theta_plus = params;
      for (size_t i = 0; i < num_params; ++i)
        theta_plus[i] += c_k * delta[i];

      // Helper to evaluate energy manually locally (forcing local reg)
      double y_plus =
          evaluateEnergy(num_qubits, theta_plus, ansatz, hamiltonian);

      // 4. Evaluation (- c_k * delta)
      std::vector<double> theta_minus = params;
      for (size_t i = 0; i < num_params; ++i)
        theta_minus[i] -= c_k * delta[i];

      double y_minus =
          evaluateEnergy(num_qubits, theta_minus, ansatz, hamiltonian);

      // 5. Estimate Gradient g_k
      double c_k_safe = std::max(c_k, 1e-12);
      double comm_factor = (y_plus - y_minus) / (2.0 * c_k_safe);
      std::vector<double> g_k(num_params);
      for (size_t i = 0; i < num_params; ++i) {
        g_k[i] = comm_factor * delta[i];
      }

      // 6. Update Parameters
      for (size_t i = 0; i < num_params; ++i) {
        params[i] -= a_k * g_k[i];
      }

      if (k % 10 == 0) {
        spdlog::info("SPSA Iteration {}, Energy Estimate: {}", k, (y_plus + y_minus) / 2.0);
      }
    }

    double final_energy = evaluateEnergy(num_qubits, params, ansatz, hamiltonian);
    return OptimizationResult{params, final_energy, iterations_run};
  }

private:
  Config config_;

  // Duplicated helper from QuantumDifferentiator (private there)
  // Ideally should be a public static helper in QuantumDifferentiator or
  // QuantumRegister. For now, inline it to avoid refactoring heavily.
  double evaluateEnergy(
      int num_qubits, const std::vector<double> &params,
      std::function<void(const std::vector<double> &, QuantumRegister &)>
          applyAnsatz,
      const std::vector<PauliTerm> &hamiltonian) {
    QuantumRegister qreg(num_qubits, true); // Force Local
    applyAnsatz(params, qreg);
    double energy = 0.0;
    for (const auto &term : hamiltonian) {
      energy += term.coefficient * qreg.expectationValue(term.pauli_string);
    }
    return energy;
  }
};

} // namespace optimizers
} // namespace qubit_engine
