#include "QuantumJIT.hpp"
#include <algorithm>
#include <iostream>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

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
  return {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0};
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

bool QuantumJIT::is_identity(const Matrix4x4 &m, double tol) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      Complex expected = (i == j) ? Complex(1.0, 0.0) : Complex(0.0, 0.0);
      if (std::abs(Complex(m[i * 4 + j]) - expected) > tol) return false;
    }
  }
  return true;
}

void QuantumJIT::apply_single_to_two(Matrix4x4 &two, const Matrix2x2 &single, int qubit_idx) {
    // qubit_idx 0 is the first target in the two-qubit gate, 1 is the second
    Matrix4x4 result;
    if (qubit_idx == 0) {
        // M = (S \otimes I) * T
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                Complex sum = 0;
                // (S \otimes I)_{ik} = S_{i>>1, k>>1} if (i&1)==(k&1)
                for (int k = 0; k < 4; ++k) {
                    if ((i & 1) == (k & 1)) {
                        sum += Complex(single[(i >> 1) * 2 + (k >> 1)]) * Complex(two[k * 4 + j]);
                    }
                }
                result[i * 4 + j] = sum;
            }
        }
    } else {
        // M = (I \otimes S) * T
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                Complex sum = 0;
                for (int k = 0; k < 4; ++k) {
                    if ((i >> 1) == (k >> 1)) {
                        sum += Complex(single[(i & 1) * 2 + (k & 1)]) * Complex(two[k * 4 + j]);
                    }
                }
                result[i * 4 + j] = sum;
            }
        }
    }
    two = result;
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
                    size_t r01 = row | c_mask; // Index 1: targets[0] set
                    size_t r10 = row | t_mask; // Index 2: targets[1] set
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

static void expand_unitary(std::vector<Complex>& unitary, 
                           const std::vector<int>& old_qubits, 
                           const std::vector<int>& new_qubits) {
    if (unitary.empty()) return;
    if (old_qubits == new_qubits) return;
    
    size_t old_dim = 1ULL << old_qubits.size();
    size_t new_dim = 1ULL << new_qubits.size();
    
    std::vector<Complex> expanded(new_dim * new_dim, Complex(0, 0));
    
    std::vector<int> old_pos_in_new;
    for (int q : old_qubits) {
        auto it = std::find(new_qubits.begin(), new_qubits.end(), q);
        if (it != new_qubits.end()) {
            old_pos_in_new.push_back(std::distance(new_qubits.begin(), it));
        }
    }
    
    std::vector<int> new_only_pos;
    for (size_t i = 0; i < new_qubits.size(); ++i) {
        if (std::find(old_qubits.begin(), old_qubits.end(), new_qubits[i]) == old_qubits.end()) {
            new_only_pos.push_back(i);
        }
    }
    
    for (size_t r = 0; r < new_dim; ++r) {
        size_t r_old = 0;
        for (size_t i = 0; i < old_pos_in_new.size(); ++i) {
            if ((r >> old_pos_in_new[i]) & 1) {
                r_old |= (1ULL << i);
            }
        }
        
        for (size_t c = 0; c < new_dim; ++c) {
            bool match = true;
            for (int pos : new_only_pos) {
                if (((r >> pos) & 1) != ((c >> pos) & 1)) {
                    match = false;
                    break;
                }
            }
            if (!match) continue;
            
            size_t c_old = 0;
            for (size_t i = 0; i < old_pos_in_new.size(); ++i) {
                if ((c >> old_pos_in_new[i]) & 1) {
                    c_old |= (1ULL << i);
                }
            }
            
            expanded[r * new_dim + c] = unitary[r_old * old_dim + c_old];
        }
    }
    
    unitary = std::move(expanded);
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
        
        if (block.type == CompiledGate::SINGLE_QUBIT) {
            for (int i = 0; i < 4; ++i) block.single_matrix[i] = current_unitary[i];
        } else if (block.type == CompiledGate::TWO_QUBIT) {
            for (int i = 0; i < 16; ++i) block.two_matrix[i] = current_unitary[i];
        } else {
            block.fused_unitary = current_unitary;
            block.fused_size = current_dim;
        }
        
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
        
        if (block.type == CompiledGate::SINGLE_QUBIT) {
            for (int i = 0; i < 4; ++i) block.single_matrix[i] = current_unitary[i];
        } else if (block.type == CompiledGate::TWO_QUBIT) {
            for (int i = 0; i < 16; ++i) block.two_matrix[i] = current_unitary[i];
        } else {
            block.fused_unitary = current_unitary;
            block.fused_size = current_dim;
        }
        
        fused_circuit.push_back(block);
      }

      current_block_qubits = g.target_qubits;
      std::sort(current_block_qubits.begin(), current_block_qubits.end());
      current_dim = 1 << current_block_qubits.size();
      current_unitary.clear();
      apply_gate_to_unitary(current_unitary, current_block_qubits, g);
    } else {
      std::vector<int> proposed_qubits_sorted = proposed_qubits;
      std::sort(proposed_qubits_sorted.begin(), proposed_qubits_sorted.end());
      expand_unitary(current_unitary, current_block_qubits, proposed_qubits_sorted);
      current_block_qubits = proposed_qubits_sorted;
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
    
    if (block.type == CompiledGate::SINGLE_QUBIT) {
        for (int i = 0; i < 4; ++i) block.single_matrix[i] = current_unitary[i];
    } else if (block.type == CompiledGate::TWO_QUBIT) {
        for (int i = 0; i < 16; ++i) block.two_matrix[i] = current_unitary[i];
    } else {
        block.fused_unitary = current_unitary;
        block.fused_size = current_dim;
    }
    
    fused_circuit.push_back(block);
  }

  return fused_circuit;
}

