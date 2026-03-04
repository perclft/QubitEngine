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

void CircuitOptimizer::mapTo1DTopology(
    std::vector<QuantumRegister::RecordedGate> &tape) {
  if (tape.empty())
    return;

  std::vector<QuantumRegister::RecordedGate> mappedTape;
  // Heuristic buffer sizing
  mappedTape.reserve(tape.size() * 2);

  for (const auto &gate : tape) {
    if (gate.qubits.size() == 2) {
      int q1 = gate.qubits[0];
      int q2 = gate.qubits[1];

      if (std::abs(q1 - q2) > 1) {
        // Need SWAP routing
        int distance = std::abs(q1 - q2);
        int direction = (q2 > q1) ? 1 : -1;

        std::vector<QuantumRegister::RecordedGate> swaps;
        int current_pos = q1;

        // Route q1 towards q2 until they are adjacent
        for (int i = 0; i < distance - 1; ++i) {
          QuantumRegister::RecordedGate swapGate;
          swapGate.type = QuantumRegister::RecordedGate::SWAP;
          swapGate.qubits = {static_cast<size_t>(current_pos),
                             static_cast<size_t>(current_pos + direction)};
          mappedTape.push_back(swapGate);
          swaps.push_back(swapGate);
          current_pos += direction;
        }

        // Apply original gate on the adjacent positions
        QuantumRegister::RecordedGate adjacentGate = gate;
        adjacentGate.qubits[0] = static_cast<size_t>(current_pos);
        mappedTape.push_back(adjacentGate);

        // Reverse the SWAPs to restore logical state
        for (auto it = swaps.rbegin(); it != swaps.rend(); ++it) {
          mappedTape.push_back(*it);
        }
      } else {
        mappedTape.push_back(gate);
      }
    } else if (gate.qubits.size() == 3) {
      // TOFFOLI or similar 3-qubit gates need decomposition or more complex
      // routing. Not natively supported in strict 1D MPS prototype without
      // decomposition.
      mappedTape.push_back(gate);
    } else {
      mappedTape.push_back(gate);
    }
  }

  std::cout << "CircuitOptimizer: Mapped 1D topology inserted "
            << (mappedTape.size() - tape.size()) << " SWAP gates." << std::endl;
  tape = std::move(mappedTape);
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
    case QuantumRegister::RecordedGate::SWAP:
    case QuantumRegister::RecordedGate::CZ:
      return true;
    default:
      break;
    }
  }

  // Future: Check PhaseS + PhaseS dagger, etc.

  return false;
}

} // namespace qubit_engine
