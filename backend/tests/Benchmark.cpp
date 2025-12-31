
#include "../src/QuantumRegister.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  int num_qubits = 25; // 2^25 * 16 bytes ~= 536 MB
  if (argc > 1) {
    num_qubits = std::atoi(argv[1]);
  }

  std::cout << "--- QubitEngine Stress Test ---" << std::endl;
  std::cout << "Initializing Quantum Register with " << num_qubits
            << " qubits..." << std::endl;
  size_t memory_size = (1ULL << num_qubits) * 16;
  std::cout << "Estimated Memory Usage: " << memory_size / (1024.0 * 1024.0)
            << " MB" << std::endl;

  QuantumRegister q(num_qubits);

  int num_gates = 100;
  std::cout << "Applying " << num_gates
            << " layers of Hadamard gates across all qubits..." << std::endl;

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < num_gates; ++i) {
    for (int k = 0; k < num_qubits; ++k) {
      q.applyHadamard(k);
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diff = end - start;

  double total_gates = (double)num_gates * num_qubits;
  double throughput = total_gates / diff.count();

  std::cout << "Time: " << diff.count() << " s" << std::endl;
  std::cout << "Throughput: " << throughput / 1e6 << " million gates/s"
            << std::endl;
  std::cout << "Effective Bandwidth: "
            << (throughput * 16.0 * 2.0) / (1024.0 * 1024.0 * 1024.0)
            << " GB/s (R/W)" << std::endl;

  return 0;
}
