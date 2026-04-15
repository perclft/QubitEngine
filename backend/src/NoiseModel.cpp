#include "NoiseModel.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace qubit_engine {

// ============================================================================
// Helper: Pauli Matrices (2×2, row-major)
// ============================================================================

static constexpr std::array<Complex, 4> PAULI_I = {
    Complex(1, 0), Complex(0, 0),
    Complex(0, 0), Complex(1, 0)};

static constexpr std::array<Complex, 4> PAULI_X = {
    Complex(0, 0), Complex(1, 0),
    Complex(1, 0), Complex(0, 0)};

static constexpr std::array<Complex, 4> PAULI_Y = {
    Complex(0, 0), Complex(0, -1),
    Complex(0, 1), Complex(0, 0)};

static constexpr std::array<Complex, 4> PAULI_Z = {
    Complex(1, 0), Complex(0, 0),
    Complex(0, 0), Complex(-1, 0)};

/// @brief Scale a 2×2 matrix by a scalar
static std::array<Complex, 4> scale2x2(Precision s,
                                        const std::array<Complex, 4>& m) {
  return {s * m[0], s * m[1], s * m[2], s * m[3]};
}

/// @brief Compute the tensor product (Kronecker product) of two 2×2 matrices.
/// Result is a 4×4 matrix in row-major order.
static std::array<Complex, 16> kronecker2x2(const std::array<Complex, 4>& A,
                                             const std::array<Complex, 4>& B) {
  std::array<Complex, 16> result{};
  // C[i*4+j] = A[ia*2+ja] * B[ib*2+jb]
  // where i = ia*2+ib, j = ja*2+jb
  for (int ia = 0; ia < 2; ++ia) {
    for (int ja = 0; ja < 2; ++ja) {
      Complex a_val = A[ia * 2 + ja];
      for (int ib = 0; ib < 2; ++ib) {
        for (int jb = 0; jb < 2; ++jb) {
          int row = ia * 2 + ib;
          int col = ja * 2 + jb;
          result[row * 4 + col] = a_val * B[ib * 2 + jb];
        }
      }
    }
  }
  return result;
}

/// @brief Scale a 4×4 matrix by a scalar
static std::array<Complex, 16> scale4x4(Precision s,
                                         const std::array<Complex, 16>& m) {
  std::array<Complex, 16> result;
  for (int i = 0; i < 16; ++i) {
    result[i] = s * m[i];
  }
  return result;
}

// ============================================================================
// Channel Factory: Single-Qubit Depolarizing
// ============================================================================

NoiseChannel1Q makeDepolarizingChannel1Q(Precision p) {
  if (p < 0.0 || p > 1.0) {
    throw std::invalid_argument(
        "Depolarizing probability must be in [0, 1], got " + std::to_string(p));
  }

  NoiseChannel1Q channel;
  channel.name = "depolarizing_1q";

  Precision sqrt_1mp = std::sqrt(1.0 - p);
  Precision sqrt_p3 = std::sqrt(p / 3.0);

  channel.operators.push_back({1.0 - p, scale2x2(sqrt_1mp, PAULI_I)});
  channel.operators.push_back({p / 3.0, scale2x2(sqrt_p3, PAULI_X)});
  channel.operators.push_back({p / 3.0, scale2x2(sqrt_p3, PAULI_Y)});
  channel.operators.push_back({p / 3.0, scale2x2(sqrt_p3, PAULI_Z)});

  return channel;
}

// ============================================================================
// Channel Factory: Two-Qubit Depolarizing (Full 16-operator)
// ============================================================================

NoiseChannel2Q makeDepolarizingChannel2Q(Precision p) {
  if (p < 0.0 || p > 1.0) {
    throw std::invalid_argument(
        "Depolarizing probability must be in [0, 1], got " + std::to_string(p));
  }

  NoiseChannel2Q channel;
  channel.name = "depolarizing_2q";

  // All 4 single-qubit Paulis
  const std::array<std::array<Complex, 4>, 4> paulis = {PAULI_I, PAULI_X,
                                                         PAULI_Y, PAULI_Z};

  Precision sqrt_1mp = std::sqrt(1.0 - p);
  Precision sqrt_p15 = std::sqrt(p / 15.0);

  for (int a = 0; a < 4; ++a) {
    for (int b = 0; b < 4; ++b) {
      auto kron = kronecker2x2(paulis[a], paulis[b]);

      if (a == 0 && b == 0) {
        // Identity ⊗ Identity: weight = 1 - p
        channel.operators.push_back({1.0 - p, scale4x4(sqrt_1mp, kron)});
      } else {
        // Non-identity Pauli product: weight = p / 15
        channel.operators.push_back({p / 15.0, scale4x4(sqrt_p15, kron)});
      }
    }
  }

  return channel;
}

// ============================================================================
// Channel Factory: Amplitude Damping (T1)
// ============================================================================

