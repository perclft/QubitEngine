#pragma once

#include "Types.hpp"
#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

namespace qubit_engine {

// ============================================================================
// Kraus Operator Representation
// ============================================================================

/// @brief A single Kraus operator for a 1-qubit channel.
/// The matrix is 2×2, stored in row-major order: [M00, M01, M10, M11].
struct KrausOperator1Q {
  Precision probability;              // Initial selection weight (||K_i||² / dim)
  std::array<Complex, 4> matrix;      // 2×2 row-major
  std::array<Complex, 4> matrix_dag_self; // K†K (Hermitian), precomputed
};

/// @brief A single Kraus operator for a 2-qubit channel.
struct KrausOperator2Q {
  Precision probability;              // Initial selection weight
  std::array<Complex, 16> matrix;     // 4×4 row-major
  std::array<Complex, 16> matrix_dag_self; // K†K, precomputed
};

// ============================================================================
// Noise Channels
// ============================================================================

/// @brief A single-qubit quantum noise channel defined by a set of Kraus operators.
struct NoiseChannel1Q {
  std::string name;
  std::vector<KrausOperator1Q> operators;
};

/// @brief A two-qubit quantum noise channel defined by a set of Kraus operators.
struct NoiseChannel2Q {
  std::string name;
  std::vector<KrausOperator2Q> operators;
};

// ============================================================================
// Channel Factory Functions
// ============================================================================

/// @brief Creates a single-qubit depolarizing channel.
NoiseChannel1Q makeDepolarizingChannel1Q(Precision p);

/// @brief Creates a full two-qubit depolarizing channel using 16 Pauli products.
NoiseChannel2Q makeDepolarizingChannel2Q(Precision p);

/// @brief Creates a single-qubit amplitude damping channel (T1 decay).
NoiseChannel1Q makeAmplitudeDampingChannel(Precision gamma);

/// @brief Creates a single-qubit phase damping channel (T2 dephasing).
NoiseChannel1Q makePhaseDampingChannel(Precision gamma);

/// @brief Creates a single-qubit thermal relaxation channel (combined T1/T2).
/// @param t1 Longitudinal relaxation time
/// @param t2 Transverse relaxation time (must satisfy T2 <= 2*T1)
/// @param gate_time Duration of the gate in the same units as T1, T2
NoiseChannel1Q makeThermalRelaxationChannel(Precision t1, Precision t2, Precision gate_time);

// ============================================================================
// Readout Error
// ============================================================================

/// @brief Per-qubit measurement confusion matrix.
struct ReadoutError {
  Precision p0_given_1 = 0.0;   ///< P(readout=0 | true state is |1⟩)
  Precision p1_given_0 = 0.0;   ///< P(readout=1 | true state is |0⟩)
};

// ============================================================================
// NoiseModel — Top-Level Configuration
// ============================================================================

/// @brief Composable noise model that holds all noise channels and readout errors.
class NoiseModel {
public:
  NoiseModel() = default;

  // --- Configuration API ---

  void addSingleQubitNoise(NoiseChannel1Q channel);
  void addTwoQubitNoise(NoiseChannel2Q channel);
  void addSingleQubitNoise(size_t qubit, NoiseChannel1Q channel);
  void addTwoQubitNoise(size_t q1, size_t q2, NoiseChannel2Q channel);
  void setReadoutError(size_t qubit, ReadoutError error);
  void setReadoutErrorAll(ReadoutError error);
  void setEnabled(bool enabled);

  /// @brief Adds a coherent error (systematic rotation bias) for a specific gate type.
  /// @param gate_type_int The integer enum value from Protobuf (GateType)
  /// @param epsilon The angle bias (added to the gate angle)
  void setCoherentError(int gate_type_int, Precision epsilon);

  // --- Query API (used by backends) ---

  [[nodiscard]] bool isEnabled() const;
  [[nodiscard]] const std::vector<NoiseChannel1Q>& getSingleQubitChannels() const;
  [[nodiscard]] const std::vector<NoiseChannel2Q>& getTwoQubitChannels() const;
  [[nodiscard]] const std::vector<NoiseChannel1Q>& getSingleQubitChannels(size_t qubit) const;
  [[nodiscard]] const std::vector<NoiseChannel2Q>& getTwoQubitChannels(size_t q1, size_t q2) const;
  [[nodiscard]] bool hasReadoutError() const;
  [[nodiscard]] ReadoutError getReadoutError(size_t qubit) const;

  /// @brief Returns the coherent error bias for a gate type. Returns 0.0 if not set.
  [[nodiscard]] Precision getCoherentError(int gate_type_int) const;

  // --- Convenience Builders ---

  static NoiseModel Depolarizing(Precision p1q, Precision p2q);
  static NoiseModel Realistic(Precision p1q, Precision p2q,
                                Precision t1_gamma, Precision t2_gamma,
                                ReadoutError readout);
  
  static NoiseModel IBMBrisbane();
  static NoiseModel GoogleSycamore();
  
  // Forward declare DeviceCalibration since we didn't include HardwareConfig.hpp
  // Actually, we can just use the struct name
  static NoiseModel FromCalibration(const struct DeviceCalibration& cal);

private:
  bool enabled_ = true;
  std::vector<NoiseChannel1Q> single_qubit_channels_;
  std::vector<NoiseChannel2Q> two_qubit_channels_;
  
  struct PairHash {
      template <class T1, class T2>
      std::size_t operator () (const std::pair<T1,T2> &p) const {
          auto h1 = std::hash<T1>{}(p.first);
          auto h2 = std::hash<T2>{}(p.second);
          return h1 ^ (h2 << 1);
      }
  };
  std::unordered_map<size_t, std::vector<NoiseChannel1Q>> per_qubit_channels_;
  std::unordered_map<std::pair<size_t, size_t>, std::vector<NoiseChannel2Q>, PairHash> per_edge_channels_;

  std::unordered_map<size_t, ReadoutError> per_qubit_readout_;
  std::unordered_map<int, Precision> coherent_errors_;
  ReadoutError default_readout_;
  bool has_readout_ = false;
};

} // namespace qubit_engine
