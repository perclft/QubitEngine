#include "MPSBackend.hpp"
#include <Eigen/Dense>
#include "../Exceptions.hpp"
#include <Eigen/SVD>
#include <cmath>
#include <stdexcept>
#include <random>
#include <algorithm>
#include <vector>


namespace qubit_engine {

MPSBackend::MPSBackend(int num_qubits, int max_bond_dimension)
    : num_qubits(num_qubits), max_bond_dimension(max_bond_dimension) {

  // Initialize to |00...0> state
  nodes.resize(num_qubits);
  for (int i = 0; i < num_qubits; ++i) {
    nodes[i].left_dim = 1;
    nodes[i].right_dim = 1;
    nodes[i].data.resize(2, Complex(0.0, 0.0));
    nodes[i].data[0] = Complex(1.0, 0.0); // |0> amplitude
  }
}

void MPSBackend::applySingleQubitGate(size_t target,
                                      const std::vector<Complex> &matrix) {
  if (target >= num_qubits)
    return;

  auto &tensor = nodes[target];
  std::vector<Complex> new_data(tensor.data.size(), Complex(0, 0));

  // matrix is 2x2. tensor is (left_dim, 2, right_dim)
  for (int l = 0; l < tensor.left_dim; ++l) {
    for (int r = 0; r < tensor.right_dim; ++r) {
      for (int p_out = 0; p_out < 2; ++p_out) {
        Complex sum(0, 0);
        for (int p_in = 0; p_in < 2; ++p_in) {
          Complex gate_val = matrix[p_out * 2 + p_in];
          Complex tensor_val =
              tensor
                  .data[l * 2 * tensor.right_dim + p_in * tensor.right_dim + r];
          sum += gate_val * tensor_val;
        }
        new_data[l * 2 * tensor.right_dim + p_out * tensor.right_dim + r] = sum;
      }
    }
  }
  tensor.data = new_data;
}

void MPSBackend::applyTwoQubitGate(size_t q1, size_t q2,
                                   const std::vector<Complex> &matrix) {
  if (std::abs((int)q1 - (int)q2) != 1) {
    static const std::vector<Complex> SWAP_MAT = {
        1, 0, 0, 0,
        0, 0, 1, 0,
        0, 1, 0, 0,
        0, 0, 0, 1
    };
    if (q1 < q2) {
      for (size_t i = q1; i < q2 - 1; ++i) {
        applyTwoQubitGate(i, i + 1, SWAP_MAT);
      }
      applyTwoQubitGate(q2 - 1, q2, matrix);
      for (size_t i = q2 - 2; i >= q1 && i != (size_t)-1; --i) {
        applyTwoQubitGate(i, i + 1, SWAP_MAT);
      }
    } else {
      for (size_t i = q1; i > q2 + 1; --i) {
        applyTwoQubitGate(i, i - 1, SWAP_MAT);
      }
      applyTwoQubitGate(q2 + 1, q2, matrix);
      for (size_t i = q2 + 2; i <= q1; ++i) {
        applyTwoQubitGate(i, i - 1, SWAP_MAT);
      }
    }
    return;
  }

  size_t left = std::min(q1, q2);
  bool swap_indices = (q1 > q2);

  auto &nodeL = nodes[left];
  auto &nodeR = nodes[left + 1];

  int L1 = nodeL.left_dim;
  int D = nodeL.right_dim; // Same as nodeR.left_dim
  int R2 = nodeR.right_dim;

  // 1. Contract nodes[left] and nodes[left+1]
  // C(l, p1, p2, r)
  std::vector<Complex> C(L1 * 2 * 2 * R2, Complex(0, 0));
  for (int l = 0; l < L1; ++l) {
    for (int p1 = 0; p1 < 2; ++p1) {
      for (int p2 = 0; p2 < 2; ++p2) {
        for (int r = 0; r < R2; ++r) {
          Complex sum(0, 0);
          for (int d = 0; d < D; ++d) {
            sum += nodeL.data[l * (2 * D) + p1 * D + d] *
                   nodeR.data[d * (2 * R2) + p2 * R2 + r];
          }
          C[l * (4 * R2) + p1 * (2 * R2) + p2 * R2 + r] = sum;
        }
      }
    }
  }

  // 2. Apply 4x4 gate matrix
  std::vector<Complex> C_new(L1 * 2 * 2 * R2, Complex(0, 0));
  for (int l = 0; l < L1; ++l) {
    for (int r = 0; r < R2; ++r) {
      for (int out1 = 0; out1 < 2; ++out1) {
        for (int out2 = 0; out2 < 2; ++out2) {
          Complex sum(0, 0);
          for (int in1 = 0; in1 < 2; ++in1) {
            for (int in2 = 0; in2 < 2; ++in2) {
              int row = swap_indices ? (out2 * 2 + out1) : (out1 * 2 + out2);
              int col = swap_indices ? (in2 * 2 + in1) : (in1 * 2 + in2);
              sum += matrix[row * 4 + col] *
                     C[l * (4 * R2) + in1 * (2 * R2) + in2 * R2 + r];
            }
          }
          C_new[l * (4 * R2) + out1 * (2 * R2) + out2 * R2 + r] = sum;
        }
      }
    }
  }

  // 3. Reshape into matrix (L1*2) x (2*R2) for SVD
  int rows = L1 * 2;
  int cols = 2 * R2;
  Eigen::MatrixXcd M(rows, cols);
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      M(i, j) = C_new[i * cols + j];
    }
  }

  // 4. Perform SVD
  // Decompose the intermediate tensor M into U * S * V^\dagger.
  // U represents the new left tensor block, V^\dagger represents the right.
  // The singular values S represent the entanglement spectrum across the bond.
  Eigen::JacobiSVD<Eigen::MatrixXcd> svd(M, Eigen::ComputeThinU |
                                                Eigen::ComputeThinV);
  Eigen::VectorXd S = svd.singularValues();
  Eigen::MatrixXcd U = svd.matrixU();
  Eigen::MatrixXcd V =
      svd.matrixV(); // this is V, not V^dagger. SVD is M = U * S * V.adjoint()

  // 5. Truncate singular values
  // To prevent the bond dimension (D) from growing exponentially to 2^N,
  // we discard singular values below a certain threshold or beyond the max_bond_dimension.
  // This effectively compresses the state by discarding low-weight entanglement branches.
  int D_new = 0;
  double threshold = 1e-6;
  for (int i = 0; i < S.size(); ++i) {
    if (S(i) > threshold) {
      D_new++;
    }
  }
  D_new = std::min(D_new, max_bond_dimension);
  if (D_new == 0)
    D_new = 1;

  // 6. Absorb S into V^\dagger (or V.adjoint())
  // The new bond connects U_trunc and (S_trunc * V_adj_trunc).
  // Absorbing S into the right side pushes the canonical center to the right.
  Eigen::MatrixXcd U_trunc = U.leftCols(D_new);
  Eigen::MatrixXcd S_trunc = S.head(D_new).asDiagonal();
  Eigen::MatrixXcd V_adj_trunc = V.leftCols(D_new).adjoint(); // (D_new x cols)
  Eigen::MatrixXcd VS_trunc = S_trunc * V_adj_trunc;

  // 7. Update tensors
  nodeL.right_dim = D_new;
  nodeL.data.resize(L1 * 2 * D_new);
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < D_new; ++j) {
      nodeL.data[i * D_new + j] = U_trunc(i, j);
    }
  }

  nodeR.left_dim = D_new;
  nodeR.data.resize(D_new * 2 * R2);
  for (int i = 0; i < D_new; ++i) {
    for (int j = 0; j < cols; ++j) {
      nodeR.data[i * cols + j] = VS_trunc(i, j);
    }
  }
}

