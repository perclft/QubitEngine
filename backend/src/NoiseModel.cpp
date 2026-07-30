#include "NoiseModel.hpp"
#include "HardwareConfig.hpp"

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

/// @brief Compute K†K for a 2×2 matrix
static std::array<Complex, 4> computeDagSelf2x2(const std::array<Complex, 4>& m) {
  // M† = [[conj(m00), conj(m10)], [conj(m01), conj(m11)]]
  // Result = M† * M
  Complex m00 = m[0], m01 = m[1], m10 = m[2], m11 = m[3];
  Complex d00 = std::conj(m00), d01 = std::conj(m10), d10 = std::conj(m01), d11 = std::conj(m11);
  
  return {
    d00 * m00 + d01 * m10, d00 * m01 + d01 * m11,
    d10 * m00 + d11 * m10, d10 * m01 + d11 * m11
  };
}

/// @brief Compute K†K for a 4×4 matrix
static std::array<Complex, 16> computeDagSelf4x4(const std::array<Complex, 16>& m) {
  std::array<Complex, 16> result{};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      Complex sum(0, 0);
      for (int k = 0; k < 4; ++k) {
        // (M†)ik = conj(Mki)
        sum += std::conj(m[k * 4 + i]) * m[k * 4 + j];
      }
      result[i * 4 + j] = sum;
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
  if (p < 0.0 || p > 0.75) {
    throw std::invalid_argument(
        "Depolarizing probability 1Q must be in [0, 0.75], got " + std::to_string(p));
  }

  NoiseChannel1Q channel;
  channel.name = "depolarizing_1q";

  Precision sqrt_1mp = std::sqrt(1.0 - p);
  Precision sqrt_p3 = std::sqrt(p / 3.0);

  channel.operators.push_back({1.0 - p, scale2x2(sqrt_1mp, PAULI_I), computeDagSelf2x2(scale2x2(sqrt_1mp, PAULI_I))});
  channel.operators.push_back({p / 3.0, scale2x2(sqrt_p3, PAULI_X), computeDagSelf2x2(scale2x2(sqrt_p3, PAULI_X))});
  channel.operators.push_back({p / 3.0, scale2x2(sqrt_p3, PAULI_Y), computeDagSelf2x2(scale2x2(sqrt_p3, PAULI_Y))});
  channel.operators.push_back({p / 3.0, scale2x2(sqrt_p3, PAULI_Z), computeDagSelf2x2(scale2x2(sqrt_p3, PAULI_Z))});

  return channel;
}

// ============================================================================
// Channel Factory: Two-Qubit Depolarizing (Full 16-operator)
// ============================================================================

