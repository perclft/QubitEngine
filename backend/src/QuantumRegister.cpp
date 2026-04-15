#include "QuantumRegister.hpp"
#include "Types.hpp"
#include <optional>
#include "QuantumMetrics.hpp"
#include "CircuitOptimizer.hpp"
#include "ConfigManager.hpp"
#define _USE_MATH_DEFINES
#include <cmath>
#include "Exceptions.hpp"
#include "backends/CpuBackend.hpp"
#include "backends/CudaBackend.hpp"
#include "backends/MPSBackend.hpp"
#include "backends/CloudBackend.hpp"
#include <cmath>
#include <cstdint>
#include <future>
#include <iostream>
#include <spdlog/spdlog.h>
#include <random>
#include <stdexcept>
#include <string>

#ifdef ENABLE_OPENTELEMETRY
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/tracer.h>
#endif
#include <memory>
#include <complex>

// Centralized M_PI used

namespace qubit_engine {

QuantumRegister::QuantumRegister(size_t n, bool force_local) : num_qubits(n) {
  // Phase 25: Security Enforcement Check
  const char* secret = std::getenv("QUBIT_ENGINE_JWT_SECRET");
  const char* skip = std::getenv("QUBIT_ENGINE_SKIP_AUTH");
  if (!secret && (!skip || std::string(skip) != "1")) {
      spdlog::warn("QUBIT_ENGINE_JWT_SECRET is not set. Simulation security may be compromised.");
      spdlog::warn("Set QUBIT_ENGINE_SKIP_AUTH=1 to explicitly disable this warning during development.");
  }

  // Check ConfigManager for force local execution override if not explicitly specified
  bool use_local = force_local || ConfigManager::Instance().forceLocalExecution();

  // Phase 4: Cloud Offloading Check
  auto cloud_url = ConfigManager::Instance().getCloudUrl();
  if (cloud_url.has_value() && !use_local) {
    backend = std::make_unique<CloudBackend>(n, cloud_url.value());
    spdlog::info("QuantumRegister: Using CloudBackend (Remote: {})", cloud_url.value());
    return;
  }

  // Phase 4: Tensor Network Acceleration
  int mps_threshold = ConfigManager::Instance().getMpsThreshold();
  if (n >= static_cast<size_t>(mps_threshold) && !use_local) {
    // Emulate using MPS for circuits >= threshold to showcase memory
    // compression
    int bond_dim = ConfigManager::Instance().getMpsBondDimension();
    backend = std::make_unique<MPSBackend>(static_cast<int>(n), bond_dim);
    spdlog::info("QuantumRegister: Using MPSBackend (bond_dim={})", bond_dim);
    return;
  }

  // Factory Logic
#ifdef ENABLE_CUDA
  if (!use_local) {
    try {
      // Stub: In real imp, check device count e.g. cudaGetDeviceCount
      backend = std::make_unique<CudaBackend>(n);
      spdlog::info("QuantumRegister: Using CudaBackend (GPU)");
      return;
    } catch (...) {
      spdlog::error("QuantumRegister: CudaBackend failed. Falling back.");
    }
  }
#endif

  // Default to CPU on macOS/Linux/Windows if CUDA not used
  backend = std::make_unique<CpuBackend>(n, use_local);
}

QuantumRegister::~QuantumRegister() {}

void QuantumRegister::validateQubit(size_t q) const {
  if (q >= num_qubits)
    throw QubitOutOfRangeException("Qubit index " + std::to_string(q) +
                            " out of range (num_qubits=" +
                            std::to_string(num_qubits) + ")");
}

// --- Core Gates ---

void QuantumRegister::applyHadamard(size_t target) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::H, {target}, {}});
  if (execution_enabled) {
    backend->applyHadamard(target);
    QuantumMetrics::Instance().RecordGateApplication();
    applyPostGateNoise1Q(target);
  }
}

void QuantumRegister::applyX(size_t target) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::X, {target}, {}});
  if (execution_enabled) {
    backend->applyX(target);
    QuantumMetrics::Instance().RecordGateApplication();
    applyPostGateNoise1Q(target);
  }
}

void QuantumRegister::applyY(size_t target) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::Y, {target}, {}});
  if (execution_enabled) {
    backend->applyY(target);
    QuantumMetrics::Instance().RecordGateApplication();
    applyPostGateNoise1Q(target);
  }
}

void QuantumRegister::applyZ(size_t target) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::Z, {target}, {}});
  if (execution_enabled) {
    backend->applyZ(target);
    QuantumMetrics::Instance().RecordGateApplication();
    applyPostGateNoise1Q(target);
  }
}

void QuantumRegister::applyCNOT(size_t control, size_t target) {
  validateQubit(control);
  validateQubit(target);
  if (control == target)
    throw InvalidArgumentException("Control and target qubits must be distinct");
  if (recording_enabled)
    tape.push_back({RecordedGate::CNOT, {control, target}, {}});
  if (execution_enabled) {
    backend->applyCNOT(control, target);
    applyPostGateNoise2Q(control, target);
  }
}