void MPSBackend::contractAndTruncate(size_t left_qubit) {
  // Deprecated, logic embedded directly in applyTwoQubitGate to process tensor
  // natively.
}

void MPSBackend::applyHadamard(size_t target) {
  double inv_sqrt2 = 1.0 / std::sqrt(2.0);
  std::vector<Complex> H = {Complex(inv_sqrt2, 0), Complex(inv_sqrt2, 0),
                            Complex(inv_sqrt2, 0), Complex(-inv_sqrt2, 0)};
  applySingleQubitGate(target, H);
}

void MPSBackend::applyX(size_t target) {
  std::vector<Complex> X = {Complex(0, 0), Complex(1, 0), Complex(1, 0),
                            Complex(0, 0)};
  applySingleQubitGate(target, X);
}

void MPSBackend::applyY(size_t target) {
  std::vector<Complex> Y = {Complex(0, 0), Complex(0, -1), Complex(0, 1),
                            Complex(0, 0)};
  applySingleQubitGate(target, Y);
}

void MPSBackend::applyZ(size_t target) {
  std::vector<Complex> Z = {Complex(1, 0), Complex(0, 0), Complex(0, 0),
                            Complex(-1, 0)};
  applySingleQubitGate(target, Z);
}

void MPSBackend::applyCNOT(size_t control, size_t target) {
  // 4x4 Matrix
  std::vector<Complex> CNOT = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0};
  applyTwoQubitGate(control, target, CNOT);
}

