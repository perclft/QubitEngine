
#include "../src/QuantumRegister.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
  int num_qubits = 15; // 2^15 = 32,768 amplitudes
  std::cout << "--- Quantum Linux 2 Benchmark ---" << std::endl;
  std::cout << "Creating Register with " << num_qubits << " qubits..."
            << std::endl;

  QuantumRegister q(num_qubits);

  auto start = std::chrono::high_resolution_clock::now();

  int num_gates = 10;
  std::cout << "Applying " << num_gates << " layers of Hadamard gates..."
            << std::endl;

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

  return 0;
}
