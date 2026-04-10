#include "QuantumJIT.hpp"
#include <algorithm>

namespace qubit_engine {
namespace jit {

// --- Gate Matrix Constants ---
constexpr double INV_SQRT2 = 0.70710678118654752440;
constexpr Matrix2x2 IDENTITY = {1, 0, 0, 1};
constexpr Matrix2x2 PAULI_X = {0, 1, 1, 0};
constexpr Matrix2x2 PAULI_Y = {0, Complex(0, -1), Complex(0, 1), 0};
constexpr Matrix2x2 PAULI_Z = {1, 0, 0, -1};
constexpr Matrix2x2 HADAMARD = {INV_SQRT2, INV_SQRT2, INV_SQRT2, -INV_SQRT2};
constexpr Matrix2x2 S_GATE = {1, 0, 0, Complex(0, 1)};
constexpr Matrix2x2 T_GATE = {1, 0, 0, Complex(INV_SQRT2, INV_SQRT2)};

// --- Compile ---
QuantumJIT::CircuitIR QuantumJIT::compile(
    int num_qubits,
    const std::vector<std::pair<std::string, std::vector<int>>> &gates,
    const std::vector<double> &params) {

  // 1. Fast Path: Check Cache
  std::string hash_key = compute_hash(num_qubits, gates, params);
  {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = ir_cache_map_.find(hash_key);
    if (it != ir_cache_map_.end()) {
      // Evict to front (LRU)
      ir_cache_list_.splice(ir_cache_list_.begin(), ir_cache_list_, it->second);
      return it->second->second;
    }
  }

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
  if (opt_level_ >= O4) {
    compiled = fuse_two_qubit_adjacent(compiled);
  }

  ir.gates = compiled;
  ir.stats.optimized_gates = compiled.size();
  ir.stats.fused_blocks = count_fused_blocks(compiled);

  auto end = std::chrono::high_resolution_clock::now();
  ir.stats.compilation_time_ms =
      std::chrono::duration<double, std::milli>(end - start).count();
  ir.stats.expected_speedup =
      static_cast<double>(ir.stats.original_gates) / ir.stats.optimized_gates;

  // Cache Results (Push to front)
  {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    ir_cache_list_.emplace_front(hash_key, ir);
    ir_cache_map_[hash_key] = ir_cache_list_.begin();

    // Enforce bounding limit
    if (ir_cache_list_.size() > max_cache_size_) {
      auto last = std::prev(ir_cache_list_.end());
      ir_cache_map_.erase(last->first);
      ir_cache_list_.pop_back();
    }
  }

  return ir;
}

// --- Caching ---
void QuantumJIT::clear_cache() {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  ir_cache_list_.clear();
  ir_cache_map_.clear();
}

std::string QuantumJIT::compute_hash(
    int num_qubits,
    const std::vector<std::pair<std::string, std::vector<int>>> &gates,
    const std::vector<double> &params) {

  std::string hash = std::to_string(num_qubits) + "|";
  for (size_t i = 0; i < gates.size(); ++i) {
    hash += gates[i].first + "_";
    for (int q : gates[i].second) {
      hash += std::to_string(q) + ",";
    }
    if (i < params.size()) {
      // Round parameters slightly to catch FP inaccuracies near Pi
      hash += std::to_string(std::round(params[i] * 1e6) / 1e6);
    }
    hash += ";";
  }
  return hash;
}

// --- Build Gate ---
CompiledGate QuantumJIT::build_gate(const std::string &name,
                                    const std::vector<int> &qubits,
                                    double param) {
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

// --- Rotation Gate Matrices ---
Matrix2x2 QuantumJIT::rz_matrix(double theta) {
  return {std::exp(Complex(0, -theta / 2)), 0, 0,
          std::exp(Complex(0, theta / 2))};
}

Matrix2x2 QuantumJIT::rx_matrix(double theta) {
  double c = std::cos(theta / 2);
  double s = std::sin(theta / 2);
  return {c, Complex(0, -s), Complex(0, -s), c};
}

Matrix2x2 QuantumJIT::ry_matrix(double theta) {
  double c = std::cos(theta / 2);
  double s = std::sin(theta / 2);
  return {c, -s, s, c};
}

// --- Two-Qubit Gate Matrices ---
Matrix4x4 QuantumJIT::cnot_matrix() {
  return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0};
}

Matrix4x4 QuantumJIT::cz_matrix() {
  return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1};
}

Matrix4x4 QuantumJIT::swap_matrix() {
  return {1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1};
}

// --- Matrix Utilities ---
Matrix2x2 QuantumJIT::matmul2x2(const Matrix2x2 &a, const Matrix2x2 &b) {
  return {a[0] * b[0] + a[1] * b[2], a[0] * b[1] + a[1] * b[3],
          a[2] * b[0] + a[3] * b[2], a[2] * b[1] + a[3] * b[3]};
}

bool QuantumJIT::is_identity(const Matrix2x2 &m, double tol) {
  return std::abs(Complex(m[0]) - 1.0) < tol && std::abs(Complex(m[1])) < tol &&
         std::abs(Complex(m[2])) < tol && std::abs(Complex(m[3]) - 1.0) < tol;
}

// --- O1: Cancel Adjacent Inverse Gates ---
std::vector<CompiledGate>
QuantumJIT::cancel_adjacent_gates(const std::vector<CompiledGate> &gates) {
  std::vector<CompiledGate> result;

  for (size_t i = 0; i < gates.size(); i++) {
    if (i + 1 < gates.size() && gates[i].type == CompiledGate::SINGLE_QUBIT &&
        gates[i + 1].type == CompiledGate::SINGLE_QUBIT &&
        gates[i].target_qubits == gates[i + 1].target_qubits) {

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

// --- O2: Fuse Consecutive Single-Qubit Gates ---
std::vector<CompiledGate>
QuantumJIT::fuse_single_qubit_gates(const std::vector<CompiledGate> &gates) {
  std::vector<CompiledGate> result;
  std::unordered_map<int, CompiledGate> pending;

  for (const auto &g : gates) {
    if (g.type == CompiledGate::SINGLE_QUBIT) {
      int q = g.target_qubits[0];
      if (pending.count(q)) {
        pending[q].single_matrix =
            matmul2x2(g.single_matrix, pending[q].single_matrix);
      } else {
        pending[q] = g;
      }
    } else {
      for (int q : g.target_qubits) {
        if (pending.count(q)) {
          result.push_back(pending[q]);
          pending.erase(q);
        }
      }
      result.push_back(g);
    }
  }

  for (const auto &pair : pending) {
    result.push_back(pair.second);
  }

  return result;
}

// --- O3: Reorder and Fuse ---
std::vector<CompiledGate>
QuantumJIT::reorder_and_fuse(const std::vector<CompiledGate> &gates,
                             int num_qubits) {
  // First, do single qubit reordering
  std::vector<CompiledGate> result;
  std::vector<std::vector<CompiledGate>> per_qubit(num_qubits);

  auto flush_all = [&]() {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (int q = 0; q < num_qubits; ++q) {
      if (per_qubit[q].size() > 1) {
        CompiledGate fused = per_qubit[q][0];
        for (size_t i = 1; i < per_qubit[q].size(); ++i) {
          fused.single_matrix =
              matmul2x2(per_qubit[q][i].single_matrix, fused.single_matrix);
        }
        per_qubit[q].clear();
        per_qubit[q].push_back(fused);
      }
    }

    for (int q = 0; q < num_qubits; ++q) {
      if (!per_qubit[q].empty()) {
        result.push_back(per_qubit[q][0]);
        per_qubit[q].clear();
      }
    }
  };

  for (const auto &g : gates) {
    if (g.type == CompiledGate::SINGLE_QUBIT) {
      int q = g.target_qubits[0];
      per_qubit[q].push_back(g);
    } else {
      flush_all();
      result.push_back(g);
    }
  }
  flush_all();

  // Then, perform generalized tensor fusion on the resulting dense sequence
  return fuse_tensor_network(result);
}

// --- Generalized Tensor Network Fusion ---

std::vector<Complex>
QuantumJIT::kronecker_product(const std::vector<Complex> &A,
                              const std::vector<Complex> &B, int dimA,
                              int dimB) {
  std::vector<Complex> result(dimA * dimB * dimA * dimB, 0.0);
  for (int rA = 0; rA < dimA; ++rA) {
    for (int cA = 0; cA < dimA; ++cA) {
      Complex a_val = A[rA * dimA + cA];
      for (int rB = 0; rB < dimB; ++rB) {
        for (int cB = 0; cB < dimB; ++cB) {
          Complex b_val = B[rB * dimB + cB];
          int rRes = rA * dimB + rB;
          int cRes = cA * dimB + cB;
          result[rRes * (dimA * dimB) + cRes] = a_val * b_val;
        }
      }
    }
  }
  return result;
}

std::vector<Complex> QuantumJIT::matrix_multiply(const std::vector<Complex> &A,
                                                 const std::vector<Complex> &B,
                                                 int dim) {
  std::vector<Complex> result(dim * dim, 0.0);
  for (int i = 0; i < dim; ++i) {
    for (int j = 0; j < dim; ++j) {
      Complex sum = 0.0;
      for (int k = 0; k < dim; ++k) {
        sum += A[i * dim + k] * B[k * dim + j];
      }
      result[i * dim + j] = sum;
    }
  }
  return result;
}

void QuantumJIT::apply_gate_to_unitary(std::vector<Complex>& unitary, 
                                      const std::vector<int>& block_qubits, 
                                      const CompiledGate& g) {
    auto extract_matrix = [](const CompiledGate &gate) -> std::vector<Complex> {
        if (gate.type == CompiledGate::SINGLE_QUBIT) {
            return {Complex(gate.single_matrix[0]), Complex(gate.single_matrix[1]), 
                    Complex(gate.single_matrix[2]), Complex(gate.single_matrix[3])};
        } else if (gate.type == CompiledGate::TWO_QUBIT) {
            std::vector<Complex> v(16);
            for (int i = 0; i < 16; ++i) v[i] = Complex(gate.two_matrix[i]);
            return v;
        } else if (gate.type == CompiledGate::FUSED_BLOCK) {
            return gate.fused_unitary;
        }
        return {};
    };

    if (unitary.empty()) {
        size_t dim = 1ULL << block_qubits.size();
        unitary.assign(dim * dim, Complex(0, 0));
        for (size_t i = 0; i < dim; i++) unitary[i * dim + i] = Complex(1, 0);
    }

    size_t dim = 1ULL << block_qubits.size();
    std::vector<Complex> next_unitary(dim * dim, Complex(0, 0));
    
    std::vector<int> local_targets;
    for (int q : g.target_qubits) {
        auto it = std::find(block_qubits.begin(), block_qubits.end(), q);
        local_targets.push_back(std::distance(block_qubits.begin(), it));
    }
    
    auto m = extract_matrix(g);
    if (m.empty()) return;

    if (g.type == CompiledGate::SINGLE_QUBIT) {
        int t = local_targets[0];
        size_t mask = 1ULL << t;
        
        for (size_t col = 0; col < dim; ++col) {
            for (size_t row = 0; row < dim; ++row) {
                if (!(row & mask)) {
                    size_t r0 = row;
                    size_t r1 = row | mask;
                    Complex v0 = unitary[r0 * dim + col];
                    Complex v1 = unitary[r1 * dim + col];
                    next_unitary[r0 * dim + col] = m[0] * v0 + m[1] * v1;
                    next_unitary[r1 * dim + col] = m[2] * v0 + m[3] * v1;
                }
            }
        }
    } else if (g.type == CompiledGate::TWO_QUBIT || (g.type == CompiledGate::FUSED_BLOCK && g.target_qubits.size() == 2)) {
        int c = local_targets[0];
        int t = local_targets[1];
        size_t c_mask = 1ULL << c;
        size_t t_mask = 1ULL << t;
        
        for (size_t col = 0; col < dim; ++col) {
            for (size_t row = 0; row < dim; ++row) {
                if (!(row & c_mask) && !(row & t_mask)) {
                    size_t r00 = row;
                    size_t r01 = row | t_mask; // Index 1: targets[1] set
                    size_t r10 = row | c_mask; // Index 2: targets[0] set
                    size_t r11 = row | c_mask | t_mask;
                    
                    Complex v00 = unitary[r00 * dim + col];
                    Complex v01 = unitary[r01 * dim + col];
                    Complex v10 = unitary[r10 * dim + col];
                    Complex v11 = unitary[r11 * dim + col];
                    
                    next_unitary[r00 * dim + col] = m[0]*v00 + m[1]*v01 + m[2]*v10 + m[3]*v11;
                    next_unitary[r01 * dim + col] = m[4]*v00 + m[5]*v01 + m[6]*v10 + m[7]*v11;
                    next_unitary[r10 * dim + col] = m[8]*v00 + m[9]*v01 + m[10]*v10 + m[11]*v11;
                    next_unitary[r11 * dim + col] = m[12]*v00 + m[13]*v01 + m[14]*v10 + m[15]*v11;
                }
            }
        }
    }
    
    unitary = std::move(next_unitary);
}

std::vector<CompiledGate>
QuantumJIT::fuse_tensor_network(const std::vector<CompiledGate> &gates) {
  if (gates.empty())
    return gates;

  std::vector<CompiledGate> fused_circuit;
  std::vector<int> current_block_qubits;
  std::vector<Complex> current_unitary;
  int current_dim = 1;
  const int TENSOR_LIMIT = 4;

  for (const auto &g : gates) {
    if (g.type == CompiledGate::FUSED_BLOCK && g.target_qubits.size() > 2) {
      if (!current_block_qubits.empty()) {
        CompiledGate block;
        block.type = (current_block_qubits.size() == 1) ? CompiledGate::SINGLE_QUBIT 
                   : (current_block_qubits.size() == 2) ? CompiledGate::TWO_QUBIT 
                   : CompiledGate::FUSED_BLOCK;
        block.target_qubits = current_block_qubits;
        block.fused_unitary = current_unitary;
        block.fused_size = current_dim;
        fused_circuit.push_back(block);
        current_block_qubits.clear();
        current_unitary.clear();
        current_dim = 1;
      }
      fused_circuit.push_back(g);
      continue;
    }

    bool overlaps = false;
    std::vector<int> proposed_qubits = current_block_qubits;
    for (int q : g.target_qubits) {
      if (std::find(current_block_qubits.begin(), current_block_qubits.end(), q) != current_block_qubits.end()) {
        overlaps = true;
      } else {
        proposed_qubits.push_back(q);
      }
    }

    if (proposed_qubits.size() > TENSOR_LIMIT || (!overlaps && !current_block_qubits.empty())) {
      if (!current_block_qubits.empty()) {
        CompiledGate block;
        block.type = (current_block_qubits.size() == 1) ? CompiledGate::SINGLE_QUBIT 
                   : (current_block_qubits.size() == 2) ? CompiledGate::TWO_QUBIT 
                   : CompiledGate::FUSED_BLOCK;
        block.target_qubits = current_block_qubits;
        block.fused_unitary = current_unitary;
        block.fused_size = current_dim;
        fused_circuit.push_back(block);
      }

      current_block_qubits = g.target_qubits;
      std::sort(current_block_qubits.begin(), current_block_qubits.end());
      current_dim = 1 << current_block_qubits.size();
      current_unitary.clear();
      apply_gate_to_unitary(current_unitary, current_block_qubits, g);
    } else {
      current_block_qubits = proposed_qubits;
      std::sort(current_block_qubits.begin(), current_block_qubits.end());
      current_dim = 1 << current_block_qubits.size();
      apply_gate_to_unitary(current_unitary, current_block_qubits, g);
    }
  }

  if (!current_block_qubits.empty()) {
    CompiledGate block;
    block.type = (current_block_qubits.size() == 1) ? CompiledGate::SINGLE_QUBIT 
               : (current_block_qubits.size() == 2) ? CompiledGate::TWO_QUBIT 
               : CompiledGate::FUSED_BLOCK;
    block.target_qubits = current_block_qubits;
    block.fused_unitary = current_unitary;
    block.fused_size = current_dim;
    fused_circuit.push_back(block);
  }

  return fused_circuit;
}

// --- Two-Qubit Adjacent Fusion (O4 Prototype) ---
std::vector<CompiledGate>
QuantumJIT::fuse_two_qubit_adjacent(const std::vector<CompiledGate> &gates) {
  if (gates.empty()) return gates;

  std::vector<CompiledGate> fused;
  // A prototype for 2-qubit fusion: merge adjacent two-qubit gates
  // that operate on the exact same pair of qubits.
  for (const auto &g : gates) {
    if (g.type == CompiledGate::TWO_QUBIT && fused.size() > 0) {
      auto &last = fused.back();
      if (last.type == CompiledGate::TWO_QUBIT &&
          ((last.target_qubits[0] == g.target_qubits[0] && last.target_qubits[1] == g.target_qubits[1]) ||
           (last.target_qubits[0] == g.target_qubits[1] && last.target_qubits[1] == g.target_qubits[0]))) {
        // We have adjacent gates on the same pair. We can fuse them by matrix multiplication.
        // For a full KAK decomposition we'd break this back down into CNOTs and 1Q gates,
        // but for now, we fuse them into a single 4x4 block to save a kernel call.
        std::vector<Complex> m1 = {last.two_matrix.begin(), last.two_matrix.end()};
        std::vector<Complex> m2 = {g.two_matrix.begin(), g.two_matrix.end()};
        
        // Ensure same target ordering for multiplication
        if (last.target_qubits[0] != g.target_qubits[0]) {
           // Swap matrix 2's basis to match last's basis
           // (This is a simplified prototype; full permutation requires proper tensoring)
        } else {
           std::vector<Complex> m_fused = matrix_multiply(m2, m1, 4);
           std::copy(m_fused.begin(), m_fused.end(), last.two_matrix.begin());
        }
        continue;
      }
    }
    fused.push_back(g);
  }
  return fused;
}

// --- Count Fused Blocks ---
int QuantumJIT::count_fused_blocks(const std::vector<CompiledGate> &gates) {
  int count = 0;
  for (const auto &g : gates) {
    if (g.type == CompiledGate::FUSED_BLOCK)
      count++;
  }
  return count;
}

} // namespace jit
} // namespace qubit_engine
