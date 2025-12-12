#pragma once
#include "IQuantumBackend.hpp"
#include <chrono>
#include <random>
#include <thread>

class MockHardwareBackend : public IQuantumBackend {
private:
  int num_qubits;
  // We simulate state crudely just to return something valid-ish
  // Or we just return a "Noisy" zeros state

public:
  explicit MockHardwareBackend(int n) : num_qubits(n) {}

  void applyGate(const qubit_engine::GateOperation &op) override {
    // Mock Hardware doesn't execute gate-by-gate in real-time usually,
    // it queues the whole circuit.
    // But for our interface streaming compatibility, we'll just sleep a tiny
    // bit per gate to simulate "transmission" or execution time.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  void getResult(qubit_engine::StateResponse *response) override {
    // Simulate Queue Wait Time
    std::this_thread::sleep_for(std::chrono::seconds(2));

    response->clear_state_vector();

    // Return a "Noisy" |0...0> state
    // In reality, hardware returns counts (shots), not state vector.
    // But our frontend expects a state vector for visualization.
    // We will fake a state vector that looks slightly noisy around |0...0>

    // Size = 2^N
    size_t size = 1ULL << num_qubits;

    // Limit size for safety (Mock shouldn't crash on large N)
    if (size > 1024)
      size = 1024; // Cap for demo

    std::mt19937 gen(std::random_device{}());
    std::normal_distribution<> noise(0.0, 0.05);

    for (size_t i = 0; i < size; ++i) {
      auto *c = response->add_state_vector();
      if (i == 0) {
        // Mostly |0...0>
        c->set_real(0.9 + noise(gen));
        c->set_imag(noise(gen));
      } else {
        // Noise floor
        c->set_real(noise(gen));
        c->set_imag(noise(gen));
      }
    }

    response->set_server_id("Mock-IBM-Q-System-One");
  }
};
