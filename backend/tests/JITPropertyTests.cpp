// Property-based Tests for QuantumJIT
#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "../src/QuantumJIT.hpp"
#include <vector>
#include <string>

using namespace qubit_engine::jit;

// Generator for random Quantum Circuits
struct RandomCircuit {
    int num_qubits;
    std::vector<std::pair<std::string, std::vector<int>>> gates;
    std::vector<double> params;
};

namespace rc {
template <>
struct Arbitrary<RandomCircuit> {
    static Gen<RandomCircuit> arbitrary() {
        return gen::exec([] {
            RandomCircuit circ;
            circ.num_qubits = *gen::inRange(2, 10);
            int num_gates = *gen::inRange(1, 50);
            for (int i = 0; i < num_gates; ++i) {
                int gate_type = *gen::inRange(0, 4);
                int q1 = *gen::inRange(0, 10);
                int q2 = *gen::inRange(0, 10);
                if (gate_type == 0) circ.gates.push_back({"H", {q1}});
                else if (gate_type == 1) circ.gates.push_back({"X", {q1}});
                else if (gate_type == 2) circ.gates.push_back({"CNOT", {q1, q2}});
                else circ.gates.push_back({"RZ", {q1}});
            }
            int num_params = *gen::inRange(1, 50);
            for (int i = 0; i < num_params; ++i) {
                circ.params.push_back(*gen::inRange(-314, 314) / 100.0);
            }
            return circ;
        });
    }
};
} // namespace rc

TEST(JITPropertyTest, O0CompilationDoesNotAlterOriginalCircuitLength) {
    rc::check("O0 compilation preserves the exact number of gates",
              [](RandomCircuit circ) {
                  // Fix qubit indices
                  for (auto& g : circ.gates) {
                      for (auto& q : g.second) q = q % circ.num_qubits;
                      if (g.second.size() > 1 && g.second[0] == g.second[1]) {
                          g.second[1] = (g.second[1] + 1) % circ.num_qubits;
                      }
                  }

                  QuantumJIT jit(QuantumJIT::O0);
                  auto ir = jit.compile(circ.num_qubits, circ.gates, circ.params);

                  RC_ASSERT(ir.stats.original_gates == circ.gates.size());
                  RC_ASSERT(ir.stats.optimized_gates == circ.gates.size());
                  RC_ASSERT(ir.gates.size() == circ.gates.size());
              });
}

TEST(JITPropertyTest, O4OptimizationNeverIncreasesGateCount) {
    rc::check("O4 optimization always yields <= gates than O0",
              [](RandomCircuit circ) {
                  for (auto& g : circ.gates) {
                      for (auto& q : g.second) q = q % circ.num_qubits;
                      if (g.second.size() > 1 && g.second[0] == g.second[1]) {
                          g.second[1] = (g.second[1] + 1) % circ.num_qubits;
                      }
                  }

                  QuantumJIT jit_o4(QuantumJIT::O4);
                  auto ir_o4 = jit_o4.compile(circ.num_qubits, circ.gates, circ.params);

                  RC_ASSERT(ir_o4.stats.optimized_gates <= ir_o4.stats.original_gates);
                  RC_ASSERT(ir_o4.gates.size() <= circ.gates.size());
              });
}