NoiseChannel2Q makeDepolarizingChannel2Q(Precision p) {
  if (p < 0.0 || p > 15.0 / 16.0) {
    throw std::invalid_argument(
        "Depolarizing probability 2Q must be in [0, 15/16], got " + std::to_string(p));
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
        auto mat = scale4x4(sqrt_1mp, kron);
        channel.operators.push_back({1.0 - p, mat, computeDagSelf4x4(mat)});
      } else {
        // Non-identity Pauli product: weight = p / 15
        auto mat = scale4x4(sqrt_p15, kron);
        channel.operators.push_back({p / 15.0, mat, computeDagSelf4x4(mat)});
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
  k0.probability = 1.0 - gamma; 
  k0.matrix = {Complex(1, 0), Complex(0, 0),
               Complex(0, 0), Complex(sqrt_1mg, 0)};
  k0.matrix_dag_self = computeDagSelf2x2(k0.matrix);

  // K1 = [[0, sqrt(γ)], [0, 0]]
  KrausOperator1Q k1;
  k1.probability = gamma;
  k1.matrix = {Complex(0, 0), Complex(sqrt_g, 0),
               Complex(0, 0), Complex(0, 0)};
  k1.matrix_dag_self = computeDagSelf2x2(k1.matrix);

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
  k0.matrix_dag_self = computeDagSelf2x2(k0.matrix);

  // K1 = [[0, 0], [0, sqrt(γ)]]
  KrausOperator1Q k1;
  k1.probability = gamma;
  k1.matrix = {Complex(0, 0), Complex(0, 0),
               Complex(0, 0), Complex(sqrt_g, 0)};
  k1.matrix_dag_self = computeDagSelf2x2(k1.matrix);

  channel.operators.push_back(k0);
  channel.operators.push_back(k1);

  return channel;
}

// ============================================================================
// Channel Factory: Thermal Relaxation
// ============================================================================

NoiseChannel1Q makeThermalRelaxationChannel(Precision t1, Precision t2, Precision gate_time) {
  if (t1 <= 0.0 || t2 <= 0.0 || gate_time < 0.0) {
    throw std::invalid_argument("T1, T2 must be positive and gate_time non-negative");
  }
  if (t2 > 2.0 * t1) {
    throw std::invalid_argument("T2 must satisfy T2 <= 2*T1 for physical relaxation, got T1=" + 
                                std::to_string(t1) + ", T2=" + std::to_string(t2));
  }

  // Calculate probabilities for AD and PD components
  Precision gamma_ad = 1.0 - std::exp(-gate_time / t1);
  // Pure dephasing rate: 1/T_phi = 1/T2 - 1/(2*T1)
  // Gamma_pd = 1 - exp(-2 * gate_time / T_phi)
  Precision gamma_pd = 1.0 - std::exp(-(2.0 / t2 - 1.0 / t1) * gate_time);

  // Combine AD and PD channels (AD then PD)
  // This results in 4 operators: K_ij = K_pd_i * K_ad_j
  auto ad = makeAmplitudeDampingChannel(gamma_ad);
  auto pd = makePhaseDampingChannel(gamma_pd);

  NoiseChannel1Q channel;
  channel.name = "thermal_relaxation";

  for (auto& k_pd : pd.operators) {
    for (auto& k_ad : ad.operators) {
      KrausOperator1Q combined;
      // Probability here is just for initialization/ordering
      combined.probability = k_pd.probability * k_ad.probability;
      
      // Matrix multiplication: M_pd * M_ad
      for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
          Complex sum(0, 0);
          for (int k = 0; k < 2; ++k) {
            sum += k_pd.matrix[i * 2 + k] * k_ad.matrix[k * 2 + j];
          }
          combined.matrix[i * 2 + j] = sum;
        }
      }
      combined.matrix_dag_self = computeDagSelf2x2(combined.matrix);
      channel.operators.push_back(combined);
    }
  }

  return channel;
}

static void validateChannel1Q(const NoiseChannel1Q& channel) {
  if (channel.operators.empty()) return;
  std::array<Complex, 4> sum_dag_self{};
  for (const auto& op : channel.operators) {
    for (int k = 0; k < 4; ++k) {
      sum_dag_self[k] += op.matrix_dag_self[k];
    }
  }
  double err = std::abs(sum_dag_self[0] - Complex(1, 0)) +
               std::abs(sum_dag_self[3] - Complex(1, 0)) +
               std::abs(sum_dag_self[1]) + std::abs(sum_dag_self[2]);
  if (err > 1e-3) {
    throw std::invalid_argument("NoiseChannel1Q " + channel.name +
                                " violates Kraus completeness relation (sum K_i^dag K_i != I).");
  }
}

static void validateChannel2Q(const NoiseChannel2Q& channel) {
  if (channel.operators.empty()) return;
  std::array<Complex, 16> sum_dag_self{};
  for (const auto& op : channel.operators) {
    for (int k = 0; k < 16; ++k) {
      sum_dag_self[k] += op.matrix_dag_self[k];
    }
  }
  double err = 0.0;
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      Complex expected = (r == c) ? Complex(1, 0) : Complex(0, 0);
      err += std::abs(sum_dag_self[r * 4 + c] - expected);
    }
  }
  if (err > 1e-3) {
    throw std::invalid_argument("NoiseChannel2Q " + channel.name +
                                " violates Kraus completeness relation (sum K_i^dag K_i != I).");
  }
}

// ============================================================================
// NoiseModel — Configuration API
// ============================================================================

void NoiseModel::addSingleQubitNoise(NoiseChannel1Q channel) {
  validateChannel1Q(channel);
  single_qubit_channels_.push_back(std::move(channel));
}

void NoiseModel::addTwoQubitNoise(NoiseChannel2Q channel) {
  validateChannel2Q(channel);
  two_qubit_channels_.push_back(std::move(channel));
}

void NoiseModel::addSingleQubitNoise(size_t qubit, NoiseChannel1Q channel) {
  validateChannel1Q(channel);
  per_qubit_channels_[qubit].push_back(std::move(channel));
}