NoiseChannel1Q makeAmplitudeDampingChannel(Precision gamma) {
  if (gamma < 0.0 || gamma > 1.0) {
    throw std::invalid_argument(
        "Amplitude damping gamma must be in [0, 1], got " +
        std::to_string(gamma));
  }

  NoiseChannel1Q channel;
  channel.name = "amplitude_damping";

  Precision sqrt_1mg = std::sqrt(1.0 - gamma);
  Precision sqrt_g = std::sqrt(gamma);

  // K0 = [[1, 0], [0, sqrt(1-γ)]]
  KrausOperator1Q k0;
  k0.probability = 1.0 - gamma; // Approximate selection weight
  k0.matrix = {Complex(1, 0), Complex(0, 0),
               Complex(0, 0), Complex(sqrt_1mg, 0)};

  // K1 = [[0, sqrt(γ)], [0, 0]]
  KrausOperator1Q k1;
  k1.probability = gamma;
  k1.matrix = {Complex(0, 0), Complex(sqrt_g, 0),
               Complex(0, 0), Complex(0, 0)};

  channel.operators.push_back(k0);
  channel.operators.push_back(k1);

  return channel;
}

// ============================================================================
// Channel Factory: Phase Damping (T2 / Dephasing)
// ============================================================================

NoiseChannel1Q makePhaseDampingChannel(Precision gamma) {
  if (gamma < 0.0 || gamma > 1.0) {
    throw std::invalid_argument(
        "Phase damping gamma must be in [0, 1], got " +
        std::to_string(gamma));
  }

  NoiseChannel1Q channel;
  channel.name = "phase_damping";

  Precision sqrt_1mg = std::sqrt(1.0 - gamma);
  Precision sqrt_g = std::sqrt(gamma);

  // K0 = [[1, 0], [0, sqrt(1-γ)]]
  KrausOperator1Q k0;
  k0.probability = 1.0 - gamma;
  k0.matrix = {Complex(1, 0), Complex(0, 0),
               Complex(0, 0), Complex(sqrt_1mg, 0)};

  // K1 = [[0, 0], [0, sqrt(γ)]]
  KrausOperator1Q k1;
  k1.probability = gamma;
  k1.matrix = {Complex(0, 0), Complex(0, 0),
               Complex(0, 0), Complex(sqrt_g, 0)};

  channel.operators.push_back(k0);
  channel.operators.push_back(k1);

  return channel;
}

// ============================================================================
// NoiseModel — Configuration API
// ============================================================================

void NoiseModel::addSingleQubitNoise(NoiseChannel1Q channel) {
  single_qubit_channels_.push_back(std::move(channel));
}

void NoiseModel::addTwoQubitNoise(NoiseChannel2Q channel) {
  two_qubit_channels_.push_back(std::move(channel));
}

void NoiseModel::setReadoutError(size_t qubit, ReadoutError error) {
  per_qubit_readout_[qubit] = error;
  has_readout_ = true;
}

void NoiseModel::setReadoutErrorAll(ReadoutError error) {
  default_readout_ = error;
  has_readout_ = true;
}

void NoiseModel::setEnabled(bool enabled) { enabled_ = enabled; }

// ============================================================================
// NoiseModel — Query API
// ============================================================================

bool NoiseModel::isEnabled() const {
  return enabled_ && (!single_qubit_channels_.empty() ||
                       !two_qubit_channels_.empty() || has_readout_);
}

const std::vector<NoiseChannel1Q>& NoiseModel::getSingleQubitChannels() const {
  return single_qubit_channels_;
}

const std::vector<NoiseChannel2Q>& NoiseModel::getTwoQubitChannels() const {
  return two_qubit_channels_;
}

bool NoiseModel::hasReadoutError() const { return has_readout_; }

ReadoutError NoiseModel::getReadoutError(size_t qubit) const {
  auto it = per_qubit_readout_.find(qubit);
  if (it != per_qubit_readout_.end()) {
    return it->second;
  }
  return default_readout_;
}

// ============================================================================
// NoiseModel — Convenience Builders
// ============================================================================

NoiseModel NoiseModel::Depolarizing(Precision p1q, Precision p2q) {
  NoiseModel model;
  if (p1q > 0.0) {
    model.addSingleQubitNoise(makeDepolarizingChannel1Q(p1q));
  }
  if (p2q > 0.0) {
    model.addTwoQubitNoise(makeDepolarizingChannel2Q(p2q));
  }
  return model;
}

NoiseModel NoiseModel::Realistic(Precision p1q, Precision p2q,
                                  Precision t1_gamma, Precision t2_gamma,
                                  ReadoutError readout) {
  NoiseModel model;

  // Depolarizing channels
  if (p1q > 0.0) {
    model.addSingleQubitNoise(makeDepolarizingChannel1Q(p1q));
  }
  if (p2q > 0.0) {
    model.addTwoQubitNoise(makeDepolarizingChannel2Q(p2q));
  }

  // Amplitude damping (T1)
  if (t1_gamma > 0.0) {
    model.addSingleQubitNoise(makeAmplitudeDampingChannel(t1_gamma));
  }

  // Phase damping (T2)
  if (t2_gamma > 0.0) {
    model.addSingleQubitNoise(makePhaseDampingChannel(t2_gamma));
  }

  // Readout error
  if (readout.p0_given_1 > 0.0 || readout.p1_given_0 > 0.0) {
    model.setReadoutErrorAll(readout);
  }

  return model;
}

} // namespace qubit_engine
