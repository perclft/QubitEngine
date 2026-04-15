#pragma once

#include "Types.hpp"
#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace qubit_engine {

// ============================================================================
// Kraus Operator Representation
// ============================================================================

/// @brief A single Kraus operator for a 1-qubit channel.
/// The matrix is 2×2, stored in row-major order: [M00, M01, M10, M11].
/// The probability field stores the selection probability for stochastic
/// (Monte Carlo) application: p_i = Tr(K_i ρ K_i†).
/// For initial channel construction, we store the squared norm ||K_i||² as
/// a proxy — during stochastic application the backend computes the exact
/// conditional probability from the current state.
struct KrausOperator1Q {
  Precision probability;              // Selection weight (||K_i||² / dim)
  std::array<Complex, 4> matrix;      // 2×2 row-major
};

/// @brief A single Kraus operator for a 2-qubit channel.
/// The matrix is 4×4, stored in row-major order (16 entries).
struct KrausOperator2Q {
  Precision probability;              // Selection weight
  std::array<Complex, 16> matrix;     // 4×4 row-major
};

// ============================================================================
// Noise Channels
// ============================================================================

/// @brief A single-qubit quantum noise channel defined by a set of Kraus operators.
/// Completeness relation: Σ_i K_i† K_i = I
struct NoiseChannel1Q {
  std::string name;
  std::vector<KrausOperator1Q> operators;
};

/// @brief A two-qubit quantum noise channel defined by a set of Kraus operators.
/// Completeness relation: Σ_i K_i† K_i = I
struct NoiseChannel2Q {
  std::string name;
  std::vector<KrausOperator2Q> operators;
};

// ============================================================================
// Channel Factory Functions
// ============================================================================

/// @brief Creates a single-qubit depolarizing channel.
/// With probability p, applies a uniformly random Pauli error (X, Y, or Z).
/// K0 = sqrt(1-p) * I,  K1 = sqrt(p/3) * X,  K2 = sqrt(p/3) * Y,  K3 = sqrt(p/3) * Z
/// @param p Error probability in [0, 1].
NoiseChannel1Q makeDepolarizingChannel1Q(Precision p);

/// @brief Creates a full two-qubit depolarizing channel using all 16 Pauli products.
/// With probability p, applies a uniformly random two-qubit Pauli error.
/// K0 = sqrt(1-p) * I⊗I,  K_{i>0} = sqrt(p/15) * (σ_a ⊗ σ_b) for all 15 non-identity combos.
/// @param p Error probability in [0, 1].
NoiseChannel2Q makeDepolarizingChannel2Q(Precision p);

/// @brief Creates a single-qubit amplitude damping channel (T1 decay).
/// Models energy relaxation: |1⟩ decays to |0⟩ with rate γ.
/// K0 = [[1, 0], [0, sqrt(1-γ)]],  K1 = [[0, sqrt(γ)], [0, 0]]
/// @param gamma Decay probability in [0, 1]. Related to T1 by γ = 1 - exp(-t/T1).
NoiseChannel1Q makeAmplitudeDampingChannel(Precision gamma);

/// @brief Creates a single-qubit phase damping channel (T2 dephasing).
/// Models loss of phase coherence without energy loss.
/// K0 = [[1, 0], [0, sqrt(1-γ)]],  K1 = [[0, 0], [0, sqrt(γ)]]
/// @param gamma Dephasing probability in [0, 1]. Related to T2 by γ = 1 - exp(-t/T2).
NoiseChannel1Q makePhaseDampingChannel(Precision gamma);

// ============================================================================
// Readout Error
// ============================================================================

/// @brief Per-qubit measurement confusion matrix.
/// Models classical bit-flip errors that occur during readout.
struct ReadoutError {
  Precision p0_given_1 = 0.0;   ///< P(readout=0 | true state is |1⟩)
  Precision p1_given_0 = 0.0;   ///< P(readout=1 | true state is |0⟩)
};

// ============================================================================
// NoiseModel — Top-Level Configuration
// ============================================================================

/// @brief Composable noise model that holds all noise channels and readout errors.
///
/// Usage:
///   NoiseModel model = NoiseModel::Depolarizing(0.001, 0.01);
///   qreg.setNoiseModel(model);
///   // All subsequent gate operations automatically apply noise.
///
/// Noise is applied stochastically (Monte Carlo): for each channel, one Kraus
/// operator is randomly selected and applied as a unitary transformation.
/// This maintains O(2^n) memory and gives statistically correct results when
/// averaged over many shots, which VQE already does.
class NoiseModel {
public:
  NoiseModel() = default;

  // --- Configuration API ---

  /// @brief Adds a single-qubit noise channel that will be applied after every 1Q gate.
  void addSingleQubitNoise(NoiseChannel1Q channel);

  /// @brief Adds a two-qubit noise channel that will be applied after every 2Q gate.
  void addTwoQubitNoise(NoiseChannel2Q channel);

  /// @brief Sets readout error for a specific qubit.
  void setReadoutError(size_t qubit, ReadoutError error);

  /// @brief Sets the same readout error for all qubits (used as default).
  void setReadoutErrorAll(ReadoutError error);

  /// @brief Enables or disables the noise model globally.
  void setEnabled(bool enabled);

  // --- Query API (used by backends) ---

  /// @brief Returns true if the noise model is enabled and has any noise channels.
  [[nodiscard]] bool isEnabled() const;

  /// @brief Returns all single-qubit noise channels.
  [[nodiscard]] const std::vector<NoiseChannel1Q>& getSingleQubitChannels() const;

  /// @brief Returns all two-qubit noise channels.
  [[nodiscard]] const std::vector<NoiseChannel2Q>& getTwoQubitChannels() const;

  /// @brief Returns true if any readout error is configured.
  [[nodiscard]] bool hasReadoutError() const;

  /// @brief Returns the readout error for a specific qubit (falls back to default).
  [[nodiscard]] ReadoutError getReadoutError(size_t qubit) const;

  // --- Convenience Builders ---

  /// @brief Creates a noise model with only depolarizing noise.
  /// @param p1q Single-qubit depolarizing probability (typ. ~0.001)
  /// @param p2q Two-qubit depolarizing probability (typ. ~0.01)
  static NoiseModel Depolarizing(Precision p1q, Precision p2q);

  /// @brief Creates a realistic noise model with all four error types.
  /// @param p1q Single-qubit depolarizing probability
  /// @param p2q Two-qubit depolarizing probability
  /// @param t1_gamma Amplitude damping gamma (T1 decay)
  /// @param t2_gamma Phase damping gamma (T2 dephasing)
  /// @param readout Default readout error for all qubits
  static NoiseModel Realistic(Precision p1q, Precision p2q,
                               Precision t1_gamma, Precision t2_gamma,
                               ReadoutError readout);

private:
  bool enabled_ = true;
  std::vector<NoiseChannel1Q> single_qubit_channels_;
  std::vector<NoiseChannel2Q> two_qubit_channels_;
  std::unordered_map<size_t, ReadoutError> per_qubit_readout_;
  ReadoutError default_readout_;
  bool has_readout_ = false;
};

} // namespace qubit_engine
