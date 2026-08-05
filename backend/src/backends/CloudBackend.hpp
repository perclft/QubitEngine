#pragma once

#include "IQuantumBackend.hpp"
#include "quantum.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <vector>
#include "qubit_engine_export.h"

namespace qubit_engine {

class QUBIT_ENGINE_EXPORT CloudBackend : public IQuantumBackend {
public:
  CloudBackend(size_t num_qubits, const std::string &remote_url);
  virtual ~CloudBackend() = default;

  // --- Core Gates ---
  void applyHadamard(size_t target) override;
  void applyX(size_t target) override;
  void applyY(size_t target) override;
  void applyZ(size_t target) override;
  void applyCNOT(size_t control, size_t target) override;

  // --- Advanced Gates ---
  void applyToffoli(size_t control1, size_t control2, size_t target) override;
  void applyPhaseS(size_t target) override;
  void applyPhaseT(size_t target) override;
  void applyRotationX(size_t target, Precision angle) override;
  void applyRotationY(size_t target, Precision angle) override;
  void applyRotationZ(size_t target, Precision angle) override;
  void applySWAP(size_t qubit1, size_t qubit2) override;
  void applyCZ(size_t control, size_t target) override;

  // --- Noise ---
  void applyDepolarizingNoise(Precision probability) override;
  void applyNoiseChannel1Q(const NoiseChannel1Q& channel, size_t target) override;
  void applyNoiseChannel2Q(const NoiseChannel2Q& channel, size_t q1, size_t q2) override;

  // --- Measurement & Analysis ---
  int measure(size_t target) override;
  std::vector<double> getProbabilities() const override;
  double expectationValue(const std::string &pauli_string) const override;

  void applyDenseUnitary(const std::vector<size_t> &targets,
                         const std::vector<Complex> &matrix) override {}

  // --- State Access ---
  std::vector<Complex> getStateVector() const override;

  // --- Hardware Properties ---
  size_t getNumQubits() const override { return num_qubits_; }
  std::string name() const override { return "Cloud"; }

private:
  size_t num_qubits_;
  std::string remote_url_;
  std::unique_ptr<QuantumCompute::Stub> stub_;
  
  // Buffering the circuit for batch remote execution
  qubit_engine::CircuitRequest request_;

  void ensureRemoteExecution() const;
  mutable qubit_engine::StateResponse last_response_;
  mutable bool needs_sync_ = true;
};

} // namespace qubit_engine
