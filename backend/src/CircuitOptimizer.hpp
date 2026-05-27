#pragma once

#include "HardwareConfig.hpp"
#include "QuantumRegister.hpp"
#include <vector>

namespace qubit_engine {

class CircuitOptimizer {
public:
  // Optimizes the given tape of gates in-place
  static void optimize(std::vector<QuantumRegister::RecordedGate> &tape);

  // Maps a logical circuit to an arbitrary hardware topology by inserting SWAPs
  static void mapToTopology(std::vector<QuantumRegister::RecordedGate> &tape, const HardwareConfig& config);

  // Transpiles non-Clifford gates on the tape to Clifford equivalents
  static void transpileToClifford(std::vector<QuantumRegister::RecordedGate> &tape,
                                  bool approximate = false,
                                  bool use_stochastic = false);

private:
  // Helper to check if two gates are inverses of each other
  static bool areInverses(const QuantumRegister::RecordedGate &g1,
                          const QuantumRegister::RecordedGate &g2);
};

} // namespace qubit_engine
