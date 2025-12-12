// Quantum JIT Compiler - Gate Fusion Optimization
// Fuses adjacent gates to reduce circuit depth and improve performance

#ifndef QUANTUM_JIT_HPP
#define QUANTUM_JIT_HPP

#include <cmath>
#include <complex>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace qubit_engine {
namespace jit {

// Gate Matrix (2x2 for single-qubit, 4x4 for two-qubit)
using Complex = std::complex<double>;
using Matrix2x2 = std::array<Complex, 4>;
using Matrix4x4 = std::array<Complex, 16>;

// Single-qubit gate matrices
const Matrix2x2 IDENTITY = {1, 0, 0, 1};
const Matrix2x2 PAULI_X = {0, 1, 1, 0};
const Matrix2x2 PAULI_Y = {0, Complex(0, -1), Complex(0, 1), 0};
const Matrix2x2 PAULI_Z = {1, 0, 0, -1};
const Matrix2x2 HADAMARD = {1 / std::sqrt(2), 1 / std::sqrt(2),
                            1 / std::sqrt(2), -1 / std::sqrt(2)};
const Matrix2x2 S_GATE = {1, 0, 0, Complex(0, 1)};
const Matrix2x2 T_GATE = {1, 0, 0, std::exp(Complex(0, M_PI / 4))};

// Compiled gate operation
struct CompiledGate {
  enum Type { SINGLE_QUBIT, TWO_QUBIT, FUSED_BLOCK };

  Type type;
  std::vector<int> target_qubits;
  Matrix2x2 single_matrix;
  Matrix4x4 two_matrix;

  // For fused blocks: combined unitary
  std::vector<Complex> fused_unitary;
  int fused_size;
};

// JIT Compiler for quantum circuits
class QuantumJIT {
public:
  struct OptimizationStats {
    int original_gates;
    int optimized_gates;
    int fused_blocks;
    double compilation_time_ms;
    double expected_speedup;
  };

  struct CircuitIR {
    int num_qubits;
    std::vector<CompiledGate> gates;
    OptimizationStats stats;
  };

  // Optimization levels
  enum OptLevel {
    O0 = 0, // No optimization
    O1 = 1, // Basic gate cancellation
    O2 = 2, // Gate fusion
    O3 = 3  // Aggressive fusion + reordering
  };

  QuantumJIT(OptLevel level = O2) : opt_level_(level) {}

  // Compile a circuit from gate list
  CircuitIR
  compile(int num_qubits,
          const std::vector<std::pair<std::string, std::vector<int>>> &gates,
          const std::vector<double> &params = {}) {
    CircuitIR ir;
    ir.num_qubits = num_qubits;
    ir.stats.original_gates = gates.size();

    auto start = std::chrono::high_resolution_clock::now();

    // Phase 1: Build initial gate list
    std::vector<CompiledGate> compiled;
    for (size_t i = 0; i < gates.size(); i++) {
      CompiledGate g = build_gate(gates[i].first, gates[i].second,
                                  i < params.size() ? params[i] : 0.0);
      compiled.push_back(g);
    }

    // Phase 2: Apply optimizations based on level
    if (opt_level_ >= O1) {
      compiled = cancel_adjacent_gates(compiled);
    }
    if (opt_level_ >= O2) {
      compiled = fuse_single_qubit_gates(compiled);
    }
    if (opt_level_ >= O3) {
      compiled = reorder_and_fuse(compiled, num_qubits);
    }

    ir.gates = compiled;
    ir.stats.optimized_gates = compiled.size();
    ir.stats.fused_blocks = count_fused_blocks(compiled);

    auto end = std::chrono::high_resolution_clock::now();
    ir.stats.compilation_time_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
    ir.stats.expected_speedup =
        static_cast<double>(ir.stats.original_gates) / ir.stats.optimized_gates;

    return ir;
  }

private:
  OptLevel opt_level_;

  // Build a single gate
  CompiledGate build_gate(const std::string &name,
                          const std::vector<int> &qubits, double param) {
    CompiledGate g;
    g.target_qubits = qubits;

    if (qubits.size() == 1) {
      g.type = CompiledGate::SINGLE_QUBIT;
      if (name == "H")
        g.single_matrix = HADAMARD;
      else if (name == "X")
        g.single_matrix = PAULI_X;
      else if (name == "Y")
        g.single_matrix = PAULI_Y;
      else if (name == "Z")
        g.single_matrix = PAULI_Z;
      else if (name == "S")
        g.single_matrix = S_GATE;
      else if (name == "T")
        g.single_matrix = T_GATE;
      else if (name == "RZ")
        g.single_matrix = rz_matrix(param);
      else if (name == "RX")
        g.single_matrix = rx_matrix(param);
      else if (name == "RY")
        g.single_matrix = ry_matrix(param);
      else
        g.single_matrix = IDENTITY;
    } else if (qubits.size() == 2) {
      g.type = CompiledGate::TWO_QUBIT;
      if (name == "CNOT" || name == "CX") {
        g.two_matrix = cnot_matrix();
      } else if (name == "CZ") {
        g.two_matrix = cz_matrix();
      } else if (name == "SWAP") {
        g.two_matrix = swap_matrix();
      }
    }

    return g;
  }

