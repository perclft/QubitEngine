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

public:
  // Calculates gradients using the Adjoint Method (Reversible Computing)
  // Time: O(M * P * L) - M terms, P passes? No.
  // Standard Adjoint: 1 Forward + 1 Backward pass per Operator in H.
  // Complexity: O(M * L) where L is depth. Constant w.r.t parameters!
  // BUT: We handle H as sum of Paulis. So we run Adjoint for each Pauli term.
  // Total Complexity: O(M * L). Far better than O(P * L) if P >> M.
  // Space: O(StateVector) -- only 2 copies needed (psi and lambda).
  static std::vector<double> calculateGradientsAdjoint(
      int num_qubits, const std::vector<double> &current_params,
      AnsatzFunction applyAnsatz, const std::vector<PauliTerm> &hamiltonian) {

    // 1. Record the Circuit (Trace)
    QuantumRegister trace_reg(num_qubits);
    trace_reg.enableRecording(true);
    applyAnsatz(current_params, trace_reg);
    const auto &tape = trace_reg.getTape();

    // Identify parameterized gates
    // We map parameter index 'i' to gate index 'k' in the tape.
    // Simplifying Assumption: params are applied in order RY/RZ/RX etc.
    // We need to know WHICH gate corresponds to WHICH param index.
    // For this MVP, we assume the ansatz applies one parameterized gate for
    // each param in order.
    std::vector<size_t> param_to_gate_idx;
    int p_idx = 0;
    for (size_t k = 0; k < tape.size(); ++k) {
      if (!tape[k].params.empty()) {
        param_to_gate_idx.push_back(k);
        p_idx++;
      }
    }

    if (p_idx != (int)current_params.size()) {
      // Mismatch between params and circuit application
      // Fallback or Error? Warning for now.
      std::cerr << "Warning: Adjoint params mismatch. Circuit uses " << p_idx
                << ", provided " << current_params.size() << std::endl;
    }

    std::vector<double> total_gradients(current_params.size(), 0.0);

    // 2. Iterate over each Hamiltonian Term (Linearity of Expectation)
    for (const auto &term : hamiltonian) {
      if (std::abs(term.coefficient) < 1e-9)
        continue;

      // A. Forward Pass
      QuantumRegister psi(num_qubits);
      // Replay tape forward to get |psi_final>
      for (const auto &gate : tape) {
        psi.applyRegisteredGate(gate);
      }

      // B. Initialize Adjoint State |lambda> = H_term |psi>
      // H_term is a Pauli string. Apply it to a COPY of psi.
      QuantumRegister lambda = psi; // Copy state
      // Apply Pauli ops corresponding to string
      // e.g. "XZ" -> Apply X on 0, Z on 1.
      for (size_t q = 0; q < (size_t)num_qubits && q < term.pauli_string.size();
           ++q) {
        char op = term.pauli_string[q];
        if (op == 'X')
          lambda.applyX(q);
        else if (op == 'Y')
          lambda.applyY(q);
        else if (op == 'Z')
          lambda.applyZ(q);
      }
      // Multiply by coefficient later? Or scale lambda now?
      // Gradient is 2 * Re(<lambda | dU | psi>).
      // Let's keep coefficient separate.

      // C. Backward Pass (Adjoint Calculation)
      // Iterate gates in REVERSE
      int current_param_idx = (int)param_to_gate_idx.size() - 1;

      for (int k = (int)tape.size() - 1; k >= 0; --k) {
        const auto &gate = tape[k];

        // 1. Undo Gate U on |psi> -> |psi_{k-1}>
        psi.applyRegisteredGateInverse(gate);

        // 2. Check if this gate is parameterized (and which param)
        if (current_param_idx >= 0 &&
            (size_t)k == param_to_gate_idx[current_param_idx]) {

          psi.applyRegisteredGate(gate);

          Complex overlap = 0.0;
          // Use fully qualified enum
          if (gate.type == QuantumRegister::RecordedGate::RY) {
            // ...

            auto sv_lambda = lambda.getStateVector();
            psi.applyY(gate.qubits[0]);
            auto sv_p = psi.getStateVector();

            for (size_t z = 0; z < sv_p.size(); ++z) {
              overlap += std::conj(sv_lambda[z]) * sv_p[z];
            }
            psi.applyY(gate.qubits[0]);
          } else if (gate.type == QuantumRegister::RecordedGate::RZ) {
            // P = Z.
            psi.applyZ(gate.qubits[0]);
            auto sv_p = psi.getStateVector();
            auto sv_lambda = lambda.getStateVector();
            for (size_t z = 0; z < sv_p.size(); ++z) {
              overlap += std::conj(sv_lambda[z]) * sv_p[z];
            }
            psi.applyZ(gate.qubits[0]);
          }

          Complex i_val(0, 1);
          Complex deriv = overlap * (-0.5 * i_val);

          double contrib = 2.0 * deriv.real() * term.coefficient;
          total_gradients[current_param_idx] += contrib;

          psi.applyRegisteredGateInverse(gate);
          current_param_idx--;
        }

        // 3. Undo Gate U on |lambda> (Backpropagate Adjoint)
        lambda.applyRegisteredGateInverse(gate);
      }
    }

    return total_gradients;
  }
};
