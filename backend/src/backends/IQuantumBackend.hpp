#pragma once

#include "Types.hpp"
#include <complex>
#include <cstddef>
#include <string>
#include <vector>

namespace qubit_engine {

class IQuantumBackend {
public:
  virtual ~IQuantumBackend() = default;

  // --- Core Gates ---
  /// @brief Applies a Hadamard gate to the target qubit, creating an equal superposition.
  /// @param target The index of the target qubit.
  virtual void applyHadamard(size_t target) = 0;
  /// @brief Applies the Pauli-X (NOT) gate to the target qubit.
  /// @param target The index of the target qubit.
  virtual void applyX(size_t target) = 0;
  
  /// @brief Applies the Pauli-Y gate to the target qubit.
  /// @param target The index of the target qubit.
  virtual void applyY(size_t target) = 0;
  
  /// @brief Applies the Pauli-Z (Phase flip) gate to the target qubit.
  /// @param target The index of the target qubit.
  virtual void applyZ(size_t target) = 0;
  
  /// @brief Applies a Controlled-NOT (CNOT) gate between two qubits.
  /// @param control The index of the control qubit.
  /// @param target The index of the target qubit.
  virtual void applyCNOT(size_t control, size_t target) = 0;

  // --- Advanced Gates ---
  /// @brief Applies a Toffoli (CCNOT) gate.
  /// @param control1 The index of the first control qubit.
  /// @param control2 The index of the second control qubit.
  /// @param target The index of the target qubit.
  virtual void applyToffoli(size_t control1, size_t control2,
                            size_t target) = 0;
                            
  /// @brief Applies the S (Phase) gate, adding a pi/2 phase.
  /// @param target The index of the target qubit.
  virtual void applyPhaseS(size_t target) = 0;
  
  /// @brief Applies the T gate, adding a pi/4 phase.
  /// @param target The index of the target qubit.
  virtual void applyPhaseT(size_t target) = 0;
  
  /// @brief Applies an arbitrary rotation around the X axis.
  /// @param target The target qubit.
  /// @param angle The rotation angle in radians.
  virtual void applyRotationX(size_t target, Precision angle) = 0;
  
  /// @brief Applies an arbitrary rotation around the Y axis.
  /// @param target The target qubit.
  /// @param angle The rotation angle in radians.
  virtual void applyRotationY(size_t target, Precision angle) = 0;
  
  /// @brief Applies an arbitrary rotation around the Z axis.
  /// @param target The target qubit.
  /// @param angle The rotation angle in radians.
  virtual void applyRotationZ(size_t target, Precision angle) = 0;
  
  /// @brief Applies a SWAP gate, exchanging the states of two qubits.
  /// @param qubit1 The first qubit.
  /// @param qubit2 The second qubit.
  virtual void applySWAP(size_t qubit1, size_t qubit2) = 0;
  
  /// @brief Applies a Controlled-Z (CZ) gate.
  /// @param control The control qubit.
  /// @param target The target qubit.
  virtual void applyCZ(size_t control, size_t target) = 0;
  /// @brief Applies a dense custom unitary matrix to a set of target qubits.
  /// @param targets Ordered vector of target qubit indices.
  /// @param matrix The flattened complex matrix entries.
  virtual void applyDenseUnitary(const std::vector<size_t> &targets,
                                 const std::vector<Complex> &matrix) = 0;

  // --- Noise ---
  /// @brief Applies depolarizing noise to all qubits with a given probability.
  /// @param probability The probability [0,1] of applying a random Pauli error.
  virtual void applyDepolarizingNoise(Precision probability) = 0;

  // --- Measurement & Analysis ---
  /// @brief Measures a target qubit in the computational basis, collapsing the state.
  /// @param target The index of the qubit to measure.
  /// @return The measurement outcome (0 or 1).
  [[nodiscard]] virtual int measure(size_t target) = 0;
  
  /// @brief Calculates the probability of measuring 1 for each computational basis state.
  /// @return A vector of probabilities for the entire state vector.
  [[nodiscard]] virtual std::vector<double> getProbabilities() const = 0; // Return double for probs usually fine
  
  /// @brief Computes the expectation value for a given Pauli string invariant.
  /// @param pauli_string The string representing the tensor product of Pauli operators.
  /// @return The computed observable expectation value.
  [[nodiscard]] virtual double expectationValue(const std::string &pauli_string) const = 0;

  // --- State Access ---
  /// @brief Retrieves the full exact complex state vector backing the simulation.
  /// @return A copy of the complex state vector.
  [[nodiscard]] virtual std::vector<Complex> getStateVector() const = 0;

  // --- Distributed Helpers (Optional / Backend Specific) ---
  virtual int getRank() const { return 0; }
  virtual int getSize() const { return 1; }

  // --- Hardware Properties ---
  virtual size_t getNumQubits() const { return 0; }
};

} // namespace qubit_engine