// --- Two-Qubit Adjacent Fusion (O4 Prototype) ---
std::vector<CompiledGate>
QuantumJIT::fuse_two_qubit_adjacent(const std::vector<CompiledGate> &gates) {
  if (gates.empty()) return gates;

  std::vector<CompiledGate> fused;
  std::unordered_map<int, int> last_gate;
  std::vector<std::vector<CompiledGate>> original_subcircuits;

  for (const auto &g : gates) {
    if (g.type == CompiledGate::TWO_QUBIT) {
      int q0 = g.target_qubits[0];
      int q1 = g.target_qubits[1];
      
      auto it0 = last_gate.find(q0);
      auto it1 = last_gate.find(q1);
      
      if (it0 != last_gate.end() && it1 != last_gate.end() && it0->second == it1->second) {
        int last_idx = it0->second;
        if (last_idx >= 0 && last_idx < static_cast<int>(fused.size()) && fused[last_idx].type == CompiledGate::TWO_QUBIT) {
          auto &last = fused[last_idx];
          
          if ((last.target_qubits[0] == q0 && last.target_qubits[1] == q1) ||
              (last.target_qubits[0] == q1 && last.target_qubits[1] == q0)) {
            
            std::vector<Complex> m1 = {last.two_matrix.begin(), last.two_matrix.end()};
            std::vector<Complex> m2 = {g.two_matrix.begin(), g.two_matrix.end()};
            std::vector<Complex> m_fused;
            
            if (last.target_qubits[0] != q0) {
               std::vector<Complex> m2_swapped(16);
               auto swap_idx = [](int i) { return (i == 1) ? 2 : (i == 2) ? 1 : i; };
               for (int i = 0; i < 4; ++i) {
                 for (int j = 0; j < 4; ++j) {
                   m2_swapped[i * 4 + j] = m2[swap_idx(i) * 4 + swap_idx(j)];
                 }
               }
               m_fused = matrix_multiply(m2_swapped, m1, 4);
            } else {
               m_fused = matrix_multiply(m2, m1, 4);
            }
            
            std::copy(m_fused.begin(), m_fused.end(), last.two_matrix.begin());
            original_subcircuits[last_idx].push_back(g);
            continue;
          }
        }
      }
    } else if (g.type == CompiledGate::SINGLE_QUBIT) {
        int q = g.target_qubits[0];
        auto it = last_gate.find(q);
        if (it != last_gate.end() && it->second >= 0) {
            int last_idx = it->second;
            if (fused[last_idx].type == CompiledGate::TWO_QUBIT) {
                auto &last = fused[last_idx];
                int q_pos = (last.target_qubits[0] == q) ? 0 : 1;
                apply_single_to_two(last.two_matrix, g.single_matrix, q_pos);
                original_subcircuits[last_idx].push_back(g);
                continue;
            }
        }
    }
    
    // If no fusion occurred, push the gate and update last_gate tracker
    int new_idx = fused.size();
    fused.push_back(g);
    original_subcircuits.push_back({g});
    for (int q : g.target_qubits) {
      last_gate[q] = new_idx;
    }
  }

  // Final sweep: run KAK decomposition on remaining TWO_QUBIT gates and expand them
  std::vector<CompiledGate> cleaned;
  for (size_t idx = 0; idx < fused.size(); ++idx) {
      const auto &g = fused[idx];
      if (g.type == CompiledGate::TWO_QUBIT) {
          if (is_identity(g.two_matrix)) {
              continue; // Strip Identity block
          }
          auto kak = decompose_unitary_kak(g.two_matrix);
          auto synthesized = synthesize_kak(kak, g.target_qubits[0], g.target_qubits[1]);
          if (synthesized.size() <= original_subcircuits[idx].size()) {
              cleaned.insert(cleaned.end(), synthesized.begin(), synthesized.end());
          } else {
              cleaned.insert(cleaned.end(), original_subcircuits[idx].begin(), original_subcircuits[idx].end());
          }
      } else {
          cleaned.push_back(g);
      }
  }
  return cleaned;
}

