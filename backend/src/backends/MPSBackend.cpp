#include "MPSBackend.hpp"
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
  // Only support adjacent qubits for this prototype architecture
  if (std::abs((int)q1 - (int)q2) != 1) {
    throw std::runtime_error("MPS two-qubit gates currently require adjacent "
                             "qubits. SWAP network needed.");
  }

  size_t left = std::min(q1, q2);
  // 1. Contract nodes[left] and nodes[left+1] into a (left_dim, 4, right_dim)
  // tensor
  // 2. Apply 4x4 gate matrix
  // 3. Perform SVD to split back into two nodes
  // 4. Truncate singular values if rank exceeds max_bond_dimension

  // Stubbed out for prototype
  contractAndTruncate(left);
}

void MPSBackend::contractAndTruncate(size_t left_qubit) {
  // Concept:
  // SVD stub to demonstrate structure. In production, link LAPACK/Eigen/MKL.
  // By aggressively truncating singular values below 1e-6, we maintain sparse
  // representation.
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

void MPSBackend::applyDepolarizingNoise(Precision probability) {
  // Not strictly supported cleanly in pure state MPS without going to MPO
  // (Matrix Product Operators)
}

int MPSBackend::measure(size_t target) { return 0; }
std::vector<double> MPSBackend::getProbabilities() { return {}; }
double MPSBackend::expectationValue(const std::string &pauli_string) {
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
