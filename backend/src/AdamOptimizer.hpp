#pragma once

#include "QuantumDifferentiator.hpp"
#include <cmath>
#include <spdlog/spdlog.h>
#include <vector>
#include "qubit_engine_export.h"

namespace qubit_engine {
namespace optimizers {

struct OptimizationResult {
  std::vector<double> parameters;
  double energy = 0.0;
  int iterations = 0;
};

class AdamOptimizer {
public:
  struct Config {
    double learning_rate = 0.1;
    double beta1 = 0.9;
    double beta2 = 0.999;
    double epsilon = 1e-8;
    int max_iterations = 100;
    double tolerance = 1e-6;
  };

  explicit AdamOptimizer(Config config) : config_(config) {}
  AdamOptimizer() : config_() {}

  OptimizationResult minimize(AnsatzFunction ansatz,
                             const std::vector<PauliTerm> &hamiltonian,
                             int num_qubits,
                             std::vector<double> initial_params) {
    std::vector<double> params = initial_params;
    size_t num_params = params.size();

    // First moment vector m and second moment vector v
    std::vector<double> m(num_params, 0.0);
    std::vector<double> v(num_params, 0.0);

    auto evalEnergy = [&](const std::vector<double> &p) -> double {
      QuantumRegister qreg(num_qubits, true);
      ansatz(p, qreg);
      double energy = 0.0;
      for (const auto &term : hamiltonian) {
        energy += term.coefficient * qreg.expectationValue(term.pauli_string);
      }
      return energy;
    };

    int iterations_run = 0;
    for (int t = 1; t <= config_.max_iterations; ++t) {
      iterations_run = t;
      // 1. Calculate Gradients
      std::vector<double> grads = QuantumDifferentiator::calculateGradients(
          num_qubits, params, ansatz, hamiltonian);

      double max_grad = 0.0;

      for (size_t i = 0; i < num_params; ++i) {
        double g = grads[i];
        if (std::abs(g) > max_grad)
          max_grad = std::abs(g);

        m[i] = config_.beta1 * m[i] + (1.0 - config_.beta1) * g;
        v[i] = config_.beta2 * v[i] + (1.0 - config_.beta2) * g * g;

        double m_hat = m[i] / (1.0 - std::pow(config_.beta1, t));
        double v_hat = v[i] / (1.0 - std::pow(config_.beta2, t));

        params[i] -= config_.learning_rate * m_hat /
                     (std::sqrt(v_hat) + config_.epsilon);
      }

      if (max_grad < config_.tolerance) {
        spdlog::info("Adam Converged at iteration {}", t);
        break;
      }

      if (t % 10 == 0) {
        spdlog::info("Iteration {}, Max Grad: {}", t, max_grad);
      }
    }

    double final_energy = evalEnergy(params);
    return OptimizationResult{params, final_energy, iterations_run};
  }

private:
  Config config_;
};

} // namespace optimizers
} // namespace qubit_engine
