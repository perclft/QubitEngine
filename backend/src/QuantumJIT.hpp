// Quantum JIT Compiler - Gate Fusion Optimization
// Fuses adjacent gates to reduce circuit depth and improve performance

#ifndef QUANTUM_JIT_HPP
#define QUANTUM_JIT_HPP

#include <array>
#include <chrono>
#define _USE_MATH_DEFINES
#include <cmath>
// Centralized M_PI used from Types.hpp
#include <complex>
#include <list>
#include <memory>
#include <mutex>
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
    O3 = 3, // Aggressive fusion + reordering
    O4 = 4  // Two-qubit adjacent fusion (KAK-style)
  };

  QuantumJIT(OptLevel level = O2) : opt_level_(level) {}

  /// @brief Sets the maximum number of circuits to cache.
  /// @param size The maximum cache size.
  void set_max_cache_size(size_t size) { max_cache_size_ = size; }

  /// @brief Compiles an immediate representation (IR) of the circuit using the current optimizations.
  /// @param num_qubits The number of qubits in the circuit.
  /// @param gates The sequence of recorded gates to compile.
  /// @param params The variational parameters for the circuit.
  /// @return The optimized list of compiled gates within a CircuitIR block.
  CircuitIR
  compile(int num_qubits,
          const std::vector<std::pair<std::string, std::vector<int>>> &gates,
          const std::vector<double> &params = {});

  /// @brief Clears the internal compilation cache.
  void clear_cache();

private:
  OptLevel opt_level_;
  size_t max_cache_size_{1000}; // Default bounding to prevent memory leak

  // Cache for storing previously compiled circuits (LRU eviction)
  std::mutex cache_mutex_;
  std::list<std::pair<std::string, CircuitIR>> ir_cache_list_;
  std::unordered_map<std::string, decltype(ir_cache_list_)::iterator>
      ir_cache_map_;

  // Compute a unique topological hash for a set of gates and parameters
  std::string compute_hash(
      int num_qubits,
      const std::vector<std::pair<std::string, std::vector<int>>> &gates,
      const std::vector<double> &params);

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

  // New Generalized Tensor Network capabilities
  std::vector<Complex> kronecker_product(const std::vector<Complex> &A,
                                         const std::vector<Complex> &B,
                                         int dimA, int dimB);

  std::vector<Complex> matrix_multiply(const std::vector<Complex> &A,
                                       const std::vector<Complex> &B, int dim);

  std::vector<CompiledGate>
  fuse_tensor_network(const std::vector<CompiledGate> &gates);

  std::vector<CompiledGate>
  fuse_two_qubit_adjacent(const std::vector<CompiledGate> &gates);

  int count_fused_blocks(const std::vector<CompiledGate> &gates);
  
private:
  void apply_gate_to_unitary(std::vector<Complex>& unitary, 
                            const std::vector<int>& block_qubits, 
                            const CompiledGate& g);
};

} // namespace jit
} // namespace qubit_engine

#endif // QUANTUM_JIT_HPP
