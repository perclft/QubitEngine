#pragma once

#include "MolecularHamiltonian.hpp"
#include "QuantumRegister.hpp"
#include "Types.hpp"
#define _USE_MATH_DEFINES
#include <cmath>
#include <functional>
#include <vector>

// Fallback for M_PI if not defined (Windows compatibility)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef MPI_ENABLED
#include <mpi.h>
#endif

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

    // --- Parallelization Strategy (MPI) ---
    int rank = 0;
    int size = 1;

#ifdef MPI_ENABLED
    int initialized;
    MPI_Initialized(&initialized);
    if (!initialized) {
      MPI_Init(NULL, NULL);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
#endif

    // Distribute parameters across ranks
    size_t num_params = current_params.size();
    size_t chunk_size = (num_params + size - 1) / size;
    size_t start_idx = rank * chunk_size;
    size_t end_idx = std::min(start_idx + chunk_size, num_params);

    // Iterate over MY subset of parameters
    for (size_t i = start_idx; i < end_idx; ++i) {

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

#ifdef MPI_ENABLED
    // Gather results (Global Sum)
    if (size > 1) {
      std::vector<double> global_gradients(num_params);
      MPI_Allreduce(gradients.data(), global_gradients.data(), num_params,
                    MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
      gradients = global_gradients;
    }
#endif

    return gradients;
  }

private:
  static double evaluateEnergy(int num_qubits,
                               const std::vector<double> &params,
                               AnsatzFunction applyAnsatz,
                               const std::vector<PauliTerm> &hamiltonian) {
    QuantumRegister qreg(num_qubits, true);
    applyAnsatz(params, qreg);
    double energy = 0.0;
    for (const auto &term : hamiltonian) {
      energy += term.coefficient * qreg.expectationValue(term.pauli_string);
    }
    return energy;
  }

public:
  // Template Ansatz Function
  template <typename RegisterType>
  using AnsatzFunc =
      std::function<void(const std::vector<double> &, RegisterType &)>;

  // ========================================================================
  // Adjoint Differentiation Method
  // ========================================================================
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

private:
  // ========================================================================
  // Gate derivative and inverse helpers for raw state vectors
  // ========================================================================

  using Complex = qubit_engine::Complex;
  using P = qubit_engine::Precision; // float

  // Apply U_gate† (inverse) to a raw state vector
  static void applyGateInverseToState(std::vector<Complex> &state,
                                      const QuantumRegister::RecordedGate &gate,
                                      int num_qubits) {
    size_t dim = state.size();

    switch (gate.type) {
    case QuantumRegister::RecordedGate::H: {
      size_t target = gate.qubits[0];
      P is2 = static_cast<P>(1.0 / std::sqrt(2.0));
      for (size_t idx = 0; idx < dim / 2; ++idx) {
        size_t i0 =
            ((idx >> target) << (target + 1)) | (idx & ((1ULL << target) - 1));
        size_t i1 = i0 | (1ULL << target);
        Complex v0 = state[i0], v1 = state[i1];
        state[i0] = is2 * (v0 + v1);
        state[i1] = is2 * (v0 - v1);
      }
      break;
    }
    case QuantumRegister::RecordedGate::X: {
      size_t target = gate.qubits[0];
      for (size_t idx = 0; idx < dim / 2; ++idx) {
        size_t i0 =
            ((idx >> target) << (target + 1)) | (idx & ((1ULL << target) - 1));
        size_t i1 = i0 | (1ULL << target);
        std::swap(state[i0], state[i1]);
      }
      break;
    }
    case QuantumRegister::RecordedGate::Y: {
      size_t target = gate.qubits[0];
      for (size_t idx = 0; idx < dim / 2; ++idx) {
        size_t i0 =
            ((idx >> target) << (target + 1)) | (idx & ((1ULL << target) - 1));
        size_t i1 = i0 | (1ULL << target);
        Complex v0 = state[i0], v1 = state[i1];
        state[i0] = Complex(0, -1) * v1;
        state[i1] = Complex(0, 1) * v0;
      }
      break;
    }
    case QuantumRegister::RecordedGate::Z: {
      size_t target = gate.qubits[0];
      for (size_t idx = 0; idx < dim / 2; ++idx) {
        size_t i0 =
            ((idx >> target) << (target + 1)) | (idx & ((1ULL << target) - 1));
        size_t i1 = i0 | (1ULL << target);
        state[i1] *= Complex(-1, 0);
      }
      break;
    }
    case QuantumRegister::RecordedGate::CNOT: {
      size_t control = gate.qubits[0], target = gate.qubits[1];
      for (size_t idx = 0; idx < dim / 2; ++idx) {
        size_t i0 =
            ((idx >> target) << (target + 1)) | (idx & ((1ULL << target) - 1));
        size_t i1 = i0 | (1ULL << target);
        if ((i0 >> control) & 1)
          std::swap(state[i0], state[i1]);
      }
      break;
    }
    case QuantumRegister::RecordedGate::SWAP: {
      size_t q1 = gate.qubits[0], q2 = gate.qubits[1];
      for (size_t idx = 0; idx < dim; ++idx) {
        int b1 = (idx >> q1) & 1, b2 = (idx >> q2) & 1;
        if (b1 == 0 && b2 == 1) {
          size_t sw = idx ^ (1ULL << q1) ^ (1ULL << q2);
          std::swap(state[idx], state[sw]);
        }
      }
      break;
    }
    case QuantumRegister::RecordedGate::CZ: {
      size_t control = gate.qubits[0], target = gate.qubits[1];
      for (size_t idx = 0; idx < dim; ++idx) {
        if (((idx >> control) & 1) && ((idx >> target) & 1))
          state[idx] *= Complex(-1, 0);
      }
      break;
    }
    case QuantumRegister::RecordedGate::RY: {
      size_t target = gate.qubits[0];
      P angle = static_cast<P>(-gate.params[0]);
      P c = std::cos(angle / P(2)), s = std::sin(angle / P(2));
      for (size_t idx = 0; idx < dim / 2; ++idx) {
        size_t i0 =
            ((idx >> target) << (target + 1)) | (idx & ((1ULL << target) - 1));
        size_t i1 = i0 | (1ULL << target);
        Complex v0 = state[i0], v1 = state[i1];
        state[i0] = c * v0 - s * v1;
        state[i1] = s * v0 + c * v1;
      }
      break;
    }
    case QuantumRegister::RecordedGate::RX: {
      size_t target = gate.qubits[0];
      P angle = static_cast<P>(-gate.params[0]);
      P c = std::cos(angle / P(2)), s = std::sin(angle / P(2));
      Complex neg_is(0, -s);
      for (size_t idx = 0; idx < dim / 2; ++idx) {
        size_t i0 =
            ((idx >> target) << (target + 1)) | (idx & ((1ULL << target) - 1));
        size_t i1 = i0 | (1ULL << target);
        Complex v0 = state[i0], v1 = state[i1];
        state[i0] = c * v0 + neg_is * v1;
        state[i1] = neg_is * v0 + c * v1;
      }
      break;
    }
    case QuantumRegister::RecordedGate::RZ: {
      size_t target = gate.qubits[0];
      P angle = static_cast<P>(-gate.params[0]);
      Complex phase0 = std::exp(Complex(0, -angle / P(2)));
      Complex phase1 = std::exp(Complex(0, angle / P(2)));
      for (size_t idx = 0; idx < dim / 2; ++idx) {
        size_t i0 =
            ((idx >> target) << (target + 1)) | (idx & ((1ULL << target) - 1));
        size_t i1 = i0 | (1ULL << target);
        state[i0] *= phase0;
        state[i1] *= phase1;
      }
      break;
    }
    default:
      break;
    }
  }

  // Apply dU/dθ (gate derivative) to |ψ⟩, accumulating into |out⟩
  static void applyGateDerivativeToState(
      std::vector<Complex> &out, const std::vector<Complex> &psi,
      const QuantumRegister::RecordedGate &gate, int num_qubits) {
    size_t dim = psi.size();

    switch (gate.type) {
    case QuantumRegister::RecordedGate::RY: {
      size_t target = gate.qubits[0];
      P angle = static_cast<P>(gate.params[0]);
      P c = std::cos(angle / P(2)), s = std::sin(angle / P(2));
      P h = P(0.5);
      for (size_t idx = 0; idx < dim / 2; ++idx) {
        size_t i0 =
            ((idx >> target) << (target + 1)) | (idx & ((1ULL << target) - 1));
        size_t i1 = i0 | (1ULL << target);
        Complex v0 = psi[i0], v1 = psi[i1];
        out[i0] += h * (-s * v0 - c * v1);
        out[i1] += h * (c * v0 - s * v1);
      }
      break;
    }
    case QuantumRegister::RecordedGate::RX: {
      size_t target = gate.qubits[0];
      P angle = static_cast<P>(gate.params[0]);
      P c = std::cos(angle / P(2)), s = std::sin(angle / P(2));
      Complex neg_ic(0, -c * P(0.5));
      P neg_s_half = -s * P(0.5);
      for (size_t idx = 0; idx < dim / 2; ++idx) {
        size_t i0 =
            ((idx >> target) << (target + 1)) | (idx & ((1ULL << target) - 1));
        size_t i1 = i0 | (1ULL << target);
        Complex v0 = psi[i0], v1 = psi[i1];
        out[i0] += neg_s_half * v0 + neg_ic * v1;
        out[i1] += neg_ic * v0 + neg_s_half * v1;
      }
      break;
    }
    case QuantumRegister::RecordedGate::RZ: {
      size_t target = gate.qubits[0];
      P angle = static_cast<P>(gate.params[0]);
      Complex d0 =
          P(0.5) * Complex(0, -1) * std::exp(Complex(0, -angle / P(2)));
      Complex d1 = P(0.5) * Complex(0, 1) * std::exp(Complex(0, angle / P(2)));
      for (size_t idx = 0; idx < dim / 2; ++idx) {
        size_t i0 =
            ((idx >> target) << (target + 1)) | (idx & ((1ULL << target) - 1));
        size_t i1 = i0 | (1ULL << target);
        out[i0] += d0 * psi[i0];
        out[i1] += d1 * psi[i1];
      }
      break;
    }
    default:
      break;
    }
  }
};
