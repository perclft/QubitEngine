#include "CircuitOptimizer.hpp"
#include <iostream>

namespace qubit_engine {

void CircuitOptimizer::optimize(
    std::vector<QuantumRegister::RecordedGate> &tape) {
  if (tape.empty())
    return;

  std::vector<QuantumRegister::RecordedGate> optimizedTape;
  optimizedTape.reserve(tape.size());

  // Simple Peephole Optimizer: Cancellation of adjacent inverse gates
  // e.g., H - H -> I
  //       X - X -> I

  for (const auto &gate : tape) {
    if (optimizedTape.empty()) {
      optimizedTape.push_back(gate);
    } else {
      const auto &lastGate = optimizedTape.back();
      if (areInverses(lastGate, gate)) {
        // Cancel them out by removing the last one and not adding the current
        // one
        optimizedTape.pop_back();
      } else {
        optimizedTape.push_back(gate);
      }
    }
  }

  std::cout << "CircuitOptimizer: Reduced " << tape.size() << " gates to "
            << optimizedTape.size() << std::endl;
  tape = std::move(optimizedTape);
}

bool CircuitOptimizer::areInverses(const QuantumRegister::RecordedGate &g1,
                                   const QuantumRegister::RecordedGate &g2) {
  // Check if acting on same qubits
  if (g1.qubits != g2.qubits)
    return false;

  // Clifford Self-Inverses
  if (g1.type == g2.type) {
    switch (g1.type) {
    case QuantumRegister::RecordedGate::H:
    case QuantumRegister::RecordedGate::X:
    case QuantumRegister::RecordedGate::Y:
    case QuantumRegister::RecordedGate::Z:
    case QuantumRegister::RecordedGate::CNOT:
    case QuantumRegister::RecordedGate::TOFFOLI:
      return true;
    default:
      break;
    }
  }

  // Future: Check PhaseS + PhaseS dagger, etc.

  return false;
}

} // namespace qubit_engine