// --- KAK Decomposition and Synthesis ---

struct ProductGateDecomposition {
    Eigen::Matrix2cd L, R;
    double phase;
};

static ProductGateDecomposition decompose_two_qubit_product_gate(const Eigen::Matrix4cd &W) {
    Eigen::Matrix2cd R = W.block<2,2>(0,0);
    Complex detR = R(0,0)*R(1,1) - R(0,1)*R(1,0);
    if (std::abs(detR) < 0.1) {
        R = W.block<2,2>(2,0);
        detR = R(0,0)*R(1,1) - R(0,1)*R(1,0);
    }
    R /= std::sqrt(detR);

    Eigen::Matrix4cd temp = Eigen::Matrix4cd::Zero();
    Eigen::Matrix2cd R_adj = R.adjoint();
    temp.block<4,2>(0,0) = W.block<4,2>(0,0) * R_adj;
    temp.block<4,2>(0,2) = W.block<4,2>(0,2) * R_adj;

    Eigen::Matrix2cd L;
    L(0,0) = temp(0,0);
    L(0,1) = temp(0,2);
    L(1,0) = temp(2,0);
    L(1,1) = temp(2,2);

    Complex detL = L(0,0)*L(1,1) - L(0,1)*L(1,0);
    L /= std::sqrt(detL);
    double phase = std::arg(detL) / 2.0;

    return {L, R, phase};
}

static Matrix2x2 eigen_to_matrix2x2(const Eigen::Matrix2cd &m) {
    Matrix2x2 res;
    res[0] = m(0,0);
    res[1] = m(0,1);
    res[2] = m(1,0);
    res[3] = m(1,1);
    return res;
}

