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

### Analysis

1.  **Cache Resident (20 Qubits)**: At 16 MB, the state vector likely fits partially or entirely within the L3 cache of the processor (depending on the specific SKU), resulting in the highest observed bandwidth (~8.56 GB/s).
2.  **Memory Bound (22+ Qubits)**: As the state vector grows to 64 MB and beyond, it exceeds cache capacity. The performance drops to ~7.2 GB/s and stabilizes around ~6.6-6.8 GB/s. This plateau represents the effective main memory bandwidth available to the single-threaded AVX2 kernel.
3.  **Conclusion**: The `applyHadamard` operation is memory-bandwidth bound for N >= 22. Further optimizations would require multi-threading (OpenMP) to saturate the memory bus, as a single core cannot typically utilize full DDR bandwidth.
