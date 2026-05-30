#include "Router.hpp"
#include "../QuantumRegister.hpp"
#include "../Exceptions.hpp"
#include <queue>
#include <algorithm>
#include <stdexcept>

namespace qubit_engine {
namespace transpiler {

Router::Router(const HardwareConfig& config) {
    int max_id = -1;
    for (const auto& node : config.getNodes()) {
        if (node.id > max_id) max_id = node.id;
    }
    num_qubits_ = max_id + 1;
    adjacency_list_.resize(num_qubits_);

    for (const auto& edge : config.getEdges()) {
        adjacency_list_[edge.node1].push_back(edge.node2);
        adjacency_list_[edge.node2].push_back(edge.node1);
    }
}

std::vector<int> Router::findShortestPath(int src, int dst) const {
    if (src == dst) return {src};
    if (src >= num_qubits_ || dst >= num_qubits_) {
        throw std::invalid_argument("Qubit ID out of bounds in Router.");
    }

    std::vector<int> parent(num_qubits_, -1);
    std::queue<int> q;
    std::vector<bool> visited(num_qubits_, false);

    q.push(src);
    visited[src] = true;

    while (!q.empty()) {
        int u = q.front();
        q.pop();

        if (u == dst) break;

        for (int v : adjacency_list_[u]) {
            if (!visited[v]) {
                visited[v] = true;
                parent[v] = u;
                q.push(v);
            }
        }
    }

    if (parent[dst] == -1) {
        throw InvalidArgumentException("Disconnected hardware topology. Cannot route.");
    }

    std::vector<int> path;
    for (int curr = dst; curr != -1; curr = parent[curr]) {
        path.push_back(curr);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<QuantumRegister::RecordedGate> Router::route(const std::vector<QuantumRegister::RecordedGate>& tape) {
    std::vector<QuantumRegister::RecordedGate> routed_tape;
    
    // We maintain a logical-to-physical mapping.
    // For a simple SABRE/A* approximation, we will dynamically inject SWAPs 
    // to bring qubits together, which alters the physical locations.
    std::vector<int> logical_to_physical(num_qubits_);
    std::vector<int> physical_to_logical(num_qubits_);
    for (int i = 0; i < num_qubits_; ++i) {
        logical_to_physical[i] = i;
        physical_to_logical[i] = i;
    }

    auto applySwap = [&](int p1, int p2) {
        // Record physical SWAP
        routed_tape.push_back({QuantumRegister::RecordedGate::SWAP, {(size_t)p1, (size_t)p2}, {}});
        
        // Update mappings
        int l1 = physical_to_logical[p1];
        int l2 = physical_to_logical[p2];
        logical_to_physical[l1] = p2;
        logical_to_physical[l2] = p1;
        physical_to_logical[p1] = l2;
        physical_to_logical[p2] = l1;
    };

    for (const auto& gate : tape) {
        if (gate.type == QuantumRegister::RecordedGate::CNOT || 
            gate.type == QuantumRegister::RecordedGate::CZ || 
            gate.type == QuantumRegister::RecordedGate::SWAP) {
            
            int p_control = logical_to_physical[gate.qubits[0]];
            int p_target = logical_to_physical[gate.qubits[1]];

            // Check if adjacent
            bool adjacent = false;
            for (int neighbor : adjacency_list_[p_control]) {
                if (neighbor == p_target) {
                    adjacent = true;
                    break;
                }
            }

            if (!adjacent) {
                // Find shortest path between p_control and p_target
                std::vector<int> path = findShortestPath(p_control, p_target);
                
                // Route target towards control (leaving it adjacent)
                // path = [p_control, n1, n2, ..., p_target]
                for (size_t i = path.size() - 1; i > 1; --i) {
                    applySwap(path[i], path[i - 1]);
                }
                
                // Now they are adjacent. Update the physical locations for the gate
                p_control = logical_to_physical[gate.qubits[0]];
                p_target = logical_to_physical[gate.qubits[1]];
            }

            QuantumRegister::RecordedGate routed_gate = gate;
            routed_gate.qubits[0] = p_control;
            routed_gate.qubits[1] = p_target;
            routed_tape.push_back(routed_gate);

        } else if (gate.type == QuantumRegister::RecordedGate::TOFFOLI) {
            // Toffoli requires decomposing or complex routing. For now, route controls and target
            throw std::runtime_error("Routing Toffoli natively not supported. Decompose first.");
        } else {
            // 1Q Gate
            QuantumRegister::RecordedGate routed_gate = gate;
            routed_gate.qubits[0] = logical_to_physical[gate.qubits[0]];
            routed_tape.push_back(routed_gate);
        }
    }

    return routed_tape;
}

} // namespace transpiler
} // namespace qubit_engine
