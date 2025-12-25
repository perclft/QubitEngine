#include "QuantumRegister.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

using namespace std::chrono;

void benchmark_gate(QuantumRegister &qreg, const std::string &name,
                    std::function<void(int)> op) {
  auto start = high_resolution_clock::now();
  int num_qubits = 24; // Implicit knowledge of qreg size
  for (int i = 0; i < num_qubits; ++i) {
    op(i);
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(stop - start);
  std::cout << "Time taken for " << num_qubits << " " << name << ": "
            << duration.count() << " ms ("
            << (double)duration.count() / num_qubits << " ms/gate)"
            << std::endl;
}

int main() {
  const int num_qubits = 24;
  std::cout << "Initializing Quantum Register with " << num_qubits
            << " qubits..." << std::endl;

  QuantumRegister qreg(num_qubits);

  // Warmup
  qreg.applyX(0);

  std::cout << "--- Benchmarking Core Gates ---" << std::endl;

  // H (Already optimized)
  benchmark_gate(qreg, "Hadamard Gates (CPU - NEON)",
                 [&](int i) { qreg.applyHadamard(i); });

  // H (Metal GPU)
  benchmark_gate(qreg, "Hadamard Gates (GPU - Metal)",
                 [&](int i) { qreg.applyHadamardMetal(i); });

  // H (Metal GPU - Resident)
  std::cout << "Uploading state to GPU..." << std::endl;
  qreg.toGPU();
  benchmark_gate(qreg, "Hadamard Gates (GPU - Resident)",
                 [&](int i) { qreg.applyHadamardMetal(i); });
  qreg.toCPU();
  std::cout << "Downloaded state from GPU." << std::endl;

  // X (Already optimized)
  benchmark_gate(qreg, "Pauli-X Gates", [&](int i) { qreg.applyX(i); });

  std::cout << "--- Benchmarking Candidates for Optimization ---" << std::endl;

  // Y
  benchmark_gate(qreg, "Pauli-Y Gates", [&](int i) { qreg.applyY(i); });

  // Z
  benchmark_gate(qreg, "Pauli-Z Gates", [&](int i) { qreg.applyZ(i); });

  // Rotation Z (Phase)
  benchmark_gate(qreg, "Rotation-Z (PI/4)",
                 [&](int i) { qreg.applyRotationZ(i, M_PI / 4.0); });

  // Rotation Y (Real rotation)
  benchmark_gate(qreg, "Rotation-Y (PI/4)",
                 [&](int i) { qreg.applyRotationY(i, M_PI / 4.0); });

  return 0;
}
