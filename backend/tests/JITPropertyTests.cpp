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
                else {
                    circ.gates.push_back({"RZ", {q1}});
                    circ.params.push_back(*gen::inRange(-314, 314) / 100.0);
                }
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

#include "../src/QuantumRegister.hpp"
#include "../src/backends/CpuBackend.hpp"

namespace {
void execute_ir(qubit_engine::QuantumRegister& qreg, const QuantumJIT::CircuitIR& ir) {
    for (const auto &g : ir.gates) {
        std::vector<size_t> targets;
        for (int t : g.target_qubits) targets.push_back(static_cast<size_t>(t));
        if (!g.fused_unitary.empty()) {
            qreg.applyDenseUnitary(targets, g.fused_unitary);
        } else if (g.type == qubit_engine::jit::CompiledGate::SINGLE_QUBIT) {
            std::vector<qubit_engine::Complex> matrix(g.single_matrix.begin(), g.single_matrix.end());
            qreg.applyDenseUnitary(targets, matrix);
        } else if (g.type == qubit_engine::jit::CompiledGate::TWO_QUBIT) {
            std::vector<qubit_engine::Complex> matrix(g.two_matrix.begin(), g.two_matrix.end());
            qreg.applyDenseUnitary(targets, matrix);
        }
    }
}
} // namespace

TEST(JITPropertyTest, O4OptimizationNeverIncreasesGateCount) {
    rc::check("O4 optimization always yields <= gates than O0 and preserves state fidelity",
              [](RandomCircuit circ) {
                  circ.num_qubits = 2;
                  for (auto& g : circ.gates) {
                      if (g.second.size() == 1) {
                          g.second[0] = g.second[0] % 2;
                      } else if (g.second.size() > 1) {
                          int qA = *rc::gen::inRange(0, 2);
                          g.second[0] = qA;
                          g.second[1] = 1 - qA;
                      }
                  }

                  QuantumJIT jit_o0(QuantumJIT::O0);
                  auto ir_o0 = jit_o0.compile(circ.num_qubits, circ.gates, circ.params);

                  QuantumJIT jit_o4(QuantumJIT::O4);
                  auto ir_o4 = jit_o4.compile(circ.num_qubits, circ.gates, circ.params);

                  RC_ASSERT(ir_o4.stats.optimized_gates <= ir_o4.stats.original_gates);
                  RC_ASSERT(ir_o4.gates.size() <= circ.gates.size());

                  // Execute both IRs on independent registers and compare fidelity
                  qubit_engine::QuantumRegister qreg_unopt(static_cast<size_t>(circ.num_qubits),
                      std::make_unique<qubit_engine::CpuBackend>(circ.num_qubits));
                  execute_ir(qreg_unopt, ir_o0);
                  auto psi_unopt = qreg_unopt.getStateVector();

                  qubit_engine::QuantumRegister qreg_opt(static_cast<size_t>(circ.num_qubits),
                      std::make_unique<qubit_engine::CpuBackend>(circ.num_qubits));
                  execute_ir(qreg_opt, ir_o4);
                  auto psi_opt = qreg_opt.getStateVector();

                  qubit_engine::Complex dot_product = 0.0;
                  for (size_t i = 0; i < psi_unopt.size(); ++i) {
                      dot_product += std::conj(psi_unopt[i]) * psi_opt[i];
                  }
                  double fidelity = std::norm(dot_product);

                  if (fidelity < 0.999) {
                      std::cout << "O4 FIDELITY MISMATCH: num_qubits=" << circ.num_qubits << " fidelity=" << fidelity << "\n";
                      for (size_t i = 0; i < circ.gates.size(); ++i) {
                          std::cout << "  " << circ.gates[i].first << " [";
                          for (int q : circ.gates[i].second) std::cout << q << " ";
                          std::cout << "]\n";
                      }
                      std::cout << "O4 GATES: count=" << ir_o4.gates.size() << "\n";
                      for (const auto& g : ir_o4.gates) {
                          std::cout << "  type=" << g.type << " targets=[";
                          for (int t : g.target_qubits) std::cout << t << " ";
                          std::cout << "]\n";
                      }
                  }

                  // State-vector fidelity between unoptimized (O0) and O4-optimized execution must be >= 1.0 - 1e-6
                  RC_ASSERT(fidelity >= 1.0 - 1e-6);
              });
}

TEST(JITPropertyTest, O2OptimizationPreservesFidelityAndDecreasesGateCount) {
    rc::check("O2 optimization always yields <= gates than O0 and preserves state fidelity across arbitrary circuits",
              [](RandomCircuit circ) {
                  for (auto& g : circ.gates) {
                      for (auto& q : g.second) q = q % circ.num_qubits;
                      if (g.second.size() > 1 && g.second[0] == g.second[1]) {
                          g.second[1] = (g.second[1] + 1) % circ.num_qubits;
                      }
                  }

                  QuantumJIT jit_o0(QuantumJIT::O0);
                  auto ir_o0 = jit_o0.compile(circ.num_qubits, circ.gates, circ.params);

                  QuantumJIT jit_o2(QuantumJIT::O2);
                  auto ir_o2 = jit_o2.compile(circ.num_qubits, circ.gates, circ.params);

                  RC_ASSERT(ir_o2.stats.optimized_gates <= ir_o2.stats.original_gates);
                  RC_ASSERT(ir_o2.gates.size() <= circ.gates.size());

                  // Execute both IRs on independent registers and compare fidelity
                  qubit_engine::QuantumRegister qreg_unopt(static_cast<size_t>(circ.num_qubits),
                      std::make_unique<qubit_engine::CpuBackend>(circ.num_qubits));
                  execute_ir(qreg_unopt, ir_o0);
                  auto psi_unopt = qreg_unopt.getStateVector();

                  qubit_engine::QuantumRegister qreg_opt(static_cast<size_t>(circ.num_qubits),
                      std::make_unique<qubit_engine::CpuBackend>(circ.num_qubits));
                  execute_ir(qreg_opt, ir_o2);
                  auto psi_opt = qreg_opt.getStateVector();

                  qubit_engine::Complex dot_product = 0.0;
                  for (size_t i = 0; i < psi_unopt.size(); ++i) {
                      dot_product += std::conj(psi_unopt[i]) * psi_opt[i];
                  }
                  double fidelity = std::norm(dot_product);

                  RC_ASSERT(fidelity >= 1.0 - 1e-6);
              });
}
