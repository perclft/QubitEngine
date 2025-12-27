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
  // Template Ansatz Function
  template <typename RegisterType>
  using AnsatzFunc =
      std::function<void(const std::vector<double> &, RegisterType &)>;

  // Calculates gradients using the Adjoint Method (Reversible Computing)
  template <typename RegisterType>
  static std::vector<double>
  calculateGradientsAdjoint(int num_qubits,
                            const std::vector<double> &current_params,
                            AnsatzFunc<RegisterType> applyAnsatz,
                            const std::vector<PauliTerm> &hamiltonian) {

    // 1. Record the Circuit (Trace) using CPU Register (always fast for just
    // recording)
    QuantumRegister trace_reg(num_qubits);
    trace_reg.enableRecording(true);
    // trace_reg expects AnsatzFunction = function<void(params,
    // QuantumRegister&)>

    // We need to bridge the types if RegisterType is NOT QuantumRegister.
    // However, the ansatz function provided by user might be generic or
    // specific. If the user provides a Python function, we wrappped it in
    // python_bindings. We should pass a "recording" version of the ansatz.
    // Problem: applyAnsatz is typed to RegisterType.
    // Solution: The user (python bindings) should provide TWO functions or a
    // generic one? Simplified: Just instantiate generic lambda in bindings.

    // BUT: Here we need to call applyAnsatz with trace_reg.
    // If applyAnsatz expects GPUQuantumRegister, passing QuantumRegister will
    // fail.

    // Hack for MVP: We assume we can construct a "Tape" externally or we use
    // RegisterType for recording too? GPUQuantumRegister doesn't support
    // recording logic yet (it has applyRegisteredGate but not enableRecording).

    // Better: We admit that QuantumDifferentiator is "Adjoint on CPU" or
    // "Adjoint on GPU". If GPU, we STILL need a Tape. Let's assume we pass in
    // the TAPE directly? No, signature change.

    // Let's try to assume RegisterType supports enableRecording.
    // I'll add enableRecording to GPUQuantumRegister (dummy or delegate to CPU
    // shadow).

    // Actually, simpler: Use 'RegisterType' for the trace too.
    RegisterType trace_instance(num_qubits);
    // trace_instance.enableRecording(true); // GPU Reg needs this method.

    // Wait, GPUQuantumRegister is strictly execution.
    // Mixed approach: The Python bindings usually define the ansatz logic.
    // If I change the signature to take "Ansatz for Tracing" and "Ansatz for
    // Execution", it's clean.

    // Let's stick to CPU recording for now.
    // We simply reconstruct the tape by running a "fake" pass if possible.
    // But applyAnsatz takes RegisterType&.
    // If RegisterType=GPUQuantumRegister, we can't pass QuantumRegister.

    // CRITICAL FIX: The wrapper in python_bindings converts python func to C++
    // lambda. We can create a NEW C++ lambda for QuantumRegister inside
    // bindings? No, calculateGradientsAdjoint is called with ONE function.

    // To support this without massive refactor:
    // We add 'getTape' capability to GPUQuantumRegister?
    // Or we make QuantumDifferentiator take a `Tape` as input?
    // Or we accept `std::function<void(QuantumRegister&)>` for tracing as an
    // extra arg.

    // Let's add an overloaded `calculateGradientsAdjoint` that takes a `tape`.
    // Then python_bindings generates the tape using CPU reg, and passes it to
    // GPU solver.

    return std::vector<double>(); // Placeholder for this thought block tool
                                  // update
  }
};
