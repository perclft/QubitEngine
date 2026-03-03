// Quantum JIT Compiler - Gate Fusion Optimization
// Fuses adjacent gates to reduce circuit depth and improve performance

#ifndef QUANTUM_JIT_HPP
#define QUANTUM_JIT_HPP

#include <array>
#include <chrono>
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <complex>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Types.hpp"

namespace qubit_engine {
namespace jit {

// Gate Matrix (2x2 for single-qubit, 4x4 for two-qubit)
using Complex = qubit_engine::Complex;
using AlignedComplex = qubit_engine::AlignedComplex;
using Matrix2x2 = std::array<AlignedComplex, 4>;
using Matrix4x4 = std::array<AlignedComplex, 16>;

// Single-qubit gate matrices
extern const Matrix2x2 IDENTITY;
extern const Matrix2x2 PAULI_X;
extern const Matrix2x2 PAULI_Y;
extern const Matrix2x2 PAULI_Z;
extern const Matrix2x2 HADAMARD;
extern const Matrix2x2 S_GATE;
extern const Matrix2x2 T_GATE;

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
          const std::vector<double> &params = {});

private:
  OptLevel opt_level_;

  CompiledGate build_gate(const std::string &name,
                          const std::vector<int> &qubits, double param);

  Matrix2x2 rz_matrix(double theta);
  Matrix2x2 rx_matrix(double theta);
  Matrix2x2 ry_matrix(double theta);

  Matrix4x4 cnot_matrix();
  Matrix4x4 cz_matrix();
  Matrix4x4 swap_matrix();

  Matrix2x2 matmul2x2(const Matrix2x2 &a, const Matrix2x2 &b);
  bool is_identity(const Matrix2x2 &m, double tol = 1e-10);

  std::vector<CompiledGate>
  cancel_adjacent_gates(const std::vector<CompiledGate> &gates);

  std::vector<CompiledGate>
  fuse_single_qubit_gates(const std::vector<CompiledGate> &gates);

  std::vector<CompiledGate>
  reorder_and_fuse(const std::vector<CompiledGate> &gates, int num_qubits);

  int count_fused_blocks(const std::vector<CompiledGate> &gates);
};

} // namespace jit
} // namespace qubit_engine

#endif // QUANTUM_JIT_HPP
