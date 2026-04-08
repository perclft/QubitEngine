#include "MPSBackend.hpp"
#include <Eigen/Dense>
#include "../Exceptions.hpp"
#include <Eigen/SVD>
#include <cmath>
#include <stdexcept>


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
    throw std::runtime_error("MPS two-qubit gates currently require adjacent "
                             "qubits. SWAP network needed.");
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
  Eigen::JacobiSVD<Eigen::MatrixXcd> svd(M, Eigen::ComputeThinU |
                                                Eigen::ComputeThinV);
  Eigen::VectorXd S = svd.singularValues();
  Eigen::MatrixXcd U = svd.matrixU();
  Eigen::MatrixXcd V =
      svd.matrixV(); // this is V, not V^dagger. SVD is M = U * S * V.adjoint()

  // 5. Truncate singular values
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

int MPSBackend::measure(size_t target) { return 0; }
std::vector<double> MPSBackend::getProbabilities() const { return {}; }
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
