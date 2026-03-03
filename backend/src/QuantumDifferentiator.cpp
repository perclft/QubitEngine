#include "QuantumDifferentiator.hpp"

#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef MPI_ENABLED
#include <mpi.h>
#endif

using Complex = qubit_engine::Complex;
using P = qubit_engine::Precision;

// --- Parameter Shift Rule ---
std::vector<double> QuantumDifferentiator::calculateGradients(
    int num_qubits, const std::vector<double> &current_params,
    AnsatzFunction applyAnsatz, const std::vector<PauliTerm> &hamiltonian) {
  std::vector<double> gradients(current_params.size(), 0.0);

  const double SHIFT = M_PI / 2.0;

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

  size_t num_params = current_params.size();

#ifdef MPI_ENABLED
  if (size > 1) {
    if (rank == 0) {
      // MASTER NODE
      std::vector<double> gradients_buf(num_params, 0.0);
      size_t next_param = 0;

      // Seed workers
      for (int p = 1; p < size; ++p) {
        int p_idx = -1;
        if (next_param < num_params) {
          p_idx = static_cast<int>(next_param++);
        }
        MPI_Send(&p_idx, 1, MPI_INT, p, 0, MPI_COMM_WORLD);
      }

      // Collect and dispatch
      int active_workers =
          (next_param > 0) ? std::min(static_cast<int>(num_params), size - 1)
                           : 0;
      while (active_workers > 0) {
        double result;
        MPI_Status status;
        MPI_Recv(&result, 1, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG,
                 MPI_COMM_WORLD, &status);

        int worker = status.MPI_SOURCE;
        int param_idx = status.MPI_TAG;
        gradients_buf[param_idx] = result;

        int p_idx = -1;
        if (next_param < num_params) {
          p_idx = static_cast<int>(next_param++);
        } else {
          active_workers--;
        }
        MPI_Send(&p_idx, 1, MPI_INT, worker, 0, MPI_COMM_WORLD);
      }
      gradients = gradients_buf;
    } else {
      // WORKER NODE
      while (true) {
        int param_idx;
        MPI_Status status;
        MPI_Recv(&param_idx, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        if (param_idx == -1)
          break;

        std::vector<double> params_plus = current_params;
        params_plus[param_idx] += SHIFT;
        double energy_plus =
            evaluateEnergy(num_qubits, params_plus, applyAnsatz, hamiltonian);

        std::vector<double> params_minus = current_params;
        params_minus[param_idx] -= SHIFT;
        double energy_minus =
            evaluateEnergy(num_qubits, params_minus, applyAnsatz, hamiltonian);

        double result = 0.5 * (energy_plus - energy_minus);
        MPI_Send(&result, 1, MPI_DOUBLE, 0, param_idx, MPI_COMM_WORLD);
      }
    }
    return gradients;
  }
#endif

  // SCALAR FALLBACK / SINGLE NODE EXECUTION
  for (size_t i = 0; i < num_params; ++i) {
    std::vector<double> params_plus = current_params;
    params_plus[i] += SHIFT;
    double energy_plus =
        evaluateEnergy(num_qubits, params_plus, applyAnsatz, hamiltonian);

    std::vector<double> params_minus = current_params;
    params_minus[i] -= SHIFT;
    double energy_minus =
        evaluateEnergy(num_qubits, params_minus, applyAnsatz, hamiltonian);

    gradients[i] = 0.5 * (energy_plus - energy_minus);
  }

  return gradients;
}

// --- Energy Evaluation ---
double QuantumDifferentiator::evaluateEnergy(
    int num_qubits, const std::vector<double> &params,
    AnsatzFunction applyAnsatz, const std::vector<PauliTerm> &hamiltonian) {
  QuantumRegister qreg(num_qubits, true);
  applyAnsatz(params, qreg);
  double energy = 0.0;
  for (const auto &term : hamiltonian) {
    energy += term.coefficient * qreg.expectationValue(term.pauli_string);
  }
  return energy;
}

// --- Gate Inverse Application ---
void QuantumDifferentiator::applyGateInverseToState(
    std::vector<Complex> &state, const QuantumRegister::RecordedGate &gate,
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

// --- Gate Derivative Application ---
void QuantumDifferentiator::applyGateDerivativeToState(
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
    Complex d0 = P(0.5) * Complex(0, -1) * std::exp(Complex(0, -angle / P(2)));
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
