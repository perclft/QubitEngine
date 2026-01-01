#include "../backend/src/QuantumRegister.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

// Lightweight Shim to mimic Google Benchmark behavior loosely for this specific
// test
namespace benchmark {
struct State {
  int qubit_count;
  int64_t iterations_count;
  int64_t bytes_processed;

  State(int q) : qubit_count(q), iterations_count(0), bytes_processed(0) {}

  struct Iterator {
    int64_t current;
    int64_t max_iter;
    bool operator!=(const Iterator &other) const {
      return current != other.current;
    }
    void operator++() { current++; }
    Iterator &operator*() { return *this; }
  };

  Iterator begin() {
    // Scale iterations inversely with size to keep runtime reasonable
    int64_t base_iter = 100;
    if (qubit_count >= 25)
      base_iter = 5;
    if (qubit_count >= 28)
      base_iter = 2; // Be careful with 28 qubits (4GB), might be slow
    iterations_count = base_iter;
    return {0, base_iter};
  }
  Iterator end() { return {iterations_count, iterations_count}; }

  void SetBytesProcessed(int64_t bytes) { bytes_processed = bytes; }

  int range(int idx) { return qubit_count; }
  int64_t iterations() { return iterations_count; }
};

void DoNotOptimize(void *p) { volatile void *vp = p; }
void ClobberMemory() { std::atomic_signal_fence(std::memory_order_acq_rel); }
} // namespace benchmark

void BM_ApplyHadamard(int num_qubits) {
  benchmark::State state(num_qubits);
  std::cout << "Benchmarking " << num_qubits << " qubits ("
            << ((1ULL << num_qubits) * 16 / (1024.0 * 1024.0)) << " MB)..."
            << std::flush;

  try {
    QuantumRegister q(num_qubits);

    auto start = std::chrono::high_resolution_clock::now();

    for (auto _ : state) {
      q.applyHadamard(0);
      benchmark::DoNotOptimize(q.getStateVector().data());
      benchmark::ClobberMemory();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    // Reporting
    double total_bytes = double(1ULL << num_qubits) * 16.0 * 2.0 *
                         state.iterations(); // Read + Write
    double gb_per_sec =
        (total_bytes / diff.count()) / (1024.0 * 1024.0 * 1024.0);

    std::cout << " Done." << std::endl;
    std::cout << "  Time:       " << diff.count() << " s" << std::endl;
    std::cout << "  Iterations: " << state.iterations() << std::endl;
    std::cout << "  Bandwidth:  " << gb_per_sec << " GB/s" << std::endl;
    std::cout << "-----------------------------------------------------"
              << std::endl;

  } catch (const std::exception &e) {
    std::cout << " Failed: " << e.what() << std::endl;
  }
}

int main() {
  std::cout << "--- Memory Wall Benchmark (Shim) ---" << std::endl;
  std::cout << "Analyzing Hadamard Gate Bandwidth" << std::endl;
  std::cout << "-----------------------------------------------------"
            << std::endl;

  BM_ApplyHadamard(20); // 16 MB - L3 Cache / RAM boundary
  BM_ApplyHadamard(22); // 64 MB - RAM
  BM_ApplyHadamard(24); // 256 MB - RAM
  BM_ApplyHadamard(25); // 512 MB - RAM significant
  // BM_ApplyHadamard(28); // 4 GB - skipped for safety in unknown agent env
  // options

  return 0;
}
