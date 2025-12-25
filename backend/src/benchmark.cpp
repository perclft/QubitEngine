#include "QuantumRegister.hpp"
#include <chrono>
#include <iostream>
#include <vector>

using namespace std::chrono;

int main() {
  const int num_qubits = 24; // 2^24 * 16 bytes ~= 268 MB state vector
  std::cout << "Initializing Quantum Register with " << num_qubits << " qubits..." << std::endl;
  
  QuantumRegister qreg(num_qubits);
  
  // Warmup
  qreg.applyX(0);

  std::cout << "Benchmarking Hadamard Gate..." << std::endl;
  auto start = high_resolution_clock::now();
  
  // Apply H on all qubits to ensure we hit all memory strides
  for (int i = 0; i < num_qubits; ++i) {
      qreg.applyHadamard(i);
  }

  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(stop - start);

  std::cout << "Time taken for " << num_qubits << " Hadamard gates: " 
            << duration.count() << " ms" << std::endl;
  
  std::cout << "Avg time per gate: " << (double)duration.count() / num_qubits << " ms" << std::endl;

  return 0;
}