void NoiseModel::addTwoQubitNoise(size_t q1, size_t q2, NoiseChannel2Q channel) {
  // Create swapped channel for the reverse edge (q2, q1)
  NoiseChannel2Q reversed_channel = channel;
  reversed_channel.name += "_swapped";

  // Permutation vector for swapping the two qubits (tensor product basis swapping)
  const int P[4] = {0, 2, 1, 3};
  for (auto& op : reversed_channel.operators) {
      std::array<Complex, 16> swapped_matrix;
      for (int r = 0; r < 4; ++r) {
          for (int c = 0; c < 4; ++c) {
              swapped_matrix[r * 4 + c] = op.matrix[P[r] * 4 + P[c]];
          }
      }
      op.matrix = swapped_matrix;
      op.matrix_dag_self = computeDagSelf4x4(swapped_matrix);
  }

  per_edge_channels_[{q1, q2}].push_back(std::move(channel));
  per_edge_channels_[{q2, q1}].push_back(std::move(reversed_channel));
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

void NoiseModel::setCoherentError(int gate_type_int, Precision epsilon) {
  coherent_errors_[gate_type_int] = epsilon;
}

// ============================================================================
// NoiseModel — Query API
// ============================================================================

bool NoiseModel::isEnabled() const {
  return enabled_ && (!single_qubit_channels_.empty() ||
                       !two_qubit_channels_.empty() || 
                       !per_qubit_channels_.empty() || 
                       !per_edge_channels_.empty() || 
                       has_readout_);
}

const std::vector<NoiseChannel1Q>& NoiseModel::getSingleQubitChannels() const {
  return single_qubit_channels_;
}

const std::vector<NoiseChannel2Q>& NoiseModel::getTwoQubitChannels() const {
  return two_qubit_channels_;
}

const std::vector<NoiseChannel1Q>& NoiseModel::getSingleQubitChannels(size_t qubit) const {
  auto it = per_qubit_channels_.find(qubit);
  if (it != per_qubit_channels_.end()) {
    return it->second;
  }
  static const std::vector<NoiseChannel1Q> empty;
  return empty;
}

const std::vector<NoiseChannel2Q>& NoiseModel::getTwoQubitChannels(size_t q1, size_t q2) const {
  auto it = per_edge_channels_.find({q1, q2});
  if (it != per_edge_channels_.end()) {
    return it->second;
  }
  static const std::vector<NoiseChannel2Q> empty;
  return empty;
}

bool NoiseModel::hasReadoutError() const { return has_readout_; }

ReadoutError NoiseModel::getReadoutError(size_t qubit) const {
  auto it = per_qubit_readout_.find(qubit);
  if (it != per_qubit_readout_.end()) {
    return it->second;
  }
  return default_readout_;
}

Precision NoiseModel::getCoherentError(int gate_type_int) const {
  auto it = coherent_errors_.find(gate_type_int);
  if (it != coherent_errors_.end()) {
    return it->second;
  }
  return 0.0;
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

NoiseModel NoiseModel::FromCalibration(const DeviceCalibration& cal) {
  NoiseModel model;
  
  // Add per-qubit noise (T1, T2, readout, 1Q depolarizing)
  for (size_t i = 0; i < cal.qubit_calibrations.size(); ++i) {
    const auto& qcal = cal.qubit_calibrations[i];
    
    if (qcal.t1_us > 0.0 && qcal.t2_us > 0.0) {
      // gate_time is in ns, convert to us
      double gate_time_us = cal.single_qubit_gate_time_ns / 1000.0;
      model.addSingleQubitNoise(i, makeThermalRelaxationChannel(qcal.t1_us, qcal.t2_us, gate_time_us));
    }
    
    if (qcal.gate_error_1q > 0.0) {
      model.addSingleQubitNoise(i, makeDepolarizingChannel1Q(qcal.gate_error_1q));
    }
    
    if (qcal.readout_error > 0.0) {
      ReadoutError err;
      err.p0_given_1 = qcal.readout_error;
      err.p1_given_0 = qcal.readout_error;
      model.setReadoutError(i, err);
    }
  }
  
  // Add per-edge noise (2Q depolarizing)
  for (const auto& ccal : cal.coupler_calibrations) {
    if (ccal.cx_error > 0.0) {
      model.addTwoQubitNoise(ccal.qubit1, ccal.qubit2, makeDepolarizingChannel2Q(ccal.cx_error));
    }
  }
  
  return model;
}

NoiseModel NoiseModel::IBMBrisbane() {
    return FromCalibration(HardwareConfig::ibmBrisbane());
}

NoiseModel NoiseModel::GoogleSycamore() {
    return FromCalibration(HardwareConfig::googleSycamore());
}

} // namespace qubit_engine