  // Rotation gate matrices
  Matrix2x2 rz_matrix(double theta) {
    return {std::exp(Complex(0, -theta / 2)), 0, 0,
            std::exp(Complex(0, theta / 2))};
  }

  Matrix2x2 rx_matrix(double theta) {
    double c = std::cos(theta / 2);
    double s = std::sin(theta / 2);
    return {c, Complex(0, -s), Complex(0, -s), c};
  }

  Matrix2x2 ry_matrix(double theta) {
    double c = std::cos(theta / 2);
    double s = std::sin(theta / 2);
    return {c, -s, s, c};
  }

  // Two-qubit gate matrices
  Matrix4x4 cnot_matrix() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0};
  }

  Matrix4x4 cz_matrix() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1};
  }

  Matrix4x4 swap_matrix() {
    return {1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1};
  }

  // Matrix multiplication for 2x2
  Matrix2x2 matmul2x2(const Matrix2x2 &a, const Matrix2x2 &b) {
    return {a[0] * b[0] + a[1] * b[2], a[0] * b[1] + a[1] * b[3],
            a[2] * b[0] + a[3] * b[2], a[2] * b[1] + a[3] * b[3]};
  }

  // Check if matrix is identity
  bool is_identity(const Matrix2x2 &m, double tol = 1e-10) {
    return std::abs(m[0] - 1.0) < tol && std::abs(m[1]) < tol &&
           std::abs(m[2]) < tol && std::abs(m[3] - 1.0) < tol;
  }

  // O1: Cancel adjacent inverse gates (X·X = I, H·H = I, etc.)
  std::vector<CompiledGate>
  cancel_adjacent_gates(const std::vector<CompiledGate> &gates) {
    std::vector<CompiledGate> result;

    for (size_t i = 0; i < gates.size(); i++) {
      if (i + 1 < gates.size() && gates[i].type == CompiledGate::SINGLE_QUBIT &&
          gates[i + 1].type == CompiledGate::SINGLE_QUBIT &&
          gates[i].target_qubits == gates[i + 1].target_qubits) {

        // Check if they cancel (product is identity)
        Matrix2x2 product =
            matmul2x2(gates[i + 1].single_matrix, gates[i].single_matrix);
        if (is_identity(product)) {
          i++; // Skip both gates
          continue;
        }
      }
      result.push_back(gates[i]);
    }

    return result;
  }

  // O2: Fuse consecutive single-qubit gates on same qubit
  std::vector<CompiledGate>
  fuse_single_qubit_gates(const std::vector<CompiledGate> &gates) {
    std::vector<CompiledGate> result;
    std::unordered_map<int, CompiledGate> pending;

    for (const auto &g : gates) {
      if (g.type == CompiledGate::SINGLE_QUBIT) {
        int q = g.target_qubits[0];
        if (pending.count(q)) {
          // Fuse with pending gate
          pending[q].single_matrix =
              matmul2x2(g.single_matrix, pending[q].single_matrix);
        } else {
          pending[q] = g;
        }
      } else {
        // Flush pending gates for any qubits this two-qubit gate touches
        for (int q : g.target_qubits) {
          if (pending.count(q)) {
            result.push_back(pending[q]);
            pending.erase(q);
          }
        }
        result.push_back(g);
      }
    }

    // Flush remaining pending gates
    for (auto &[q, gate] : pending) {
      result.push_back(gate);
    }

    return result;
  }

  // O3: Reorder gates when possible and fuse more aggressively
  std::vector<CompiledGate>
  reorder_and_fuse(const std::vector<CompiledGate> &gates, int num_qubits) {
    // Simplified: just do another pass of fusion
    // Real implementation would use dependency graph
    return fuse_single_qubit_gates(gates);
  }

  int count_fused_blocks(const std::vector<CompiledGate> &gates) {
    int count = 0;
    for (const auto &g : gates) {
      if (g.type == CompiledGate::FUSED_BLOCK)
        count++;
    }
    return count;
  }
};

} // namespace jit
} // namespace qubit_engine

#endif // QUANTUM_JIT_HPP