void MPSBackend::applyToffoli(size_t control1, size_t control2, size_t target) {
  throw std::runtime_error("Toffoli not natively supported in MPS prototype");
}

void MPSBackend::applyPhaseS(size_t target) {
  std::vector<Complex> S = {1, 0, 0, Complex(0, 1)};
  applySingleQubitGate(target, S);
}

void MPSBackend::applyPhaseT(size_t target) {
  double inv_sqrt2 = 1.0 / std::sqrt(2.0);
  std::vector<Complex> T = {1, 0, 0, Complex(inv_sqrt2, inv_sqrt2)};
  applySingleQubitGate(target, T);
}

void MPSBackend::applyRotationX(size_t target, Precision angle) {
  double half = angle / 2.0;
  std::vector<Complex> RX = {std::cos(half), Complex(0, -std::sin(half)),
                             Complex(0, -std::sin(half)), std::cos(half)};
  applySingleQubitGate(target, RX);
}

void MPSBackend::applyRotationY(size_t target, Precision angle) {
  double half = angle / 2.0;
  std::vector<Complex> RY = {std::cos(half), -std::sin(half), std::sin(half),
                             std::cos(half)};
  applySingleQubitGate(target, RY);
}

void MPSBackend::applyRotationZ(size_t target, Precision angle) {
  double half = angle / 2.0;
  std::vector<Complex> RZ = {std::polar(1.0, -half), 0, 0,
                             std::polar(1.0, half)};
  applySingleQubitGate(target, RZ);
}

void MPSBackend::applySWAP(size_t qubit1, size_t qubit2) {
  std::vector<Complex> SWAP = {1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1};
  applyTwoQubitGate(qubit1, qubit2, SWAP);
}

void MPSBackend::applyCZ(size_t control, size_t target) {
  std::vector<Complex> CZ = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1};
  applyTwoQubitGate(control, target, CZ);
}

// --- Density Matrix Helpers ---

std::array<Complex, 4> MPSBackend::getReducedDensityMatrix1Q(size_t target) const {
  const auto& tensor = nodes[target];
  Complex r00(0,0), r01(0,0), r10(0,0), r11(0,0);
  int L = tensor.left_dim;
  int R = tensor.right_dim;
  for (int l = 0; l < L; ++l) {
    for (int r = 0; r < R; ++r) {
      Complex a0 = tensor.data[l * 2 * R + 0 * R + r];
      Complex a1 = tensor.data[l * 2 * R + 1 * R + r];
      r00 += a0 * std::conj(a0);
      r01 += a0 * std::conj(a1);
      r10 += a1 * std::conj(a0);
      r11 += a1 * std::conj(a1);
    }
  }
  return {r00, r01, r10, r11};
}

