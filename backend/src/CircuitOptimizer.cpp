#include "CircuitOptimizer.hpp"
#include "Exceptions.hpp"
#include "transpiler/Router.hpp"
#include <spdlog/spdlog.h>
#include <random>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

void CircuitOptimizer::transpileToClifford(
    std::vector<QuantumRegister::RecordedGate> &tape,
    bool approximate,
    bool use_stochastic) {
  
  std::vector<QuantumRegister::RecordedGate> transpiled;
  transpiled.reserve(tape.size());

  // Setup random generator for stochastic snapping
  std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<double> dis(0.0, 1.0);

  // Helper lambda to check if a gate is Clifford
  auto is_clifford = [](QuantumRegister::RecordedGate::Type type) {
    switch (type) {
      case QuantumRegister::RecordedGate::H:
      case QuantumRegister::RecordedGate::X:
      case QuantumRegister::RecordedGate::Y:
      case QuantumRegister::RecordedGate::Z:
      case QuantumRegister::RecordedGate::CNOT:
      case QuantumRegister::RecordedGate::PHASE_S:
      case QuantumRegister::RecordedGate::CZ:
      case QuantumRegister::RecordedGate::SWAP:
      case QuantumRegister::RecordedGate::MEASURE:
        return true;
      default:
        return false;
    }
  };

  // Helper to snap an angle to the nearest multiple of pi/2
  auto snap_angle = [&](double theta, bool &was_halfway) -> int {
    // Normalize theta to [0, 2*pi)
    double two_pi = 2.0 * M_PI;
    double normalized = std::fmod(theta, two_pi);
    if (normalized < 0.0) {
      normalized += two_pi;
    }

    // Divide by pi/2 to get number of steps
    double steps = normalized / (M_PI / 2.0);
    double fract = steps - std::floor(steps);
    
    // Check if it's exactly halfway (e.g. within 1e-9 of 0.5)
    was_halfway = (std::abs(fract - 0.5) < 1e-9);

    int rounded_steps;
    if (was_halfway) {
      if (use_stochastic) {
        rounded_steps = (dis(gen) < 0.5) ? static_cast<int>(std::floor(steps)) : static_cast<int>(std::ceil(steps));
      } else {
        // Deterministic snapping: round-half-to-even (Bankers' rounding) to avoid phase drift
        int floor_steps = static_cast<int>(std::floor(steps));
        if (floor_steps % 2 == 0) {
          rounded_steps = floor_steps;
        } else {
          rounded_steps = floor_steps + 1;
        }
      }
    } else {
      rounded_steps = static_cast<int>(std::round(steps));
    }

    return rounded_steps % 4;
  };

  // Helper to map RZ snapped steps to Clifford gates
  auto append_rz_clifford = [&](size_t target, int steps) {
    switch (steps) {
      case 1: // pi/2 -> S gate
        transpiled.push_back({QuantumRegister::RecordedGate::PHASE_S, {target}, {}});
        break;
      case 2: // pi -> Z gate
        transpiled.push_back({QuantumRegister::RecordedGate::Z, {target}, {}});
        break;
      case 3: // 3*pi/2 = -pi/2 -> S† (implemented as Z * S)
        transpiled.push_back({QuantumRegister::RecordedGate::Z, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::PHASE_S, {target}, {}});
        break;
      default: // 0 -> I (no gate)
        break;
    }
  };

  // Helper to map RX snapped steps to Clifford gates
  auto append_rx_clifford = [&](size_t target, int steps) {
    switch (steps) {
      case 1: // pi/2 -> H * S * H
        transpiled.push_back({QuantumRegister::RecordedGate::H, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::PHASE_S, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::H, {target}, {}});
        break;
      case 2: // pi -> X gate
        transpiled.push_back({QuantumRegister::RecordedGate::X, {target}, {}});
        break;
      case 3: // -pi/2 -> H * S† * H = H * Z * S * H
        transpiled.push_back({QuantumRegister::RecordedGate::H, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::Z, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::PHASE_S, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::H, {target}, {}});
        break;
      default: // 0 -> I
        break;
    }
  };

  // Helper to map RY snapped steps to Clifford gates
  auto append_ry_clifford = [&](size_t target, int steps) {
    switch (steps) {
      case 1: // pi/2 -> S * H * S
        transpiled.push_back({QuantumRegister::RecordedGate::PHASE_S, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::H, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::PHASE_S, {target}, {}});
        break;
      case 2: // pi -> Y gate
        transpiled.push_back({QuantumRegister::RecordedGate::Y, {target}, {}});
        break;
      case 3: // -pi/2 -> S† * H * S† = Z * S * H * Z * S
        transpiled.push_back({QuantumRegister::RecordedGate::Z, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::PHASE_S, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::H, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::Z, {target}, {}});
        transpiled.push_back({QuantumRegister::RecordedGate::PHASE_S, {target}, {}});
        break;
      default:
        break;
    }
  };

  for (const auto &gate : tape) {
    if (is_clifford(gate.type)) {
      transpiled.push_back(gate);
      continue;
    }

    if (!approximate) {
      throw NonCliffordGateException("Non-Clifford gate encountered during strict validation: type=" + std::to_string(gate.type));
    }

    // Approximation mode
    switch (gate.type) {
      case QuantumRegister::RecordedGate::PHASE_T: {
        // T gate is RZ(pi/4)
        bool halfway = false;
        int steps = snap_angle(M_PI / 4.0, halfway);
        append_rz_clifford(gate.qubits[0], steps);
        break;
      }
      case QuantumRegister::RecordedGate::RZ: {
        bool halfway = false;
        int steps = snap_angle(gate.params[0], halfway);
        append_rz_clifford(gate.qubits[0], steps);
        break;
      }
      case QuantumRegister::RecordedGate::RX: {
        bool halfway = false;
        int steps = snap_angle(gate.params[0], halfway);
        append_rx_clifford(gate.qubits[0], steps);
        break;
      }
      case QuantumRegister::RecordedGate::RY: {
        bool halfway = false;
        int steps = snap_angle(gate.params[0], halfway);
        append_ry_clifford(gate.qubits[0], steps);
        break;
      }
      case QuantumRegister::RecordedGate::TOFFOLI: {
        // Decompose Toffoli(c1, c2, t) into standard 15-gate sequence using Clifford+T
        size_t c1 = gate.qubits[0];
        size_t c2 = gate.qubits[1];
        size_t t = gate.qubits[2];

        std::vector<QuantumRegister::RecordedGate> toffoli_tape = {
            {QuantumRegister::RecordedGate::H, {t}, {}},
            {QuantumRegister::RecordedGate::CNOT, {c2, t}, {}},
            {QuantumRegister::RecordedGate::RZ, {t}, {-M_PI / 4.0}}, // T†
            {QuantumRegister::RecordedGate::CNOT, {c1, t}, {}},
            {QuantumRegister::RecordedGate::RZ, {t}, {M_PI / 4.0}},  // T
            {QuantumRegister::RecordedGate::CNOT, {c2, t}, {}},
            {QuantumRegister::RecordedGate::RZ, {t}, {-M_PI / 4.0}}, // T†
            {QuantumRegister::RecordedGate::CNOT, {c1, t}, {}},
            {QuantumRegister::RecordedGate::RZ, {c2}, {M_PI / 4.0}},  // T
            {QuantumRegister::RecordedGate::RZ, {t}, {M_PI / 4.0}},   // T
            {QuantumRegister::RecordedGate::H, {t}, {}},
            {QuantumRegister::RecordedGate::CNOT, {c1, c2}, {}},
            {QuantumRegister::RecordedGate::RZ, {c1}, {M_PI / 4.0}},  // T
            {QuantumRegister::RecordedGate::RZ, {c2}, {-M_PI / 4.0}}, // T†
            {QuantumRegister::RecordedGate::CNOT, {c1, c2}, {}}
        };

        // Recursively call transpileToClifford on the toffoli sequence and append to transpiled
        transpileToClifford(toffoli_tape, true, use_stochastic);
        transpiled.insert(transpiled.end(), toffoli_tape.begin(), toffoli_tape.end());
        break;
      }
      default:
        throw NonCliffordGateException("Cannot transpile unsupported non-Clifford gate type: " + std::to_string(gate.type));
    }
  }

  tape = std::move(transpiled);
}

} // namespace qubit_engine
