#pragma once

#include "QuantumRegister.hpp"
#include <vector>

namespace qubit_engine {

class CircuitOptimizer {
public:
  // Optimizes the given tape of gates in-place
  static void optimize(std::vector<QuantumRegister::RecordedGate> &tape);

private:
  // Helper to check if two gates are inverses of each other
  static bool areInverses(const QuantumRegister::RecordedGate &g1,
                          const QuantumRegister::RecordedGate &g2);
};

} // namespace qubit_engine