std::array<Complex, 16> MPSBackend::getReducedDensityMatrix2Q(size_t q1, size_t q2) const {
  if (std::abs((int)q1 - (int)q2) != 1) {
    throw std::runtime_error("MPS two-qubit density matrix requires adjacent qubits.");
  }
  size_t left = std::min(q1, q2);
  bool swap_indices = (q1 > q2);
  
  const auto& nodeL = nodes[left];
  const auto& nodeR = nodes[left + 1];
  
  int L1 = nodeL.left_dim;
  int D = nodeL.right_dim;
  int R2 = nodeR.right_dim;
  
  std::vector<Complex> C(L1 * 4 * R2, Complex(0,0));
  for (int l = 0; l < L1; ++l) {
    for (int p1 = 0; p1 < 2; ++p1) {
      for (int p2 = 0; p2 < 2; ++p2) {
        for (int r = 0; r < R2; ++r) {
          Complex sum(0,0);
          for (int d = 0; d < D; ++d) {
            sum += nodeL.data[l * 2 * D + p1 * D + d] * nodeR.data[d * 2 * R2 + p2 * R2 + r];
          }
          C[l * 4 * R2 + p1 * 2 * R2 + p2 * R2 + r] = sum;
        }
      }
    }
  }
  
  std::array<Complex, 16> rdm{};
  rdm.fill(Complex(0,0));
  
  for (int l = 0; l < L1; ++l) {
    for (int r = 0; r < R2; ++r) {
      for (int p1 = 0; p1 < 2; ++p1) {
        for (int p2 = 0; p2 < 2; ++p2) {
          for (int p1_prime = 0; p1_prime < 2; ++p1_prime) {
            for (int p2_prime = 0; p2_prime < 2; ++p2_prime) {
              int row = swap_indices ? (p2 * 2 + p1) : (p1 * 2 + p2);
              int col = swap_indices ? (p2_prime * 2 + p1_prime) : (p1_prime * 2 + p2_prime);
              Complex a = C[l * 4 * R2 + p1 * 2 * R2 + p2 * R2 + r];
              Complex b = C[l * 4 * R2 + p1_prime * 2 * R2 + p2_prime * R2 + r];
              rdm[row * 4 + col] += a * std::conj(b);
            }
          }
        }
      }
    }
  }
  return rdm;
}

// --- Noise ---

void MPSBackend::applyDepolarizingNoise(Precision p) {
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<> dis(0.0, 1.0);
  for (size_t i = 0; i < num_qubits; ++i) {
    if (dis(gen) < p) {
      double type = dis(gen);
      if (type < 0.333) applyX(i);
      else if (type < 0.666) applyY(i);
      else applyZ(i);
    }
  }
}

void MPSBackend::applyNoiseChannel1Q(const NoiseChannel1Q& channel, size_t target) {
  if (channel.operators.empty()) return;
  auto rdm = getReducedDensityMatrix1Q(target);
  
  std::vector<Precision> probs;
  probs.reserve(channel.operators.size());
  Precision total_p = 0.0;
  
  for (const auto& op : channel.operators) {
    const auto& M = op.matrix_dag_self;
    Complex tr = M[0] * rdm[0] + M[1] * rdm[2] + M[2] * rdm[1] + M[3] * rdm[3];
    Precision p = std::abs(tr.real());
    probs.push_back(p);
    total_p += p;
  }
  
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<Precision> dis(0.0, total_p);
  Precision r = dis(gen);
  
  const KrausOperator1Q* selected = &channel.operators.back();
  Precision selected_prob = probs.back();
  Precision cum = 0.0;
  for (size_t i = 0; i < channel.operators.size(); ++i) {
    cum += probs[i];
    if (r <= cum) {
      selected = &channel.operators[i];
      selected_prob = probs[i];
      break;
    }
  }
  
  if (selected_prob < 1e-20) return;
  
  std::vector<Complex> m(selected->matrix.begin(), selected->matrix.end());
  applySingleQubitGate(target, m);
  
  Precision inv_norm = 1.0 / std::sqrt(selected_prob);
  auto& tensor = nodes[target];
  for (auto& val : tensor.data) {
    val *= inv_norm;
  }
}

