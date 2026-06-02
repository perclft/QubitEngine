#pragma once

#include "../HardwareConfig.hpp"
#include "../QuantumRegister.hpp"
#include "../Types.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace qubit_engine {
namespace transpiler {

class Router {
public:
    explicit Router(const HardwareConfig& config);

    // Routes a circuit given as a tape of RecordedGates.
    // Injects SWAP gates so that all 2Q gates (CNOT, CZ, SWAP) operate on adjacent qubits.
    std::vector<QuantumRegister::RecordedGate> route(const std::vector<QuantumRegister::RecordedGate>& tape);

private:
    const HardwareConfig& config_;
    int num_qubits_;
    std::vector<std::vector<int>> adjacency_list_;

    // Finds the path between src and dst maximizing fidelity using Dijkstra
    std::vector<int> findMaximumFidelityPath(int src, int dst) const;
};

} // namespace transpiler
} // namespace qubit_engine
