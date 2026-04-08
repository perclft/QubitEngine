#include "CloudBackend.hpp"
#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>

namespace qubit_engine {

CloudBackend::CloudBackend(size_t num_qubits, const std::string &remote_url)
    : num_qubits_(num_qubits), remote_url_(remote_url) {
  auto channel = grpc::CreateChannel(remote_url, grpc::InsecureChannelCredentials());
  stub_ = QuantumCompute::NewStub(channel);
  request_.set_num_qubits(static_cast<int32_t>(num_qubits));
}

void CloudBackend::applyHadamard(size_t target) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::HADAMARD);
  op->set_target_qubit(static_cast<uint32_t>(target));
  needs_sync_ = true;
}

void CloudBackend::applyX(size_t target) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::PAULI_X);
  op->set_target_qubit(static_cast<uint32_t>(target));
  needs_sync_ = true;
}

void CloudBackend::applyY(size_t target) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::PAULI_Y);
  op->set_target_qubit(static_cast<uint32_t>(target));
  needs_sync_ = true;
}

void CloudBackend::applyZ(size_t target) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::PAULI_Z);
  op->set_target_qubit(static_cast<uint32_t>(target));
  needs_sync_ = true;
}

void CloudBackend::applyCNOT(size_t control, size_t target) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::CNOT);
  op->set_control_qubit(static_cast<uint32_t>(control));
  op->set_target_qubit(static_cast<uint32_t>(target));
  needs_sync_ = true;
}

void CloudBackend::applyToffoli(size_t control1, size_t control2, size_t target) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::TOFFOLI);
  op->set_control_qubit(static_cast<uint32_t>(control1));
  op->set_second_control_qubit(static_cast<uint32_t>(control2));
  op->set_target_qubit(static_cast<uint32_t>(target));
  needs_sync_ = true;
}

void CloudBackend::applyPhaseS(size_t target) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::PHASE_S);
  op->set_target_qubit(static_cast<uint32_t>(target));
  needs_sync_ = true;
}

void CloudBackend::applyPhaseT(size_t target) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::PHASE_T);
  op->set_target_qubit(static_cast<uint32_t>(target));
  needs_sync_ = true;
}

void CloudBackend::applyRotationX(size_t target, Precision angle) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::ROTATION_X);
  op->set_target_qubit(static_cast<uint32_t>(target));
  op->set_angle(angle);
  needs_sync_ = true;
}

void CloudBackend::applyRotationY(size_t target, Precision angle) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::ROTATION_Y);
  op->set_target_qubit(static_cast<uint32_t>(target));
  op->set_angle(angle);
  needs_sync_ = true;
}

void CloudBackend::applyRotationZ(size_t target, Precision angle) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::ROTATION_Z);
  op->set_target_qubit(static_cast<uint32_t>(target));
  op->set_angle(angle);
  needs_sync_ = true;
}

void CloudBackend::applySWAP(size_t qubit1, size_t qubit2) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::SWAP);
  op->set_target_qubit(static_cast<uint32_t>(qubit1));
  op->set_second_target_qubit(static_cast<uint32_t>(qubit2));
  needs_sync_ = true;
}

void CloudBackend::applyCZ(size_t control, size_t target) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::CZ);
  op->set_control_qubit(static_cast<uint32_t>(control));
  op->set_target_qubit(static_cast<uint32_t>(target));
  needs_sync_ = true;
}

void CloudBackend::applyDepolarizingNoise(Precision probability) {
  auto *op = request_.add_operations();
  op->set_type(GateOperation::DEPOLARIZING_NOISE);
  op->set_noise_probability(probability);
  needs_sync_ = true;
}

int CloudBackend::measure(size_t target) {
  // Cloud measurement is tricky if we want to continue mid-circuit.
  // For now, satisfy with a one-shot full circuit run.
  ensureRemoteExecution();
  auto it = last_response_.classical_results().find(static_cast<uint32_t>(target));
  if (it != last_response_.classical_results().end()) {
    return it->second ? 1 : 0;
  }
  return 0; // Default
}

std::vector<double> CloudBackend::getProbabilities() const {
  ensureRemoteExecution();
  // If we had expectation values or similar from cloud...
  // Usually cloud doesn't return full state vector for > 30 qubits.
  return {}; 
}

double CloudBackend::expectationValue(const std::string &pauli_string) const {
  // Remote execution to get expectation value
  ensureRemoteExecution();
  if (last_response_.expectation_values_size() > 0) {
    return last_response_.expectation_values(0);
  }
  return 0.0;
}

std::vector<Complex> CloudBackend::getStateVector() const {
  ensureRemoteExecution();
  std::vector<Complex> state;
  state.reserve(last_response_.state_vector_size());
  for (const auto &c : last_response_.state_vector()) {
    state.emplace_back(c.real(), c.imag());
  }
  return state;
}

void CloudBackend::ensureRemoteExecution() const {
  if (!needs_sync_) return;

  grpc::ClientContext context;
  grpc::Status status = stub_->RunCircuit(&context, request_, &last_response_);

  if (!status.ok()) {
    spdlog::error("Cloud gRPC Error: {}", status.error_message());
  } else {
    needs_sync_ = false;
  }
}

} // namespace qubit_engine