QuantumJIT::KAKDecomposition QuantumJIT::decompose_unitary_kak(const Matrix4x4 &U) {
  Eigen::Matrix4cd U_eigen;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      U_eigen(i, j) = U[i * 4 + j];
    }
  }

  // 1. Make U be in SU(4)
  Complex detU = U_eigen.determinant();
  U_eigen *= std::pow(detU, -0.25);
  double global_phase = std::arg(detU) / 4.0;

  // Construct magic basis matrix M
  Eigen::Matrix4cd M;
  double r2 = 1.0 / std::sqrt(2.0);
  M << Complex(r2, 0), Complex(0, r2), Complex(0, 0), Complex(0, 0),
       Complex(0, 0), Complex(0, 0), Complex(0, r2), Complex(r2, 0),
       Complex(0, 0), Complex(0, 0), Complex(0, r2), Complex(-r2, 0),
       Complex(r2, 0), Complex(0, -r2), Complex(0, 0), Complex(0, 0);

  Eigen::Matrix4cd Up = M.adjoint() * U_eigen * M;
  Eigen::Matrix4cd M2 = Up.transpose() * Up;

  // Diagonalize M2 = P * D * P^T using the commuting real/imag parts method
  Eigen::Matrix4d P;
  
  // Mix real and imaginary parts deterministically.
  double c_mix = std::cos(0.6);
  double s_mix = std::sin(0.6);
  Eigen::Matrix4d M2real = c_mix * M2.real() + s_mix * M2.imag();
  
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix4d> saes(M2real);
  P = saes.eigenvectors();

  Eigen::Matrix4cd D_mat = P.transpose().cast<std::complex<double>>() * M2 * P.cast<std::complex<double>>();
  Eigen::Vector4cd D = D_mat.diagonal();

  std::vector<double> d(4);
  for (int i = 0; i < 3; ++i) {
    d[i] = -std::arg(D[i]) / 2.0;
  }
  d[3] = -d[0] - d[1] - d[2];

  std::vector<double> cs(3);
  for (int i = 0; i < 3; ++i) {
    cs[i] = std::fmod((d[i] + d[3]) / 2.0, 2.0 * M_PI);
    if (cs[i] < 0) cs[i] += 2.0 * M_PI;
  }

  // Reorder eigenvalues to get in the Weyl chamber
  std::vector<double> cstemp(3);
  for (int i = 0; i < 3; ++i) {
    double val = std::fmod(cs[i], M_PI / 2.0);
    if (val < 0) val += M_PI / 2.0;
    cstemp[i] = std::min(val, M_PI / 2.0 - val);
  }

  // Sort order
  std::vector<int> order = {0, 1, 2};
  std::sort(order.begin(), order.end(), [&](int i, int j) {
    return cstemp[i] < cstemp[j];
  });
  
  std::vector<int> qiskit_order = {order[1], order[2], order[0]};

  std::vector<double> cs_sorted(3);
  std::vector<double> d_sorted(4);
  Eigen::Matrix4d P_sorted = P;
  for (int i = 0; i < 3; ++i) {
    cs_sorted[i] = cs[qiskit_order[i]];
    d_sorted[i] = d[qiskit_order[i]];
    P_sorted.col(i) = P.col(qiskit_order[i]);
  }
  d_sorted[3] = d[3];
  P_sorted.col(3) = P.col(3);

  cs = cs_sorted;
  d = d_sorted;
  P = P_sorted;

  // Fix sign of P to be in SO(4)
  if (P.determinant() < 0) {
    P.col(3) *= -1.0;
  }

  // Find K1, K2
  Eigen::Matrix4cd diag_exp = Eigen::Matrix4cd::Zero();
  for (int i = 0; i < 4; ++i) {
    diag_exp(i, i) = std::exp(Complex(0, d[i]));
  }

  Eigen::Matrix4cd K1_tensor = M * (Up * P.cast<std::complex<double>>() * diag_exp) * M.adjoint();
  Eigen::Matrix4cd K2_tensor = M * P.transpose().cast<std::complex<double>>() * M.adjoint();

  auto dec1 = decompose_two_qubit_product_gate(K1_tensor);
  auto dec2 = decompose_two_qubit_product_gate(K2_tensor);

  Eigen::Matrix2cd K1l = dec1.L;
  Eigen::Matrix2cd K1r = dec1.R;
  Eigen::Matrix2cd K2l = dec2.L;
  Eigen::Matrix2cd K2r = dec2.R;

  global_phase += dec1.phase + dec2.phase;

  // Weyl chamber flips
  Eigen::Matrix2cd ipx, ipy, ipz;
  ipx << 0, Complex(0, 1), Complex(0, 1), 0;
  ipy << 0, 1, -1, 0;
  ipz << Complex(0, 1), 0, 0, Complex(0, -1);

  double pi = M_PI;
  double pi2 = M_PI / 2.0;
  double pi4 = M_PI / 4.0;

  if (cs[0] > pi2) {
    cs[0] -= 3.0 * pi2;
    K1l = K1l * ipy;
    K1r = K1r * ipy;
    global_phase += pi2;
  }
  if (cs[1] > pi2) {
    cs[1] -= 3.0 * pi2;
    K1l = K1l * ipx;
    K1r = K1r * ipx;
    global_phase += pi2;
  }
  int conjs = 0;
  if (cs[0] > pi4) {
    cs[0] = pi2 - cs[0];
    K1l = K1l * ipy;
    K2r = ipy * K2r;
    conjs += 1;
    global_phase -= pi2;
  }
  if (cs[1] > pi4) {
    cs[1] = pi2 - cs[1];
    K1l = K1l * ipx;
    K2r = ipx * K2r;
    conjs += 1;
    global_phase += pi2;
    if (conjs == 1) {
      global_phase -= pi;
    }
  }
  if (cs[2] > pi2) {
    cs[2] -= 3.0 * pi2;
    K1l = K1l * ipz;
    K1r = K1r * ipz;
    global_phase += pi2;
    if (conjs == 1) {
      global_phase -= pi;
    }
  }
  if (conjs == 1) {
    cs[2] = pi2 - cs[2];
    K1l = K1l * ipz;
    K2r = ipz * K2r;
    global_phase += pi2;
  }
  if (cs[2] > pi4) {
    cs[2] -= pi2;
    K1l = K1l * ipz;
    K1r = K1r * ipz;
    global_phase -= pi2;
  }

  KAKDecomposition kak;
  kak.x = cs[1];
  kak.y = cs[0];
  kak.z = cs[2];

  Complex phase_factor = std::exp(Complex(0, global_phase));
  K1l *= phase_factor;

  kak.A1 = eigen_to_matrix2x2(K1l);
  kak.A2 = eigen_to_matrix2x2(K1r);
  kak.B1 = eigen_to_matrix2x2(K2l);
  kak.B2 = eigen_to_matrix2x2(K2r);
  return kak;
}

