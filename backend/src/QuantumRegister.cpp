#include "QuantumRegister.hpp"
#include "QuantumMetrics.hpp"
#include "CircuitOptimizer.hpp"
#include "ConfigManager.hpp"
#include "Exceptions.hpp"
#include "backends/CpuBackend.hpp"
#include "backends/CudaBackend.hpp"
#include "backends/MPSBackend.hpp"
#include "backends/CloudBackend.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace qubit_engine {

// --- Lifecycle ---
QuantumRegister::QuantumRegister(size_t n, bool force_local) : num_qubits(n) {
  // Check ConfigManager for force local execution override if not explicitly specified
  bool use_local = force_local || ConfigManager::Instance().forceLocalExecution();

  // Phase 4: Cloud Offloading Check
  auto cloud_url = ConfigManager::Instance().getCloudUrl();
  if (cloud_url.has_value() && !use_local) {
    backend = std::make_unique<CloudBackend>(n, cloud_url.value());
    std::cout << "QuantumRegister: Using CloudBackend (Remote: " << cloud_url.value() << ")" << std::endl;
    return;
  }

  // Phase 4: Tensor Network Acceleration
  if (n >= 25 && !use_local) {
    // Emulate using MPS for circuits >= 25 qubits to showcase memory
    // compression
    backend = std::make_unique<MPSBackend>(n);
    return;
  }

  // Factory Logic
#ifdef ENABLE_CUDA
  if (!use_local) {
    try {
      // Stub: In real imp, check device count e.g. cudaGetDeviceCount
      backend = std::make_unique<CudaBackend>(n);
      std::cout << "QuantumRegister: Using CudaBackend (GPU)" << std::endl;
      return;
    } catch (...) {
      std::cerr << "QuantumRegister: CudaBackend failed. Falling back."
                << std::endl;
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
  }
}

void QuantumRegister::applyX(size_t target) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::X, {target}, {}});
  if (execution_enabled) {
    backend->applyX(target);
    QuantumMetrics::Instance().RecordGateApplication();
  }
}

void QuantumRegister::applyY(size_t target) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::Y, {target}, {}});
  if (execution_enabled) {
    backend->applyY(target);
    QuantumMetrics::Instance().RecordGateApplication();
  }
}

void QuantumRegister::applyZ(size_t target) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::Z, {target}, {}});
  if (execution_enabled) {
    backend->applyZ(target);
    QuantumMetrics::Instance().RecordGateApplication();
  }
}

void QuantumRegister::applyCNOT(size_t control, size_t target) {
  validateQubit(control);
  validateQubit(target);
  if (control == target)
    throw InvalidArgumentException("Control and target qubits must be distinct");
  if (recording_enabled)
    tape.push_back({RecordedGate::CNOT, {control, target}, {}});
  if (execution_enabled)
    backend->applyCNOT(control, target);
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
  if (execution_enabled)
    backend->applyToffoli(c1, c2, t);
}

void QuantumRegister::applyPhaseS(size_t target) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::PHASE_S, {target}, {}});
  if (execution_enabled)
    backend->applyPhaseS(target);
}

void QuantumRegister::applyPhaseT(size_t target) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::PHASE_T, {target}, {}});
  if (execution_enabled)
    backend->applyPhaseT(target);
}

void QuantumRegister::applyRotationY(size_t target, Precision angle) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::RY, {target}, {(double)angle}});
  if (execution_enabled)
    backend->applyRotationY(target, angle);
}

void QuantumRegister::applyRotationZ(size_t target, Precision angle) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::RZ, {target}, {(double)angle}});
  if (execution_enabled)
    backend->applyRotationZ(target, angle);
}

void QuantumRegister::applyRotationX(size_t target, Precision angle) {
  validateQubit(target);
  if (recording_enabled)
    tape.push_back({RecordedGate::RX, {target}, {(double)angle}});
  if (execution_enabled)
    backend->applyRotationX(target, angle);
}

void QuantumRegister::applySWAP(size_t qubit1, size_t qubit2) {
  validateQubit(qubit1);
  validateQubit(qubit2);
  if (qubit1 == qubit2)
    throw InvalidArgumentException("Qubits must be distinct");
  if (recording_enabled)
    tape.push_back({RecordedGate::SWAP, {qubit1, qubit2}, {}});
  if (execution_enabled)
    backend->applySWAP(qubit1, qubit2);
}

