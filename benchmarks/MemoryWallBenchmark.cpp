#include <benchmark/benchmark.h>
#include "../backend/src/QuantumRegister.hpp"
#include <iostream>

// Benchmark for Hadamard Gate to test Memory Wall
static void BM_ApplyHadamard(benchmark::State& state) {
  int num_qubits = state.range(0);
  
  // Setup: Allocate memory once (outside the measurement loop) to avoid measuring alloc time
  // Note: 28 qubits = 2^28 * 16 bytes = 4 GB of RAM.
  try {
      qubit_engine::QuantumRegister q(num_qubits);

      for (auto _ : state) {
        // Measure the bandwidth-bound operation
        q.applyHadamard(0); 
        
        // Prevent optimization (though unlikely for such a large side-effect function)
        benchmark::DoNotOptimize(q.getStateVector().data());
        benchmark::ClobberMemory();
      }
      
      // Calculate bandwidth metrics for report
      // Total bytes moved: Read 2 vectors (real/imag), Write 2 vectors.
      // Actually CpuBackend: Read 1 stream, Write 1 stream (In-place? No, stride based access)
      // applyHadamard usually does: a = state[i], b = state[i+stride], state[i]=..., state[i+stride]=...
      // So it reads the array once and writes it once. 2 * Size.
      // Size = 2^N * 16 bytes.
      double bytes_processed = double(1ULL << num_qubits) * 16.0 * 2.0;
      state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(bytes_processed));
      
  } catch (const std::exception& e) {
      state.SkipWithError("Not enough memory or error during allocation");
  }
}

// Register benchmarks for 20 (16MB), 25 (512MB), 28 (4GB)
BENCHMARK(BM_ApplyHadamard)
    ->Arg(20)
    ->Arg(25)
    ->Arg(28)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