std::vector<CompiledGate> QuantumJIT::synthesize_kak(const KAKDecomposition &kak, int q0, int q1) {
  std::vector<CompiledGate> synthesized;

  auto make_single_gate = [](int target, const Matrix2x2 &m) {
    CompiledGate g;
    g.type = CompiledGate::SINGLE_QUBIT;
    g.target_qubits = {target};
    g.single_matrix = m;
    return g;
  };

  // Helper to map single qubit matrices back to named/discrete Clifford gates to preserve compatibility
  auto map_to_clifford_gates = [make_single_gate, this](int target, const Matrix2x2 &m) {
    double tol = 1e-9;
    auto trace_prod = [](const Matrix2x2 &a, const Matrix2x2 &b) {
        Complex sum = 0.0;
        for (int i = 0; i < 4; ++i) {
            sum += std::conj(Complex(a[i])) * Complex(b[i]);
        }
        return std::abs(sum);
    };

    Matrix2x2 S_DAGGER = {1, 0, 0, Complex(0, -1)};
    Matrix2x2 H_S = matmul2x2(HADAMARD, S_GATE);
    Matrix2x2 S_H = matmul2x2(S_GATE, HADAMARD);
    Matrix2x2 H_S_H = matmul2x2(HADAMARD, matmul2x2(S_GATE, HADAMARD));

    std::vector<CompiledGate> res;
    if (trace_prod(m, IDENTITY) > 2.0 - tol) return res;
    if (trace_prod(m, HADAMARD) > 2.0 - tol) { res.push_back(make_single_gate(target, HADAMARD)); return res; }
    if (trace_prod(m, PAULI_X) > 2.0 - tol) { res.push_back(make_single_gate(target, PAULI_X)); return res; }
    if (trace_prod(m, PAULI_Y) > 2.0 - tol) { res.push_back(make_single_gate(target, PAULI_Y)); return res; }
    if (trace_prod(m, PAULI_Z) > 2.0 - tol) { res.push_back(make_single_gate(target, PAULI_Z)); return res; }
    if (trace_prod(m, S_GATE) > 2.0 - tol) { res.push_back(make_single_gate(target, S_GATE)); return res; }
    if (trace_prod(m, S_DAGGER) > 2.0 - tol) { res.push_back(make_single_gate(target, S_DAGGER)); return res; }
    if (trace_prod(m, H_S) > 2.0 - tol) { 
        res.push_back(make_single_gate(target, S_GATE)); 
        res.push_back(make_single_gate(target, HADAMARD)); 
        return res; 
    }
    if (trace_prod(m, S_H) > 2.0 - tol) { 
        res.push_back(make_single_gate(target, HADAMARD)); 
        res.push_back(make_single_gate(target, S_GATE)); 
        return res; 
    }
    if (trace_prod(m, H_S_H) > 2.0 - tol) { 
        res.push_back(make_single_gate(target, HADAMARD)); 
        res.push_back(make_single_gate(target, S_GATE)); 
        res.push_back(make_single_gate(target, HADAMARD)); 
        return res; 
    }

    res.push_back(make_single_gate(target, m));
    return res;
  };

  auto insert_clifford_gates = [&](int target, const Matrix2x2 &m) {
    auto gates = map_to_clifford_gates(target, m);
    synthesized.insert(synthesized.end(), gates.begin(), gates.end());
  };

  // Helper to compute adjoint of Matrix2x2
  auto adjoint2x2 = [](const Matrix2x2 &m) -> Matrix2x2 {
    return {std::conj(Complex(m[0])), std::conj(Complex(m[2])),
            std::conj(Complex(m[1])), std::conj(Complex(m[3]))};
  };

  // Pre-computed constant matrices for CNOT-based Weyl/Cartan decomposition
  const Matrix2x2 CX_K1L = {Complex(0.8478065680, 0.0000000000), Complex(0.0000000000, 0.5303055943), Complex(0.0000000000, 0.5303055943), Complex(0.8478065680, 0.0000000000)};
  const Matrix2x2 CX_K1R = {Complex(-0.5000000000, 0.5000000000), Complex(-0.5000000000, 0.5000000000), Complex(0.5000000000, 0.5000000000), Complex(-0.5000000000, -0.5000000000)};
  const Matrix2x2 CX_K2L = {Complex(0.2245070915, 0.0000000000), Complex(0.0000000000, -0.9744724552), Complex(0.0000000000, -0.9744724552), Complex(0.2245070915, 0.0000000000)};
  const Matrix2x2 CX_K2R = {Complex(0.7071067812, 0.0000000000), Complex(-0.7071067812, 0.0000000000), Complex(0.7071067812, 0.0000000000), Complex(0.7071067812, 0.0000000000)};

  const Matrix2x2 Q0L = {Complex(0.1587504869, -0.1587504869), Complex(-0.6890560811, -0.6890560811), Complex(0.6890560811, -0.6890560811), Complex(0.1587504869, 0.1587504869)};
  const Matrix2x2 Q0R = {Complex(0.0000000000, -0.7071067812), Complex(-0.0000000000, -0.7071067812), Complex(0.0000000000, -0.7071067812), Complex(0.0000000000, 0.7071067812)};
  const Matrix2x2 Q1LA = {Complex(-0.5994897733, 0.5994897733), Complex(0.3749826818, -0.3749826818), Complex(-0.3749826818, -0.3749826818), Complex(-0.5994897733, -0.5994897733)};
  const Matrix2x2 Q1LB = {Complex(-0.6890560811, -0.6890560811), Complex(0.1587504869, -0.1587504869), Complex(-0.1587504869, -0.1587504869), Complex(-0.6890560811, 0.6890560811)};
  const Matrix2x2 Q1RA = {Complex(0.5000000000, 0.5000000000), Complex(0.5000000000, 0.5000000000), Complex(-0.5000000000, 0.5000000000), Complex(0.5000000000, -0.5000000000)};
  const Matrix2x2 Q1RB = {Complex(0.7071067812, 0.0000000000), Complex(0.7071067812, 0.0000000000), Complex(-0.7071067812, 0.0000000000), Complex(0.7071067812, 0.0000000000)};
  const Matrix2x2 Q2L = {Complex(-0.3749826818, -0.3749826818), Complex(0.5994897733, 0.5994897733), Complex(-0.5994897733, 0.5994897733), Complex(-0.3749826818, 0.3749826818)};
  const Matrix2x2 Q2R = {Complex(-0.5000000000, 0.5000000000), Complex(0.5000000000, -0.5000000000), Complex(-0.5000000000, -0.5000000000), Complex(-0.5000000000, -0.5000000000)};

  const Matrix2x2 U0L = {Complex(0.5994897733, -0.3749826818), Complex(0.5994897733, -0.3749826818), Complex(-0.5994897733, -0.3749826818), Complex(0.5994897733, 0.3749826818)};
  const Matrix2x2 U0R = {Complex(0.5000000000, -0.5000000000), Complex(0.5000000000, 0.5000000000), Complex(-0.5000000000, 0.5000000000), Complex(0.5000000000, 0.5000000000)};
  const Matrix2x2 U1L = {Complex(0.5000000000, -0.2308205892), Complex(0.6683725425, 0.5000000000), Complex(-0.6683725425, 0.5000000000), Complex(0.5000000000, 0.2308205892)};
  const Matrix2x2 U1RA = {Complex(0.7071067812, 0.0000000000), Complex(0.0000000000, -0.7071067812), Complex(-0.0000000000, -0.7071067812), Complex(0.7071067812, 0.0000000000)};
  const Matrix2x2 U1RB = {Complex(-0.5000000000, -0.5000000000), Complex(-0.5000000000, -0.5000000000), Complex(0.5000000000, -0.5000000000), Complex(-0.5000000000, 0.5000000000)};
  const Matrix2x2 U2LA = {Complex(0.1587504869, 0.6890560811), Complex(-0.1587504869, 0.6890560811), Complex(0.1587504869, 0.6890560811), Complex(0.1587504869, -0.6890560811)};
  const Matrix2x2 U2LB = {Complex(-0.6890560811, -0.6890560811), Complex(0.1587504869, -0.1587504869), Complex(-0.1587504869, -0.1587504869), Complex(-0.6890560811, 0.6890560811)};
  const Matrix2x2 U2RA = {Complex(-0.7071067812, 0.0000000000), Complex(0.7071067812, 0.0000000000), Complex(-0.7071067812, 0.0000000000), Complex(-0.7071067812, 0.0000000000)};
  const Matrix2x2 U2RB = {Complex(0.7071067812, 0.0000000000), Complex(0.7071067812, 0.0000000000), Complex(-0.7071067812, 0.0000000000), Complex(0.7071067812, 0.0000000000)};
  const Matrix2x2 U3L = {Complex(-0.3749826818, -0.3749826818), Complex(0.5994897733, 0.5994897733), Complex(-0.5994897733, 0.5994897733), Complex(-0.3749826818, 0.3749826818)};
  const Matrix2x2 U3R = {Complex(-0.5000000000, 0.5000000000), Complex(0.5000000000, -0.5000000000), Complex(-0.5000000000, -0.5000000000), Complex(-0.5000000000, -0.5000000000)};

  double x = kak.x;
  double y = kak.y;
  double z = kak.z;

  // CNOT gate (control = q0, target = q1)
  CompiledGate cx;
  cx.type = CompiledGate::TWO_QUBIT;
  cx.target_qubits = {q0, q1};
  cx.two_matrix = cnot_matrix();

  double tol = 1e-9;
  if (std::abs(x) < tol && std::abs(y) < tol && std::abs(z) < tol) {
    // 0 CNOTs
    Matrix2x2 post_q0 = matmul2x2(kak.A2, kak.B2);
    Matrix2x2 post_q1 = matmul2x2(kak.A1, kak.B1);

    insert_clifford_gates(q0, post_q0);
    insert_clifford_gates(q1, post_q1);
  } else if (std::abs(x - M_PI / 4.0) < tol && std::abs(y) < tol && std::abs(z) < tol) {
    // 1 CNOT
    Matrix2x2 pre_q0 = matmul2x2(adjoint2x2(CX_K2R), kak.B2);
    Matrix2x2 pre_q1 = matmul2x2(adjoint2x2(CX_K2L), kak.B1);
    Matrix2x2 post_q0 = matmul2x2(kak.A2, adjoint2x2(CX_K1R));
    Matrix2x2 post_q1 = matmul2x2(kak.A1, adjoint2x2(CX_K1L));

    insert_clifford_gates(q0, pre_q0);
    insert_clifford_gates(q1, pre_q1);
    synthesized.push_back(cx);
    insert_clifford_gates(q0, post_q0);
    insert_clifford_gates(q1, post_q1);
  } else if (std::abs(z) < tol) {
    // 2 CNOTs
    Matrix2x2 pre_q0 = matmul2x2(Q2R, kak.B2);
    Matrix2x2 pre_q1 = matmul2x2(Q2L, kak.B1);
    Matrix2x2 mid_q0 = matmul2x2(Q1RA, matmul2x2(rz_matrix(2.0 * y), Q1RB));
    Matrix2x2 mid_q1 = matmul2x2(Q1LA, matmul2x2(rz_matrix(-2.0 * x), Q1LB));
    Matrix2x2 post_q0 = matmul2x2(kak.A2, Q0R);
    Matrix2x2 post_q1 = matmul2x2(kak.A1, Q0L);

    insert_clifford_gates(q0, pre_q0);
    insert_clifford_gates(q1, pre_q1);
    synthesized.push_back(cx);
    insert_clifford_gates(q0, mid_q0);
    insert_clifford_gates(q1, mid_q1);
    synthesized.push_back(cx);
    insert_clifford_gates(q0, post_q0);
    insert_clifford_gates(q1, post_q1);
  } else {
    // 3 CNOTs
    Matrix2x2 pre_q0 = matmul2x2(U3R, kak.B2);
    Matrix2x2 pre_q1 = matmul2x2(U3L, kak.B1);
    Matrix2x2 mid1_q0 = matmul2x2(U2RA, matmul2x2(rz_matrix(2.0 * y), U2RB));
    Matrix2x2 mid1_q1 = matmul2x2(U2LA, matmul2x2(rz_matrix(-2.0 * x), U2LB));
    Matrix2x2 mid2_q0 = matmul2x2(U1RA, matmul2x2(rz_matrix(-2.0 * z), U1RB));
    Matrix2x2 mid2_q1 = U1L;
    Matrix2x2 post_q0 = matmul2x2(kak.A2, U0R);
    Matrix2x2 post_q1 = matmul2x2(kak.A1, U0L);

    insert_clifford_gates(q0, pre_q0);
    insert_clifford_gates(q1, pre_q1);
    synthesized.push_back(cx);
    insert_clifford_gates(q0, mid1_q0);
    insert_clifford_gates(q1, mid1_q1);
    synthesized.push_back(cx);
    insert_clifford_gates(q0, mid2_q0);
    insert_clifford_gates(q1, mid2_q1);
    synthesized.push_back(cx);
    insert_clifford_gates(q0, post_q0);
    insert_clifford_gates(q1, post_q1);
  }

  return synthesized;
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
