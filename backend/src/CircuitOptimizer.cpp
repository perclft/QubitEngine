#include "CircuitOptimizer.hpp"
#include "transpiler/Router.hpp"
#include <spdlog/spdlog.h>

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

  spdlog::info("CircuitOptimizer: Reduced {} gates to {}", tape.size(), optimizedTape.size());
  // Maps a logical circuit to an arbitrary hardware topology by inserting SWAPs
  tape = std::move(optimizedTape);
}

void CircuitOptimizer::mapToTopology(
    std::vector<QuantumRegister::RecordedGate> &tape, const HardwareConfig& config) {
  if (tape.empty())
    return;

  transpiler::Router router(config);
  
  auto routed_tape = router.route(tape);
  spdlog::info("CircuitOptimizer: Mapped topology inserted {} SWAP gates.", (routed_tape.size() - tape.size()));
  tape = std::move(routed_tape);
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