// --- Advanced Gates ---

void QuantumRegister::applyToffoli(size_t c1, size_t c2, size_t t) {
  validateQubit(c1);
  validateQubit(c2);
  validateQubit(t);
  if (c1 == c2 || c1 == t || c2 == t)
    throw InvalidArgumentException("Control and target qubits must be distinct");
  if (recording_enabled)
    tape.push_back({RecordedGate::TOFFOLI, {c1, c2, t}, {}});
  if (execution_enabled) {
    backend->applyToffoli(c1, c2, t);
    // Toffoli is a 3-qubit gate; apply 2Q noise to all pairs involved
    applyPostGateNoise2Q(c1, t);
    applyPostGateNoise2Q(c2, t);
  }
}

void QuantumRegister::applyPhaseS(size_t target) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::PHASE_S, {target}, {}});
  if (execution_enabled) {
    backend->applyPhaseS(target);
    applyPostGateNoise1Q(target);
  }
}

void QuantumRegister::applyPhaseT(size_t target) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::PHASE_T, {target}, {}});
  if (execution_enabled) {
    backend->applyPhaseT(target);
    applyPostGateNoise1Q(target);
  }
}

void QuantumRegister::applyRotationY(size_t target, Precision angle) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::RY, {target}, {(double)angle}});
  if (execution_enabled) {
    backend->applyRotationY(target, angle);
    applyPostGateNoise1Q(target);
  }
}

void QuantumRegister::applyRotationZ(size_t target, Precision angle) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::RZ, {target}, {(double)angle}});
  if (execution_enabled) {
    backend->applyRotationZ(target, angle);
    applyPostGateNoise1Q(target);
  }
}

void QuantumRegister::applyRotationX(size_t target, Precision angle) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::RX, {target}, {(double)angle}});
  if (execution_enabled) {
    backend->applyRotationX(target, angle);
    applyPostGateNoise1Q(target);
  }
}

void QuantumRegister::applySWAP(size_t qubit1, size_t qubit2) {
  validateQubit(qubit1);
  validateQubit(qubit2);
  if (qubit1 == qubit2)
    throw InvalidArgumentException("Qubits must be distinct");
  if (recording_enabled)
    tape.push_back({RecordedGate::SWAP, {qubit1, qubit2}, {}});
  if (execution_enabled) {
    backend->applySWAP(qubit1, qubit2);
    applyPostGateNoise2Q(qubit1, qubit2);
  }
}

void QuantumRegister::applyCZ(size_t control, size_t target) {
  validateQubit(control);
  validateQubit(target);
  if (control == target)
    throw InvalidArgumentException("Control and target must be distinct");
  if (recording_enabled)
    tape.push_back({RecordedGate::CZ, {control, target}, {}});
  if (execution_enabled) {
    backend->applyCZ(control, target);
    applyPostGateNoise2Q(control, target);
  }
}

void QuantumRegister::applyDenseUnitary(const std::vector<size_t> &targets,
                                       const std::vector<Complex> &matrix) {
  for (size_t q : targets) validateQubit(q);
  // Dense unitaries are usually JIT-produced and not recorded to tape to avoid
  // bloating it, but they are executed directly.
  if (execution_enabled)
    backend->applyDenseUnitary(targets, matrix);
}

// --- Noise ---

void QuantumRegister::applyDepolarizingNoise(Precision probability) {
  if (execution_enabled)
    backend->applyDepolarizingNoise(probability);
}

void QuantumRegister::setNoiseModel(const NoiseModel& model) {
  noise_model_ = std::make_shared<NoiseModel>(model);
}

const NoiseModel* QuantumRegister::getNoiseModel() const {
  return noise_model_ ? noise_model_.get() : nullptr;
}

void QuantumRegister::applyPostGateNoise1Q(size_t target) {
  if (!noise_model_ || !noise_model_->isEnabled()) return;
  for (const auto& channel : noise_model_->getSingleQubitChannels()) {
    backend->applyNoiseChannel1Q(channel, target);
  }
}

void QuantumRegister::applyPostGateNoise2Q(size_t q1, size_t q2) {
  if (!noise_model_ || !noise_model_->isEnabled()) return;
  // Apply single-qubit channels to both qubits involved
  for (const auto& channel : noise_model_->getSingleQubitChannels()) {
    backend->applyNoiseChannel1Q(channel, q1);
    backend->applyNoiseChannel1Q(channel, q2);
  }
  // Apply two-qubit channels to the pair
  for (const auto& channel : noise_model_->getTwoQubitChannels()) {
    backend->applyNoiseChannel2Q(channel, q1, q2);
  }
}

// --- Measurement ---

int QuantumRegister::measure(size_t target) {
  if (recording_enabled)
    tape.push_back({RecordedGate::MEASURE, {target}, {}});
  // Apply readout error if noise model is configured
  if (noise_model_ && noise_model_->isEnabled() && noise_model_->hasReadoutError()) {
    return backend->measureWithReadoutError(target, noise_model_->getReadoutError(target));
  }
  return backend->measure(target);
}

