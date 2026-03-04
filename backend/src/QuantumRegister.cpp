#include "QuantumRegister.hpp"
#include "CircuitOptimizer.hpp"
#include "backends/CpuBackend.hpp"
#include "backends/CudaBackend.hpp"
#include "backends/MPSBackend.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace qubit_engine {

// --- Lifecycle ---
QuantumRegister::QuantumRegister(size_t n, bool force_local) : num_qubits(n) {
  // Phase 4: Tensor Network Acceleration
  if (n >= 25 && !force_local) {
    // Emulate using MPS for circuits >= 25 qubits to showcase memory
    // compression
    backend = std::make_unique<MPSBackend>(n);
    return;
  }

  // Factory Logic
#ifdef ENABLE_CUDA
  if (!force_local) {
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
  backend = std::make_unique<CpuBackend>(n, force_local);
}

QuantumRegister::~QuantumRegister() {}

// --- Core Gates ---

void QuantumRegister::applyHadamard(size_t target) {
  if (recording_enabled)
    tape.push_back({RecordedGate::H, {target}, {}});
  backend->applyHadamard(target);
}

void QuantumRegister::applyX(size_t target) {
  if (recording_enabled)
    tape.push_back({RecordedGate::X, {target}, {}});
  backend->applyX(target);
}

void QuantumRegister::applyY(size_t target) {
  if (recording_enabled)
    tape.push_back({RecordedGate::Y, {target}, {}});
  backend->applyY(target);
}

void QuantumRegister::applyZ(size_t target) {
  if (recording_enabled)
    tape.push_back({RecordedGate::Z, {target}, {}});
  backend->applyZ(target);
}

void QuantumRegister::applyCNOT(size_t control, size_t target) {
  if (control == target)
    throw std::invalid_argument("Control and target qubits must be distinct");
  if (recording_enabled)
    tape.push_back({RecordedGate::CNOT, {control, target}, {}});
  backend->applyCNOT(control, target);
}

// --- Advanced Gates ---

void QuantumRegister::applyToffoli(size_t c1, size_t c2, size_t t) {
  if (c1 == c2 || c1 == t || c2 == t)
    throw std::invalid_argument("Control and target qubits must be distinct");
  if (recording_enabled)
    tape.push_back({RecordedGate::TOFFOLI, {c1, c2, t}, {}});
  backend->applyToffoli(c1, c2, t);
}

void QuantumRegister::applyPhaseS(size_t target) {
  if (recording_enabled)
    tape.push_back({RecordedGate::PHASE_S, {target}, {}});
  backend->applyPhaseS(target);
}

void QuantumRegister::applyPhaseT(size_t target) {
  if (recording_enabled)
    tape.push_back({RecordedGate::PHASE_T, {target}, {}});
  backend->applyPhaseT(target);
}

void QuantumRegister::applyRotationY(size_t target, Precision angle) {
  if (recording_enabled)
    tape.push_back({RecordedGate::RY, {target}, {(double)angle}});
  backend->applyRotationY(target, angle);
}

void QuantumRegister::applyRotationZ(size_t target, Precision angle) {
  if (recording_enabled)
    tape.push_back({RecordedGate::RZ, {target}, {(double)angle}});
  backend->applyRotationZ(target, angle);
}

void QuantumRegister::applyRotationX(size_t target, Precision angle) {
  if (recording_enabled)
    tape.push_back({RecordedGate::RX, {target}, {(double)angle}});
  backend->applyRotationX(target, angle);
}

void QuantumRegister::applySWAP(size_t qubit1, size_t qubit2) {
  if (qubit1 == qubit2)
    throw std::invalid_argument("Qubits must be distinct");
  if (recording_enabled)
    tape.push_back({RecordedGate::SWAP, {qubit1, qubit2}, {}});
  backend->applySWAP(qubit1, qubit2);
}

void QuantumRegister::applyCZ(size_t control, size_t target) {
  if (control == target)
    throw std::invalid_argument("Control and target qubits must be distinct");
  if (recording_enabled)
    tape.push_back({RecordedGate::CZ, {control, target}, {}});
  backend->applyCZ(control, target);
}

// --- Noise ---

void QuantumRegister::applyDepolarizingNoise(Precision probability) {
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
void QuantumRegister::clearTape() { tape.clear(); }
const std::vector<QuantumRegister::RecordedGate> &
QuantumRegister::getTape() const {
  return tape;
}

void QuantumRegister::optimize() { CircuitOptimizer::optimize(tape); }

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
    applyRotationY(gate.qubits[0], -gate.params[0]);
  else if (gate.type == RecordedGate::RZ)
    applyRotationZ(gate.qubits[0], -gate.params[0]);
  else if (gate.type == RecordedGate::RX)
    applyRotationX(gate.qubits[0], -gate.params[0]);
  else if (gate.type == RecordedGate::SWAP)
    applySWAP(gate.qubits[0], gate.qubits[1]);
  else if (gate.type == RecordedGate::CZ)
    applyCZ(gate.qubits[0], gate.qubits[1]);
}

} // namespace qubit_engine
