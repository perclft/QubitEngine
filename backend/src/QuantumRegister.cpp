#include "QuantumRegister.hpp"
#include "backends/CpuBackend.hpp"
#include "backends/MetalBackend.hpp" // Added for MetalBackend
#include <fstream>
#include <iostream>

// --- Lifecycle ---
QuantumRegister::QuantumRegister(size_t n, bool force_local) : num_qubits(n) {
  // Factory Logic
#ifdef __APPLE__
  // Check if Metal shaders are available before using Metal backend
  bool metalAvailable = false;
  if (!force_local) {
    // Check for metallib file in current directory or common locations
    std::ifstream metallib("default.metallib");
    if (metallib.good()) {
      metalAvailable = true;
    }
  }

  if (metalAvailable) {
    try {
      backend = std::make_unique<qubit_engine::MetalBackend>(n);
      std::cout << "QuantumRegister: Using MetalBackend (GPU)" << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "QuantumRegister: MetalBackend failed to initialize ("
                << e.what() << "). Falling back to CpuBackend." << std::endl;
      backend = std::make_unique<qubit_engine::CpuBackend>(n, force_local);
    }
  } else {
    backend = std::make_unique<qubit_engine::CpuBackend>(n, force_local);
    std::cout
        << "QuantumRegister: Using CpuBackend (Metal shaders not available)"
        << std::endl;
  }
#else
  // Default to CPU on Linux/Windows for now
  backend = std::make_unique<qubit_engine::CpuBackend>(n, force_local);
#endif
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
  if (recording_enabled)
    tape.push_back({RecordedGate::CNOT, {control, target}, {}});
  backend->applyCNOT(control, target);
}

// --- Advanced Gates ---

void QuantumRegister::applyToffoli(size_t c1, size_t c2, size_t t) {
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

void QuantumRegister::applyRotationY(size_t target,
                                     qubit_engine::Precision angle) {
  if (recording_enabled)
    tape.push_back({RecordedGate::RY, {target}, {(double)angle}});
  backend->applyRotationY(target, angle);
}

void QuantumRegister::applyRotationZ(size_t target,
                                     qubit_engine::Precision angle) {
  if (recording_enabled)
    tape.push_back({RecordedGate::RZ, {target}, {(double)angle}});
  backend->applyRotationZ(target, angle);
}

// --- Noise ---

void QuantumRegister::applyDepolarizingNoise(
    qubit_engine::Precision probability) {
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
std::vector<qubit_engine::Complex> QuantumRegister::getStateVector() const {
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
  // Add others if needed
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
}
