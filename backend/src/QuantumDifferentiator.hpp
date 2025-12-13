#pragma once

#include "MolecularHamiltonian.hpp"
#include "QuantumRegister.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

// Type alias for the ansatz function: (params, register) -> void
using AnsatzFunction =
    std::function<void(const std::vector<double> &, QuantumRegister &)>;

class QuantumDifferentiator {
public:
  // Calculates the gradient of the expectation value <H> with respect to each
  // parameter using the Parameter Shift Rule.
  static std::vector<double>
  calculateGradients(int num_qubits, const std::vector<double> &current_params,
                     AnsatzFunction applyAnsatz,
                     const std::vector<PauliTerm> &hamiltonian) {
    std::vector<double> gradients(current_params.size(), 0.0);

    // Parameter Shift Rule for gate G(theta) = exp(-i * theta/2 * P):
    // dE/dtheta = 0.5 * (E(theta + pi/2) - E(theta - pi/2))

    const double SHIFT = M_PI / 2.0;

    // Iterate over each parameter
    for (size_t i = 0; i < current_params.size(); ++i) {

      // --- Forward Shift (+pi/2) ---
      std::vector<double> params_plus = current_params;
      params_plus[i] += SHIFT;

      double energy_plus =
          evaluateEnergy(num_qubits, params_plus, applyAnsatz, hamiltonian);

      // --- Backward Shift (-pi/2) ---
      std::vector<double> params_minus = current_params;
      params_minus[i] -= SHIFT;

      double energy_minus =
          evaluateEnergy(num_qubits, params_minus, applyAnsatz, hamiltonian);

      // --- Gradient ---
      gradients[i] = 0.5 * (energy_plus - energy_minus);
    }

    return gradients;
  }

private:
  static double evaluateEnergy(int num_qubits,
                               const std::vector<double> &params,
                               AnsatzFunction applyAnsatz,
                               const std::vector<PauliTerm> &hamiltonian) {
    // 1. Initialize Register |0...0>
    QuantumRegister qreg(num_qubits);

    // 2. Apply Ansatz Circuit with specific params
    applyAnsatz(params, qreg);

    // 3. Measure Expectation Value of Hamiltonian
    double energy = 0.0;
    for (const auto &term : hamiltonian) {
      energy += term.coefficient * qreg.expectationValue(term.pauli_string);
    }

    return energy;
  }
};