void QuantumRegister::applyCZ(size_t control, size_t target) {
  validateQubit(control);
  validateQubit(target);
  if (control == target)
    throw InvalidArgumentException("Control and target qubits must be distinct");
  if (recording_enabled)
    tape.push_back({RecordedGate::CZ, {control, target}, {}});
  if (execution_enabled)
    backend->applyCZ(control, target);
}

// --- Noise ---

void QuantumRegister::applyDepolarizingNoise(Precision probability) {
  if (execution_enabled)
    backend->applyDepolarizingNoise(probability);
}

// --- Measurement ---

int QuantumRegister::measure(size_t target) {
  if (recording_enabled)
    tape.push_back({RecordedGate::MEASURE, {target}, {}});
  return backend->measure(target);
}

std::vector<double> QuantumRegister::getProbabilities() {
  return backend->getProbabilities();
}

double QuantumRegister::expectationValue(const std::string &pauli_string) {
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

void QuantumRegister::optimize() { CircuitOptimizer::optimize(tape); }
void QuantumRegister::mapTo1DTopology() {
  CircuitOptimizer::mapTo1DTopology(tape);
}

// --- Replay Logic (Kept purely on proxy as it uses public API) ---

void QuantumRegister::applyRegisteredGate(const RecordedGate &gate) {
  if (gate.type == RecordedGate::H)
    applyHadamard(gate.qubits[0]);
  else if (gate.type == RecordedGate::X)
    applyX(gate.qubits[0]);
  else if (gate.type == RecordedGate::Y)
    applyY(gate.qubits[0]);
  else if (gate.type == RecordedGate::Z)
    applyZ(gate.qubits[0]);
  else if (gate.type == RecordedGate::CNOT)
    applyCNOT(gate.qubits[0], gate.qubits[1]);
  else if (gate.type == RecordedGate::RY)
    applyRotationY(gate.qubits[0], gate.params[0]);
  else if (gate.type == RecordedGate::RZ)
    applyRotationZ(gate.qubits[0], gate.params[0]);
  else if (gate.type == RecordedGate::RX)
    applyRotationX(gate.qubits[0], gate.params[0]);
  else if (gate.type == RecordedGate::SWAP)
    applySWAP(gate.qubits[0], gate.qubits[1]);
  else if (gate.type == RecordedGate::CZ)
    applyCZ(gate.qubits[0], gate.qubits[1]);
}

void QuantumRegister::applyRegisteredGateInverse(const RecordedGate &gate) {
  if (gate.type == RecordedGate::H)
    applyHadamard(gate.qubits[0]); // H is self-inverse
  else if (gate.type == RecordedGate::X)
    applyX(gate.qubits[0]); // X is self-inverse
  else if (gate.type == RecordedGate::Y)
    applyY(gate.qubits[0]); // Y is self-inverse
  else if (gate.type == RecordedGate::Z)
    applyZ(gate.qubits[0]); // Z is self-inverse
  else if (gate.type == RecordedGate::CNOT)
    applyCNOT(gate.qubits[0], gate.qubits[1]); // CNOT is self-inverse
  else if (gate.type == RecordedGate::RY)
    applyRotationY(gate.qubits[0], -gate.params[0]);
  else if (gate.type == RecordedGate::RZ)
    applyRotationZ(gate.qubits[0], -gate.params[0]);
  else if (gate.type == RecordedGate::RX)
    applyRotationX(gate.qubits[0], -gate.params[0]);
  else if (gate.type == RecordedGate::SWAP)
    applySWAP(gate.qubits[0], gate.qubits[1]); // SWAP is self-inverse
  else if (gate.type == RecordedGate::CZ)
    applyCZ(gate.qubits[0], gate.qubits[1]); // CZ is self-inverse
  else if (gate.type == RecordedGate::TOFFOLI)
    applyToffoli(gate.qubits[0], gate.qubits[1], gate.qubits[2]); // Toffoli is self-inverse
  else if (gate.type == RecordedGate::PHASE_S) {
    // S† = S^3 (apply S three times to get S-dagger)
    applyPhaseS(gate.qubits[0]);
    applyPhaseS(gate.qubits[0]);
    applyPhaseS(gate.qubits[0]);
  } else if (gate.type == RecordedGate::PHASE_T) {
    // T† = T^7 (apply T seven times to get T-dagger)
    for (int i = 0; i < 7; ++i)
      applyPhaseT(gate.qubits[0]);
  }
}

} // namespace qubit_engine