void MPSBackend::applyNoiseChannel2Q(const NoiseChannel2Q& channel, size_t q1, size_t q2) {
  if (channel.operators.empty()) return;
  auto rdm = getReducedDensityMatrix2Q(q1, q2);
  
  std::vector<Precision> probs;
  probs.reserve(channel.operators.size());
  Precision total_p = 0.0;
  
  for (const auto& op : channel.operators) {
    const auto& M = op.matrix_dag_self;
    Complex tr(0,0);
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        tr += M[r * 4 + c] * rdm[c * 4 + r];
      }
    }
    Precision p = std::abs(tr.real());
    probs.push_back(p);
    total_p += p;
  }
  
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<Precision> dis(0.0, total_p);
  Precision r = dis(gen);
  
  const KrausOperator2Q* selected = &channel.operators.back();
  Precision selected_prob = probs.back();
  Precision cum = 0.0;
  for (size_t i = 0; i < channel.operators.size(); ++i) {
    cum += probs[i];
    if (r <= cum) {
      selected = &channel.operators[i];
      selected_prob = probs[i];
      break;
    }
  }
  
  if (selected_prob < 1e-20) return;
  
  std::vector<Complex> m(selected->matrix.begin(), selected->matrix.end());
  applyTwoQubitGate(q1, q2, m);
  
  Precision inv_norm = 1.0 / std::sqrt(selected_prob);
  auto& tensorL = nodes[std::min(q1, q2)];
  for (auto& val : tensorL.data) {
    val *= inv_norm;
  }
}

int MPSBackend::measure(size_t target) {
  auto rdm = getReducedDensityMatrix1Q(target);
  Precision prob0 = std::abs(rdm[0].real());
  
  thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<> dis(0.0, 1.0);
  int outcome = (dis(gen) > prob0) ? 1 : 0;
  
  std::vector<Complex> proj;
  if (outcome == 0) {
    proj = {1.0, 0.0, 0.0, 0.0};
  } else {
    proj = {0.0, 0.0, 0.0, 1.0};
  }
  applySingleQubitGate(target, proj);
  
  Precision norm = (outcome == 0) ? std::sqrt(prob0) : std::sqrt(1.0 - prob0);
  if (norm > 1e-9) {
    auto& tensor = nodes[target];
    for (auto& val : tensor.data) {
      val /= norm;
    }
  }
  return outcome;
}

std::vector<double> MPSBackend::getProbabilities() const {
  if (num_qubits > 25) {
    throw std::runtime_error("Cannot return full 2^N probabilities for > 25 qubits.");
  }
  
  size_t dim = 1ULL << num_qubits;
  std::vector<double> probs(dim, 0.0);
  
  std::vector<Complex> state = {1.0};
  for (int q = 0; q < num_qubits; ++q) {
    const auto& tensor = nodes[q];
    int L = tensor.left_dim;
    int R = tensor.right_dim;
    std::vector<Complex> new_state(R * (1ULL << (q + 1)), Complex(0,0));
    
    for (size_t s = 0; s < (1ULL << q); ++s) {
      for (int l = 0; l < L; ++l) {
        Complex v = state[l * (1ULL << q) + s];
        for (int p = 0; p < 2; ++p) {
          for (int r = 0; r < R; ++r) {
            new_state[r * (1ULL << (q + 1)) + s + (p << q)] += v * tensor.data[l * 2 * R + p * R + r];
          }
        }
      }
    }
    state = std::move(new_state);
  }
  
  int k = 100;
  std::vector<std::pair<double, size_t>> prob_pairs;
  for (size_t i = 0; i < dim; ++i) {
    double p = std::norm(state[i]);
    if (p > 1e-10) {
      prob_pairs.push_back({p, i});
    }
  }
  
  std::sort(prob_pairs.begin(), prob_pairs.end(), [](const auto& a, const auto& b) {
    return a.first > b.first;
  });
  
  for (size_t i = 0; i < std::min((size_t)k, prob_pairs.size()); ++i) {
    probs[prob_pairs[i].second] = prob_pairs[i].first;
  }
  
  return probs;
}
void MPSBackend::applyDenseUnitary(const std::vector<size_t> &targets,
                                   const std::vector<Complex> &matrix) {
  throw FeatureNotSupportedException(
      "applyDenseUnitary not supported in MPS prototype.");
}

double MPSBackend::expectationValue(const std::string &pauli_string) const {
  return 0.0;
}

std::vector<Complex> MPSBackend::getStateVector() const {
  // Requires full contraction of the tensor network: O(2^N)
  // Only feasible for small N or if we explicitly want to blow up memory.
  if (num_qubits > 30) {
    throw std::runtime_error(
        "Cannot expand state vector > 30 qubits due to O(2^N) memory limit!");
  }

  // Contract all tensors sequentially...
  // (Stubbed returning empty for prototype)
  return {};
}

} // namespace qubit_engine