std::vector<double> QuantumRegister::getProbabilities() const {
  return backend->getProbabilities();
}

double QuantumRegister::expectationValue(const std::string &pauli_string) const {
  return backend->expectationValue(pauli_string);
}

// --- Distributed Helpers ---
int QuantumRegister::getRank() const { return backend->getRank(); }
int QuantumRegister::getSize() const { return backend->getSize(); }

// --- Debugging ---
std::vector<Complex> QuantumRegister::getStateVector() const {
  return backend->getStateVector();
}

// --- Tape Management ---

void QuantumRegister::enableRecording(bool enable) {
  recording_enabled = enable;
}
void QuantumRegister::enableExecution(bool enable) {
  execution_enabled = enable;
}
void QuantumRegister::clearTape() { tape.clear(); }
const std::vector<QuantumRegister::RecordedGate> &
QuantumRegister::getTape() const {
  return tape;
}

void QuantumRegister::optimize() {
#ifdef ENABLE_OPENTELEMETRY
  auto tracer = opentelemetry::trace::Provider::GetTracerProvider()->GetTracer("QubitEngine");
  auto span = tracer->StartSpan("QuantumRegister::optimize");
  auto scope = tracer->WithActiveSpan(span);
#endif
  spdlog::debug("Optimizing circuit tape of size {}", tape.size());
  CircuitOptimizer::optimize(tape);
}

void QuantumRegister::mapTo1DTopology() {
#ifdef ENABLE_OPENTELEMETRY
  auto tracer = opentelemetry::trace::Provider::GetTracerProvider()->GetTracer("QubitEngine");
  auto span = tracer->StartSpan("QuantumRegister::mapTo1DTopology");
  auto scope = tracer->WithActiveSpan(span);
#endif
  CircuitOptimizer::mapTo1DTopology(tape);
}

// --- Replay Logic (Kept purely on proxy as it uses public API) ---

void QuantumRegister::applyRegisteredGate(const RecordedGate &gate) {
  switch (gate.type) {
  case RecordedGate::H:      applyHadamard(gate.qubits[0]); break;
  case RecordedGate::X:      applyX(gate.qubits[0]); break;
  case RecordedGate::Y:      applyY(gate.qubits[0]); break;
  case RecordedGate::Z:      applyZ(gate.qubits[0]); break;
  case RecordedGate::CNOT:   applyCNOT(gate.qubits[0], gate.qubits[1]); break;
  case RecordedGate::RX:     applyRotationX(gate.qubits[0], gate.params[0]); break;
  case RecordedGate::RY:     applyRotationY(gate.qubits[0], gate.params[0]); break;
  case RecordedGate::RZ:     applyRotationZ(gate.qubits[0], gate.params[0]); break;
  case RecordedGate::SWAP:   applySWAP(gate.qubits[0], gate.qubits[1]); break;
  case RecordedGate::CZ:     applyCZ(gate.qubits[0], gate.qubits[1]); break;
  case RecordedGate::TOFFOLI: applyToffoli(gate.qubits[0], gate.qubits[1], gate.qubits[2]); break;
  case RecordedGate::PHASE_S: applyPhaseS(gate.qubits[0]); break;
  case RecordedGate::PHASE_T: applyPhaseT(gate.qubits[0]); break;
  case RecordedGate::MEASURE: break; // Measurements are not replayed
  }
}

void QuantumRegister::applyRegisteredGateInverse(const RecordedGate &gate) {
  switch (gate.type) {
  // Self-inverse gates
  case RecordedGate::H:       applyHadamard(gate.qubits[0]); break;
  case RecordedGate::X:       applyX(gate.qubits[0]); break;
  case RecordedGate::Y:       applyY(gate.qubits[0]); break;
  case RecordedGate::Z:       applyZ(gate.qubits[0]); break;
  case RecordedGate::CNOT:    applyCNOT(gate.qubits[0], gate.qubits[1]); break;
  case RecordedGate::SWAP:    applySWAP(gate.qubits[0], gate.qubits[1]); break;
  case RecordedGate::CZ:      applyCZ(gate.qubits[0], gate.qubits[1]); break;
  case RecordedGate::TOFFOLI: applyToffoli(gate.qubits[0], gate.qubits[1], gate.qubits[2]); break;
  // Rotation inverses (negate angle)
  case RecordedGate::RX:      applyRotationX(gate.qubits[0], -gate.params[0]); break;
  case RecordedGate::RY:      applyRotationY(gate.qubits[0], -gate.params[0]); break;
  case RecordedGate::RZ:      applyRotationZ(gate.qubits[0], -gate.params[0]); break;
  // Phase gate inverses: S† = RZ(-π/2), T† = RZ(-π/4)
  case RecordedGate::PHASE_S: applyRotationZ(gate.qubits[0], -M_PI / 2.0); break;
  case RecordedGate::PHASE_T: applyRotationZ(gate.qubits[0], -M_PI / 4.0); break;
  case RecordedGate::MEASURE: break; // Measurements cannot be inverted
  }
}

} // namespace qubit_engine
