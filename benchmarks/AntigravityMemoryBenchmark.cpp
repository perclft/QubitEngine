#include "../backend/src/QuantumRegister.hpp"
#include <benchmark/benchmark.h>
#include <iostream>
#include <vector>

static void BM_Antigravity_ApplyHadamard(benchmark::State &state) {
  size_t num_qubits = state.range(0);

  // Setup: Allocate memory once
  try {
    // Note: QuantumRegister is in global namespace
    QuantumRegister q(num_qubits);

    for (auto _ : state) {
      // Measure Hadamard gate performance
      q.applyHadamard(0);

      // Prevent compiler optimizations
      benchmark::DoNotOptimize(q.getStateVector().data());
      benchmark::ClobberMemory();
    }
  } catch (const std::exception &e) {
    state.SkipWithError(e.what());
  }
}

// Register the benchmark for 20, 25, and 28 qubits
BENCHMARK(BM_Antigravity_ApplyHadamard)
    ->Arg(20)
    ->Arg(25)
    ->Arg(28)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
