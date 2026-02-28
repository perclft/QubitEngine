#pragma once
#include "IQuantumBackend.hpp"
#include "../Types.hpp"
#include <chrono>
#include <complex>
#include <random>
#include <thread>
#include <string>
#include <vector>

namespace qubit_engine {

class MockHardwareBackend : public IQuantumBackend {
private:
  int num_qubits;
  std::mt19937 gen;
  std::normal_distribution<double> noise;

public:
  explicit MockHardwareBackend(int n)
      : num_qubits(n), gen(std::random_device{}()), noise(0.0, 0.05) {}

  // --- Core Gates ---
  void applyHadamard(size_t target) override { simulateExecutionDelay(); }
  void applyX(size_t target) override { simulateExecutionDelay(); }
  void applyY(size_t target) override { simulateExecutionDelay(); }
  void applyZ(size_t target) override { simulateExecutionDelay(); }
  void applyCNOT(size_t control, size_t target) override {
    simulateExecutionDelay();
  }

  // --- Advanced Gates ---
  void applyToffoli(size_t control1, size_t control2, size_t target) override {
    simulateExecutionDelay();
  }
  void applyPhaseS(size_t target) override { simulateExecutionDelay(); }
  void applyPhaseT(size_t target) override { simulateExecutionDelay(); }
  void applyRotationX(size_t target, Precision angle) override {
    simulateExecutionDelay();
  }
  void applyRotationY(size_t target, Precision angle) override {
    simulateExecutionDelay();
  }
  void applyRotationZ(size_t target, Precision angle) override {
    simulateExecutionDelay();
  }
  void applySWAP(size_t qubit1, size_t qubit2) override {
    simulateExecutionDelay();
  }
  void applyCZ(size_t control, size_t target) override {
    simulateExecutionDelay();
  }

  // --- Noise ---
  void applyDepolarizingNoise(Precision probability) override {}

  // --- Measurement & Analysis ---
  int measure(size_t target) override { return 0; }

  std::vector<double> getProbabilities() override {
    size_t size = 1ULL << num_qubits;
    std::vector<double> probs(size, 0.0);
    probs[0] = 1.0;
    return probs;
  }

  double expectationValue(const std::string &pauli_string) override {
    return 1.0;
  }

  // --- State Access ---
  std::vector<qubit_engine::Complex> getStateVector() const override {
    // Return a noisy |0...0> state to mock a hardware return struct
    size_t size = 1ULL << num_qubits;
    if (size > 1024)
      size = 1024; // Cap for demo safety

    std::vector<qubit_engine::Complex> state(size);

    // Need mutable RNG for const method, so we use a local one
    // just for state vector extraction payload
    std::mt19937 local_gen(std::random_device{}());
    std::normal_distribution<double> local_noise(0.0, 0.05);

    for (size_t i = 0; i < size; ++i) {
      if (i == 0) {
        state[i] = qubit_engine::Complex(0.9 + local_noise(local_gen),
                                         local_noise(local_gen));
      } else {
        state[i] = qubit_engine::Complex(local_noise(local_gen),
                                         local_noise(local_gen));
      }
    }
    return state;
  }

private:
  void simulateExecutionDelay() const {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
};

} // namespace qubit_engine
