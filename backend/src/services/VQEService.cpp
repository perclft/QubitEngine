#include "VQEService.hpp"
#include "../QuantumRegister.hpp"
#include "../QuantumDifferentiator.hpp"
#include "../HardwareConfig.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <random>

namespace qubit_engine {
namespace services {

// Struct for Hamiltonian terms
struct PauliTerm {
  double coefficient;
  std::string pauli_string;
};

// Typedef for Ansatz function
using AnsatzFunction = std::function<void(const std::vector<double> &, QuantumRegister &)>;

grpc::Status VQEService::RunVQE(
    grpc::ServerContext *context, const qubit_engine::VQERequest *request,
    grpc::ServerWriter<qubit_engine::VQEResponse> *writer) {

  spdlog::info("VQEService: Starting VQE Optimization...");

  int num_qubits = 0;
  std::vector<PauliTerm> hamiltonian;

  if (request->observables_size() > 0) {
    num_qubits = request->observables(0).pauli_string().length();
    for (const auto &obs : request->observables()) {
      hamiltonian.push_back({obs.coefficient(), obs.pauli_string()});
    }
  } else {
    // Fallback logic for deprecated molecules
    auto molType = (request->molecule() == qubit_engine::VQERequest::LiH)
                       ? MolecularHamiltonian::LiH
                       : MolecularHamiltonian::H2;
    num_qubits = MolecularHamiltonian::getNumQubits(molType);
    hamiltonian = MolecularHamiltonian::getHamiltonian(molType);
  }

  AnsatzFunction applyAnsatz = [](const std::vector<double> &p, QuantumRegister &qreg) {
    int n = qreg.getNumQubits();
    for (int i = 0; i < n; ++i) {
        if (i < (int)p.size()) qreg.applyRotationY(i, p[i]);
    }
    for (int i = 0; i < n - 1; ++i) {
        qreg.applyCNOT(i, i + 1);
    }
    for (int i = 0; i < n; ++i) {
        if (i + n < (int)p.size()) qreg.applyRotationY(i, p[i + n]);
    }
  };

  std::vector<double> params(num_qubits * 2, 0.0); 
  double learning_rate = request->learning_rate() > 0 ? request->learning_rate() : 0.1;
  int max_iters = request->max_iterations();
  bool use_gradient_descent = (request->optimizer_type() == qubit_engine::VQERequest::GRADIENT_DESCENT);

  double alpha = 0.602, gamma = 0.101, A = max_iters * 0.1, a = learning_rate, c = 0.1;

  for (int k = 0; k < max_iters; k++) {
    double current_energy = 0.0;
    auto evalEnergy = [&](const std::vector<double> &p) -> double {
      QuantumRegister qreg(num_qubits);
      applyAnsatz(p, qreg);
      double energy = 0.0;
      for (const auto &term : hamiltonian) {
        energy += term.coefficient * qreg.expectationValue(term.pauli_string);
      }
      return energy;
    };

    if (use_gradient_descent) {
      auto grads = QuantumDifferentiator::calculateGradients(num_qubits, params, applyAnsatz, hamiltonian);
      for (size_t i = 0; i < params.size(); ++i) {
        params[i] -= learning_rate * grads[i];
      }
      current_energy = evalEnergy(params);
    } else {
      double ak = a / std::pow(k + 1 + A, alpha);
      double ck = c / std::pow(k + 1, gamma);
      std::vector<double> delta(params.size());
      thread_local std::mt19937 gen(std::random_device{}());
      std::bernoulli_distribution dist(0.5);
      for (size_t i = 0; i < params.size(); ++i) delta[i] = dist(gen) ? 1.0 : -1.0;
      std::vector<double> p_plus = params, p_minus = params;
      for (size_t i = 0; i < params.size(); ++i) {
        p_plus[i] += ck * delta[i];
        p_minus[i] -= ck * delta[i];
      }
      double E_plus = evalEnergy(p_plus);
      double E_minus = evalEnergy(p_minus);
      double g_est = (E_plus - E_minus) / (2.0 * ck);
      for (size_t i = 0; i < params.size(); ++i) {
        params[i] -= ak * g_est * delta[i];
      }
      current_energy = evalEnergy(params);
    }

    if (k % 1 == 0 || k == max_iters - 1) {
      qubit_engine::VQEResponse resp;
      resp.set_iteration(k);
      resp.set_energy(current_energy);
      for (double p : params) resp.add_parameters(p);
      resp.set_converged(false);
      double target_energy = (num_qubits == 4) ? -7.86 : -1.13;
      if (current_energy < target_energy + 0.001) {
        resp.set_converged(true);
        writer->Write(resp);
        break;
      }
      writer->Write(resp);
    }
  }

  return grpc::Status::OK;
}

} // namespace services
} // namespace qubit_engine
