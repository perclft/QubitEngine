# QubitEngine Benchmark Results

**Date:** 2025-12-31
**Environment:** Windows (MSVC)
**Optimization:** AVX2 Enabled
**Backend:** CpuBackend (Double Precision Complex)

## Memory Wall Analysis (Windows Baseline)

The following benchmarks measured the effective memory bandwidth during the application of a Hadamard gate across the entire state vector.

### Data (Windows)

| Qubits | State Vector Size | Time (s) | effective Bandwidth (GB/s) |
|:-------|:------------------|:---------|:---------------------------|
| 20     | 16 MB             | 0.365    | **8.56**                   |
| 22     | 64 MB             | 1.724    | **7.25**                   |
| 24     | 256 MB            | 7.329    | **6.82**                   |
| 25     | 512 MB            | 0.753*   | **6.64**                   |

### Windows Scaling Note

Note: Time for 25 qubits is normalized per iteration, total runtime was longer.

### Baseline Conclusion

The `applyHadamard` operation is memory-bandwidth bound for N >= 22. Further optimizations would require multi-threading (OpenMP) to saturate the memory bus, as a single core cannot typically utilize full DDR bandwidth.

## Apple Silicon Performance (NEON)

**Date:** 2025-12-31
**Environment:** macOS (M3 Air)
**Optimization:** NEON/ARM64 Enabled (Release Build)
**Backend:** CpuBackend (Non-Metal fallback)

| Qubits | State Vector Size | Time (ms) | effective Bandwidth (GB/s) |
|:-------|:------------------|:----------|:---------------------------|
| 20     | 16 MB             | 0.713     | **22.44**                  |
| 25     | 512 MB            | 26.9      | **19.03**                  |
| 28     | 4096 MB           | 787.0     | **5.20**                   |

### Analysis (macOS)

1. **L1/L2 Cache Efficiency (20 Qubits)**: The M3's memory architecture provides significantly higher bandwidth for small state vectors compared to the baseline Windows results, hitting ~22 GB/s.
2. **The Memory Wall (28 Qubits)**: A massive drop-off occurs as the state vector size (4 GB) exceeds the unified memory cache efficiency sweet spot and TLB limits, resulting in a drop to ~5.2 GB/s. This confirms the memory wall effect on Apple Silicon.

## Arch Linux Local Benchmarks (AVX2/MPI/OpenMP)

**Date:** 2025-12-31
**Environment:** Arch Linux (GCC 15)
**Backend:** CpuBackend (Double Precision)

### 1. Single-Threaded (Baseline)

| Qubits | State Vector Size | Time (ms) | effective Bandwidth (GB/s) |
|:-------|:------------------|:----------|:---------------------------|
| 20     | 16 MB             | 22.45     | **1.39**                   |
| 22     | 64 MB             | 85.39     | **1.46**                   |
| 24     | 256 MB            | 336.47    | **1.48**                   |
| 26     | 1024 MB           | 1352.29   | **1.48**                   |

### 2. Multi-Threaded (OpenMP)

| Qubits | State Vector Size | Time (ms) | effective Bandwidth (GB/s) |
|:-------|:------------------|:----------|:---------------------------|
| 20     | 16 MB             | 20.24     | **1.58**                   |
| 22     | 64 MB             | 84.46     | **1.52**                   |
| 24     | 256 MB            | 342.35    | **1.50**                   |
| 26     | 1024 MB           | 1372.94   | **1.49**                   |

### 3. Distributed (MPI n=2)

| Qubits | State Vector Size | Time (ms) | effective Bandwidth (GB/s) |
|:-------|:------------------|:----------|:---------------------------|
| 20     | 16 MB             | 10.16     | **3.15**                   |
| 22     | 64 MB             | 40.68     | **3.15**                   |
| 24     | 256 MB            | 287.25    | **1.78**                   |
| 26     | 1024 MB           | 1133.99   | **1.81**                   |

### Final Comparative Analysis

1. **MPI Scaling**: Distributed execution via MPI provided a near-perfect 2x speedup for smaller state vectors (up to 64 MB), effectively doubling the available memory bandwidth by utilizing two independent simulation kernels.
2. **Network/Sync Overhead**: For larger state vectors (256 MB+), the speedup dropped to ~16-20%. This indicates that the time spent communicating half the state vector across the MPI interconnect (even on localhost) begins to rival the computation time as memory access becomes the bottleneck.
3. **OpenMP Limitations**: The Multi-threaded (OpenMP) results showed negligible gains over single-threaded execution. This confirms that the current AVX2 implementation is strictly memory-bandwidth bound on this processor; adding more cores does not increase the rate at which data can be fetched from the memory controllers.
