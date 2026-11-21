#include <iostream>
#include <vector>
#include <complex>
#include "../backend/src/QuantumJIT.hpp"
#include "../backend/src/QuantumRegister.hpp"
#include "../backend/src/backends/CpuBackend.hpp"

using namespace qubit_engine;
using namespace qubit_engine::jit;

int main() {
    std::cout << "=== Minimal Reproduction of NEW-09 (O4 KAK Control > Target) ===\n";
    
    // Circuit: X(1) followed by CNOT(control=1, target=0)
    // Expected output state: |11> (index 3)
    std::vector<std::pair<std::string, std::vector<int>>> gates = {
        {"X", {1}},
        {"CNOT", {1, 0}}
    };
    std::vector<double> params;
    
    // Unoptimized O0
    QuantumJIT jit_o0(QuantumJIT::O0);
    auto ir_o0 = jit_o0.compile(2, gates, params);
    
    // O4 Optimized
    QuantumJIT jit_o4(QuantumJIT::O4);
    auto ir_o4 = jit_o4.compile(2, gates, params);
    
    std::cout << "O0 gates count: " << ir_o0.gates.size() << "\n";
    for (const auto& g : ir_o0.gates) {
        std::cout << "  O0 gate targets: [";
        for (int t : g.target_qubits) std::cout << t << " ";
        std::cout << "]\n";
    }
    
    std::cout << "O4 gates count: " << ir_o4.gates.size() << "\n";
    for (const auto& g : ir_o4.gates) {
        std::cout << "  O4 gate targets: [";
        for (int t : g.target_qubits) std::cout << t << " ";
        std::cout << "] type=" << g.type << "\n";
    }
    
    return 0;
}
