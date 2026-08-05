#pragma once

#include "backends/IQuantumBackend.hpp"
#include "BackendFactory.hpp"
#include "NoiseModel.hpp"
#include <complex>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "qubit_engine_export.h"

#include "Types.hpp"

namespace qubit_engine {

class QUBIT_ENGINE_EXPORT QuantumRegister {
public:
  // --- Lifecycle ---
  /// @brief Constructs a register with automatic backend selection via BackendFactory.
  QuantumRegister(size_t n, bool force_local = false);

  /// @brief Dependency injection constructor — accepts a pre-built backend.
  /// Use this in tests to inject mock/stub backends.
  QuantumRegister(size_t n, std::unique_ptr<IQuantumBackend> injected_backend);

  ~QuantumRegister();

  // --- Core Gates ---
  void applyHadamard(size_t target);
  void applyX(size_t target);
  void applyY(size_t target);
  void applyZ(size_t target);
  void applyCNOT(size_t control, size_t target);

  // --- Advanced Gates ---
  void applyToffoli(size_t control1, size_t control2, size_t target);
  void applyPhaseS(size_t target);
  void applyPhaseT(size_t target);
  void applyRotationX(size_t target, Precision angle);
  void applyRotationY(size_t target, Precision angle);
  void applyRotationZ(size_t target, Precision angle);
  void applySWAP(size_t qubit1, size_t qubit2);
  void applyCZ(size_t control, size_t target);
  void applyDenseUnitary(const std::vector<size_t> &targets,
                         const std::vector<Complex> &matrix);

  // --- Noise Simulation ---
  void applyDepolarizingNoise(Precision probability);
  void applyNoiseChannel1Q(const NoiseChannel1Q& channel, size_t target);
  void applyNoiseChannel2Q(const NoiseChannel2Q& channel, size_t q1, size_t q2);

  /// @brief Attaches a noise model. When set, noise is applied automatically
  /// after every gate operation.
  void setNoiseModel(const NoiseModel& model);

  /// @brief Returns the current noise model, or nullptr if none is set.
  const NoiseModel* getNoiseModel() const;

  // --- Measurement & Analysis ---
  int measure(size_t target);
  std::vector<double> getProbabilities() const;
  double expectationValue(const std::string &pauli_string) const;

  // --- Distributed Helpers ---
  int getRank() const;
  int getSize() const;
  size_t getNumQubits() const { return num_qubits; }

  // --- Debugging ---
  std::vector<Complex> getStateVector() const;
  IQuantumBackend* getBackend() const { return backend.get(); }

  // --- Recording / Tape Helper ---
  struct RecordedGate {
    enum Type {
      H,
      X,
      Y,
      Z,
      CNOT,
      RX,
      RY,
      RZ,
      PHASE_S,
      PHASE_T,
      TOFFOLI,
      MEASURE,
      SWAP,
      CZ
    };
    Type type;
    std::vector<size_t> qubits; // [target] or [control, target]
    std::vector<double> params; // [angle]
  };

  void enableRecording(bool enable);
  void enableExecution(bool enable);
  void clearTape();
  const std::vector<RecordedGate> &getTape() const;

  void optimize();        // Optimizes the current tape
  void mapToTopology(const std::string& device_preset = "");   // Routes tape to physical topology
  void transpileToClifford(bool approximate = false, bool use_stochastic = false);

  // Helper to Replay Tape (Optional, for adjoint)
  void applyRegisteredGate(const RecordedGate &gate);
  void applyRegisteredGateInverse(const RecordedGate &gate);

  std::string getBackendName() const;

private:
  void validateQubit(size_t q) const;
  void applyPostGateNoise1Q(size_t target);
  void applyPostGateNoise2Q(size_t q1, size_t q2);
  size_t num_qubits;
  std::unique_ptr<qubit_engine::IQuantumBackend> backend;
  std::shared_ptr<NoiseModel> noise_model_;
  bool recording_enabled = false;
  bool execution_enabled = true;
  std::vector<RecordedGate> tape;
};

} // namespace qubit_engine
