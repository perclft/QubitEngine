#pragma once

#include "QuantumDifferentiator.hpp"
#include <cmath>
#include <iostream>
#include <vector>

namespace qubit_engine {
namespace optimizers {

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

  std::vector<double> minimize(AnsatzFunction ansatz,
                               const std::vector<PauliTerm> &hamiltonian,
                               int num_qubits,
                               std::vector<double> initial_params) {
    std::vector<double> params = initial_params;
    size_t num_params = params.size();

    // First moment vector m and second moment vector v
    std::vector<double> m(num_params, 0.0);
    std::vector<double> v(num_params, 0.0);

    double prev_energy = 1e9; // Large initial value

    for (int t = 1; t <= config_.max_iterations; ++t) {
      // 1. Calculate Gradients
      std::vector<double> grads = QuantumDifferentiator::calculateGradients(
          num_qubits, params, ansatz, hamiltonian);

      // 2. Adam Update Rule
      // m_t = beta1 * m_{t-1} + (1 - beta1) * g_t
      // v_t = beta2 * v_{t-1} + (1 - beta2) * g_t^2
      // m_hat = m_t / (1 - beta1^t)
      // v_hat = v_t / (1 - beta2^t)
      // theta_t = theta_{t-1} - alpha * m_hat / (sqrt(v_hat) + epsilon)

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

      // Check convergence (simple gradient norm or energy change)
      // Re-evaluating energy every step is expensive, but good for debugging /
      // tolerance check.
      // Optimally, calculateGradients returns the energy too to save a call.
      // But QuantumDifferentiator::calculateGradients does 2N evaluations. It
      // does NOT return the center energy.
      // Let's optimize: perform check every 10 steps or just rely on gradient
      // norm.
      if (max_grad < config_.tolerance) {
        std::cout << "Adam Converged at iteration " << t << std::endl;
        break;
      }

      if (t % 10 == 0) {
        std::cout << "Iteration " << t << ", Max Grad: " << max_grad
                  << std::endl;
      }
    }

    return params;
  }

private:
  Config config_;
};

} // namespace optimizers
} // namespace qubit_engine
