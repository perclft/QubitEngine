# QubitEngine Benchmark Results

**Date:** 2025-12-31
**Environment:** Windows (MSVC)
**Optimization:** AVX2 Enabled
**Backend:** CpuBackend (Double Precision Complex)

## Memory Wall Analysis

The following benchmarks measured the effective memory bandwidth during the application of a Hadamard gate across the entire state vector.

### Data

| Qubits | State Vector Size | Time (s) | effective Bandwidth (GB/s) |
|:-------|:------------------|:---------|:---------------------------|
| 20     | 16 MB             | 0.365    | **8.56**                   |
| 22     | 64 MB             | 1.724    | **7.25**                   |
| 24     | 256 MB            | 7.329    | **6.82**                   |
| 25     | 512 MB            | 0.753*   | **6.64**                   |

*(Note: Time for 25 qubits is normalized per iteration, total runtime was longer)*

## Antigravity Memory Benchmark (macOS Apple Silicon)

**Date:** 2025-12-31
**Environment:** macOS (M3 Air)
**Optimization:** NEON/ARM64 Enabled (Release Build)
**Backend:** CpuBackend (Non-Metal fallback)

| Qubits | State Vector Size | Time (ms) | effective Bandwidth (GB/s) |
|:-------|:------------------|:----------|:---------------------------|
| 20     | 16 MB             | 0.713     | **22.44**                  |
| 25     | 512 MB            | 26.9      | **19.03**                  |
| 28     | 4096 MB           | 787.0     | **5.20**                   |

### Analysis

1. **L1/L2 Cache Efficiency (20 Qubits)**: The M3's memory architecture provides significantly higher bandwidth for small state vectors compared to the baseline Windows results, hitting ~22 GB/s.
2. **The Memory Wall (28 Qubits)**: A massive drop-off occurs as the state vector size (4 GB) exceeds the unified memory cache efficiency sweet spot and TLB limits, resulting in a drop to ~5.2 GB/s. This confirms the memory wall effect on Apple Silicon.

### Analysis

1. **Cache Resident (20 Qubits)**: At 16 MB, the state vector likely fits partially or entirely within the L3 cache of the processor (depending on the specific SKU), resulting in the highest observed bandwidth (~8.56 GB/s).
2. **Memory Bound (22+ Qubits)**: As the state vector grows to 64 MB and beyond, it exceeds cache capacity. The performance drops to ~7.2 GB/s and stabilizes around ~6.6-6.8 GB/s. This plateau represents the effective main memory bandwidth available to the single-threaded AVX2 kernel.
3. **Conclusion**: The `applyHadamard` operation is memory-bandwidth bound for N >= 22. Further optimizations would require multi-threading (OpenMP) to saturate the memory bus, as a single core cannot typically utilize full DDR bandwidth.

## Antigravity Local Benchmark (Arch Linux)

**Date:** 2025-12-31
**Environment:** Arch Linux (GCC 15)
**Optimization:** AVX2 Enabled (Single-threaded path)
**Backend:** CpuBackend (Double Precision)

| Qubits | State Vector Size | Time (ms) | effective Bandwidth (GB/s) |
|:-------|:------------------|:----------|:---------------------------|
| 20     | 16 MB             | 22.45     | **1.39**                   |
| 22     | 64 MB             | 85.39     | **1.46**                   |
| 24     | 256 MB            | 336.47    | **1.48**                   |
| 26     | 1024 MB           | 1352.29   | **1.48**                   |

### Analysis

1. **Memory Bound Stability**: The effective bandwidth stabilizes around 1.48 GB/s for state vectors larger than 64 MB. This represents the peak throughput of the single-threaded AVX2 implementation on this specific hardware.
2. **Comparison to Apple Silicon**: While the peak bandwidth is lower than the M3's unified memory (~22 GB/s), it remains consistent as the state vector grows to 1 GB, showing less drastic "wall" effects compared to the M3's 4GB drop-off (down to 5.2 GB/s).
3. **Optimization Path**: Enabling OpenMP for the AVX2 path in `CpuBackend.cpp` should significantly increase these numbers by leveraging multiple memory controllers.
