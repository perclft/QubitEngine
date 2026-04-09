#pragma once

#include "MolecularHamiltonian.hpp"
#include "QuantumRegister.hpp"
#include "Types.hpp"
#define _USE_MATH_DEFINES
#include <cmath>
#include <functional>
#include <vector>

// Redundant M_PI definition removed; centralized in Types.hpp

// Type alias for the ansatz function: (params, register) -> void
using qubit_engine::QuantumRegister;
using AnsatzFunction =
    std::function<void(const std::vector<double> &, QuantumRegister &)>;

class QuantumDifferentiator {
public:
  // Calculates the gradient of the expectation value <H> with respect to each
  // parameter using the Parameter Shift Rule.
  static std::vector<double>
  calculateGradients(int num_qubits, const std::vector<double> &current_params,
                     AnsatzFunction applyAnsatz,
                     const std::vector<PauliTerm> &hamiltonian);

  // Template Ansatz Function
  template <typename RegisterType>
  using AnsatzFunc =
      std::function<void(const std::vector<double> &, RegisterType &)>;

  // ========================================================================
  // Adjoint Differentiation Method (template — must remain in header)
  // ========================================================================
  /// @brief Calculates analytical gradients using the Adjoint Differentiation method.
  /// @param num_qubits The number of qubits in the circuit.
  /// @param current_params The current variational parameters.
  /// @param applyAnsatz The callback function to apply the parameterized ansatz.
  /// @param hamiltonian The observable Hamiltonian as a list of Pauli terms.
  /// @return A vector containing the gradients with respect to each parameter.
  template <typename RegisterType = QuantumRegister>
  static std::vector<double>
  calculateGradientsAdjoint(int num_qubits,
                            const std::vector<double> &current_params,
                            AnsatzFunc<QuantumRegister> applyAnsatz,
                            const std::vector<PauliTerm> &hamiltonian) {

    using Complex = qubit_engine::Complex;
    using P = qubit_engine::Precision;

    size_t num_params = current_params.size();
    std::vector<double> gradients(num_params, 0.0);

    if (hamiltonian.empty() || num_params == 0) {
      return gradients;
    }

    // --- Step 1: Record the circuit tape ---
    QuantumRegister tape_reg(num_qubits, true);
    tape_reg.enableRecording(true);
    applyAnsatz(current_params, tape_reg);
    tape_reg.enableRecording(false);

    const auto &tape = tape_reg.getTape();

    // Build map: tape index -> parameter index
    std::vector<int> tape_param_index(tape.size(), -1);
    int param_counter = 0;
    for (size_t i = 0; i < tape.size(); ++i) {
      auto t = tape[i].type;
      if (t == QuantumRegister::RecordedGate::RX ||
          t == QuantumRegister::RecordedGate::RY ||
          t == QuantumRegister::RecordedGate::RZ) {
        if (param_counter < static_cast<int>(num_params)) {
          tape_param_index[i] = param_counter++;
        }
      }
    }

    // --- Step 2: Forward pass to get |ψ⟩ ---
    QuantumRegister psi_reg(num_qubits, true);
    applyAnsatz(current_params, psi_reg);
    auto psi_state = psi_reg.getStateVector();

    size_t dim = psi_state.size();

    // --- Step 3: Compute |λ⟩ = H|ψ⟩ ---
    std::vector<Complex> lambda_state(dim, Complex(0, 0));

    for (const auto &term : hamiltonian) {
      std::vector<Complex> pauli_psi(dim, Complex(0, 0));
      for (size_t i = 0; i < dim; ++i) {
        size_t j = i;
        Complex coeff(1, 0);

        for (size_t q = 0; q < static_cast<size_t>(num_qubits) &&
                           q < term.pauli_string.size();
             ++q) {
          char op = term.pauli_string[q];
          if (op == 'I')
            continue;

          bool bit_set = (i >> q) & 1;
          if (op == 'X') {
            j ^= (1ULL << q);
          } else if (op == 'Y') {
            j ^= (1ULL << q);
            coeff *= (bit_set ? Complex(0, -1) : Complex(0, 1));
          } else if (op == 'Z') {
            if (bit_set)
              coeff *= Complex(-1, 0);
          }
        }

        if (j < dim) {
          pauli_psi[j] += coeff * psi_state[i];
        }
      }

      // Accumulate: |λ⟩ += coeff * P|ψ⟩
      P tc = static_cast<P>(term.coefficient);
      for (size_t i = 0; i < dim; ++i) {
        lambda_state[i] += tc * pauli_psi[i];
      }
    }

    // --- Step 4: Backward pass ---
    for (int i = static_cast<int>(tape.size()) - 1; i >= 0; --i) {
      const auto &gate = tape[i];

      // a. Un-apply gate from |ψ⟩
      applyGateInverseToState(psi_state, gate, num_qubits);

      // b. If parameterized, compute gradient contribution
      if (tape_param_index[i] >= 0) {
        int pidx = tape_param_index[i];

        // Compute dU/dθ |ψ⟩
        std::vector<Complex> dpsi(dim, Complex(0, 0));
        applyGateDerivativeToState(dpsi, psi_state, gate, num_qubits);

        // grad[pidx] += 2 * Re(⟨λ|dU/dθ|ψ⟩)
        Complex inner(0, 0);
        for (size_t k = 0; k < dim; ++k) {
          inner += std::conj(lambda_state[k]) * dpsi[k];
        }
        gradients[pidx] += 2.0 * static_cast<double>(inner.real());
      }

      // c. Un-apply gate from |λ⟩
      applyGateInverseToState(lambda_state, gate, num_qubits);
    }

    return gradients;
  }

  // GPU Adjoint Differentiation implementation
  static std::vector<double> calculateGradientsAdjointGPU(
      int num_qubits,
      const std::vector<double> &current_params,
      AnsatzFunc<QuantumRegister> applyAnsatz,
      const std::vector<PauliTerm> &hamiltonian);

private:
  using Complex = qubit_engine::Complex;
  using P = qubit_engine::Precision;

  static double evaluateEnergy(int num_qubits,
                               const std::vector<double> &params,
                               AnsatzFunction applyAnsatz,
                               const std::vector<PauliTerm> &hamiltonian);

  // Apply U_gate† (inverse) to a raw state vector
  static void applyGateInverseToState(std::vector<Complex> &state,
                                      const QuantumRegister::RecordedGate &gate,
                                      int num_qubits);

  // Apply dU/dθ (gate derivative) to |ψ⟩, accumulating into |out⟩
  static void applyGateDerivativeToState(
      std::vector<Complex> &out, const std::vector<Complex> &psi,
      const QuantumRegister::RecordedGate &gate, int num_qubits);
};
