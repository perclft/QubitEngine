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
#pragma omp parallel for
  for (int i = 0; i < static_cast<int>(num_params); ++i) {
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
#ifdef _OPENMP
#pragma omp parallel for reduction(+:energy) schedule(static)
#endif
  for (int i = 0; i < static_cast<int>(hamiltonian.size()); ++i) {
    const auto &term = hamiltonian[i];
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
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (long long idx = 0; idx < static_cast<long long>(dim / 2); ++idx) {
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
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (long long idx = 0; idx < static_cast<long long>(dim / 2); ++idx) {
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
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (long long idx = 0; idx < static_cast<long long>(dim / 2); ++idx) {
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
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (long long idx = 0; idx < static_cast<long long>(dim / 2); ++idx) {
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

// --- GPU Adjoint Differentiation ---


#ifdef ENABLE_CUDA
#include "kernels/GateKernels.hpp"
#include "backends/CudaBackend.hpp"
#endif

std::vector<double> QuantumDifferentiator::calculateGradientsAdjointGPU(
    int num_qubits,
    const std::vector<double> &current_params,
    AnsatzFunc<QuantumRegister> applyAnsatz,
    const std::vector<PauliTerm> &hamiltonian) {
  
  size_t num_params = current_params.size();
  std::vector<double> gradients(num_params, 0.0);

#ifndef ENABLE_CUDA
  // Fallback to CPU if CUDA is not enabled during build
  return calculateGradientsAdjoint<QuantumRegister>(num_qubits, current_params, applyAnsatz, hamiltonian);
#else
  if (hamiltonian.empty() || num_params == 0) {
    return gradients;
  }

  // --- Step 1: Record the circuit tape using CPU QuantumRegister ---
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

  // --- Step 2: Forward pass on GPU to get |ψ⟩ ---
  QuantumRegister psi_reg(num_qubits, std::make_unique<CudaBackend>(num_qubits));
  for (const auto& gate : tape) {
    psi_reg.applyRegisteredGate(gate);
  }
  
  size_t dim = 1ULL << num_qubits;
  size_t size_bytes = dim * 2 * sizeof(double); // cuDoubleComplex size
  
  // psi_ptr is managed by psi_reg, do not free it manually!
  void* psi_ptr = dynamic_cast<CudaBackend*>(psi_reg.getBackend())->getDeviceState();

  // --- Step 3: Compute |λ⟩ = H|ψ⟩ on GPU ---
  void* lambda_ptr = qe::cuda::allocateDeviceState(size_bytes);
  qe::cuda::setDeviceStateZero(lambda_ptr, size_bytes);

  void* pauli_psi_ptr = qe::cuda::allocateDeviceState(size_bytes);
  
  for (const auto &term : hamiltonian) {
    qe::cuda::setDeviceStateZero(pauli_psi_ptr, size_bytes);
    
    // Encode Pauli ops (matching direct qubit mapping)
    // 0=I, 1=X, 2=Y, 3=Z
    std::vector<int> pauli_ops(num_qubits, 0);
    for (size_t q = 0; q < static_cast<size_t>(num_qubits) && q < term.pauli_string.size(); ++q) {
      char op = term.pauli_string[q];
      if (op == 'X') pauli_ops[q] = 1;
      else if (op == 'Y') pauli_ops[q] = 2;
      else if (op == 'Z') pauli_ops[q] = 3;
    }

    int* d_pauli_ops;
    cudaMalloc(&d_pauli_ops, num_qubits * sizeof(int));
    cudaMemcpy(d_pauli_ops, pauli_ops.data(), num_qubits * sizeof(int), cudaMemcpyHostToDevice);

    // Apply term.coefficient * P|ψ⟩ and add to lambda
    qe::cuda::launchApplyPauliTerm(lambda_ptr, psi_ptr, num_qubits, d_pauli_ops, term.coefficient);
    cudaFree(d_pauli_ops);
  }
  
  qe::cuda::freeDeviceState(pauli_psi_ptr);

  // --- Step 4: Backward pass ---
  void* dpsi_ptr = qe::cuda::allocateDeviceState(size_bytes);
  // Device pointer for single double result
  double* d_grad_out;
  cudaMalloc(&d_grad_out, sizeof(double));

  for (int i = static_cast<int>(tape.size()) - 1; i >= 0; --i) {
    const auto &gate = tape[i];

    // a. Un-apply gate from |ψ⟩
    psi_reg.applyRegisteredGateInverse(gate);

    // b. If parameterized, compute gradient contribution
    if (tape_param_index[i] >= 0) {
      int pidx = tape_param_index[i];

      // Compute dU/dθ |ψ⟩ => dpsi
      qe::cuda::setDeviceStateZero(dpsi_ptr, size_bytes);
      if (gate.type == QuantumRegister::RecordedGate::RY) {
        qe::cuda::launchDerivativeRY(dpsi_ptr, psi_ptr, num_qubits, gate.qubits[0], gate.params[0]);
      } else if (gate.type == QuantumRegister::RecordedGate::RX) {
        qe::cuda::launchDerivativeRX(dpsi_ptr, psi_ptr, num_qubits, gate.qubits[0], gate.params[0]);
      } else if (gate.type == QuantumRegister::RecordedGate::RZ) {
        qe::cuda::launchDerivativeRZ(dpsi_ptr, psi_ptr, num_qubits, gate.qubits[0], gate.params[0]);
      }

      // clear d_grad_out
      cudaMemset(d_grad_out, 0, sizeof(double));

      // grad[pidx] += 2 * Re(⟨λ|dU/dθ|ψ⟩)
      qe::cuda::launchAdjointInnerProduct(dpsi_ptr, lambda_ptr, d_grad_out, dim);
      
      double grad_val = 0.0;
      cudaMemcpy(&grad_val, d_grad_out, sizeof(double), cudaMemcpyDeviceToHost);
      gradients[pidx] += grad_val;
    }

    // c. Un-apply gate from |λ⟩
    // To do this, we temporarily wrap lambda_ptr in a dummy call or just call the Inverse kernels directly. 
    // We already have inverse kernels available via launchHadamard, etc.
    // Let's implement static applyInverse wrapper locally.
    auto applyInv = [&](void* device_state_ptr) {
      if (gate.type == QuantumRegister::RecordedGate::H) qe::cuda::launchHadamard(device_state_ptr, num_qubits, gate.qubits[0]);
      else if (gate.type == QuantumRegister::RecordedGate::X) qe::cuda::launchapplyX(device_state_ptr, num_qubits, gate.qubits[0]);
      else if (gate.type == QuantumRegister::RecordedGate::Y) qe::cuda::launchapplyY(device_state_ptr, num_qubits, gate.qubits[0]);
      else if (gate.type == QuantumRegister::RecordedGate::Z) qe::cuda::launchapplyZ(device_state_ptr, num_qubits, gate.qubits[0]);
      else if (gate.type == QuantumRegister::RecordedGate::CNOT) qe::cuda::launchCNOT(device_state_ptr, num_qubits, gate.qubits[0], gate.qubits[1]);
      else if (gate.type == QuantumRegister::RecordedGate::CZ) qe::cuda::launchCZ(device_state_ptr, num_qubits, gate.qubits[0], gate.qubits[1]);
      else if (gate.type == QuantumRegister::RecordedGate::SWAP) qe::cuda::launchSWAP(device_state_ptr, num_qubits, gate.qubits[0], gate.qubits[1]);
      else if (gate.type == QuantumRegister::RecordedGate::RX) qe::cuda::launchRotationX(device_state_ptr, num_qubits, gate.qubits[0], -gate.params[0]);
      else if (gate.type == QuantumRegister::RecordedGate::RY) qe::cuda::launchRotationY(device_state_ptr, num_qubits, gate.qubits[0], -gate.params[0]);
      else if (gate.type == QuantumRegister::RecordedGate::RZ) qe::cuda::launchRotationZ(device_state_ptr, num_qubits, gate.qubits[0], -gate.params[0]);
      else if (gate.type == QuantumRegister::RecordedGate::PHASE_S) qe::cuda::launchRotationZ(device_state_ptr, num_qubits, gate.qubits[0], -M_PI/2.0);
      else if (gate.type == QuantumRegister::RecordedGate::PHASE_T) qe::cuda::launchRotationZ(device_state_ptr, num_qubits, gate.qubits[0], -M_PI/4.0);
      else if (gate.type == QuantumRegister::RecordedGate::TOFFOLI) qe::cuda::launchToffoli(device_state_ptr, num_qubits, gate.qubits[0], gate.qubits[1], gate.qubits[2]);
    };
    applyInv(lambda_ptr);
  }

  cudaFree(d_grad_out);
  qe::cuda::freeDeviceState(dpsi_ptr);
  qe::cuda::freeDeviceState(lambda_ptr);

  return gradients;
#endif
}
