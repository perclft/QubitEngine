# QubitEngine Performance Benchmark Results

**Platform:** Windows (win32)
**Python Version:** 3.14.0
**Backend:** CudaBackend / CpuBackend Dual (via native extensions)

## 1. Scaling Test: State Vector Size vs Performance

| Qubits | State Size | Memory (KB) | Init Time | H Gate Time |
|--------|------------|-------------|-----------|-------------|
| 4      | 16         | 0.2         | 0.295 ms  | 0.0152 ms   |
| 8      | 256        | 4.0         | 0.009 ms  | 0.0150 ms   |
| 12     | 4,096      | 64.0        | 0.029 ms  | 0.0094 ms   |
| 16     | 65,536     | 1,024.0     | 0.135 ms  | 0.0278 ms   |
| 18     | 262,144    | 4,096.0     | 0.526 ms  | 0.0875 ms   |
| 20     | 1,048,576  | 16,384.0    | 1.761 ms  | 0.3220 ms   |
| 22     | 4,194,304  | 65,536.0    | 7.459 ms  | 1.2050 ms   |

## 2. Gate Throughput Test (16 qubits, 100 iterations)

| Gate       | Total Gates | Duration | Gates/sec | μs/gate |
|------------|-------------|----------|-----------|---------|
| Hadamard   | 1,600       | 0.0218 s | 73.56K    | 13.59   |
| Pauli-X    | 1,600       | 0.0714 s | 22.40K    | 44.65   |
| Pauli-Y    | 1,600       | 0.0496 s | 32.26K    | 30.99   |
| Pauli-Z    | 1,600       | 0.0153 s | 104.33K   | 9.58    |
| Rotation-Y | 1,600       | 0.0430 s | 37.23K    | 26.86   |
| Rotation-Z | 1,600       | 0.0396 s | 40.37K    | 24.77   |
| CNOT       | 1,500       | 0.0292 s | 51.41K    | 19.45   |

## 3. Entanglement Benchmarks

| Circuit    | Qubits | Time     | Gates | Gates/sec |
|------------|--------|----------|-------|-----------|
| Bell Pairs | 8      | 0.067 ms | 8     | 119.40K   |
| GHZ State  | 8      | 0.062 ms | 8     | 129.24K   |
| Bell Pairs | 12     | 0.089 ms | 12    | 134.68K   |
| GHZ State  | 12     | 0.071 ms | 12    | 169.01K   |
| Bell Pairs | 16     | 0.194 ms | 16    | 82.60K    |
| GHZ State  | 16     | 0.248 ms | 16    | 64.46K    |
| Bell Pairs | 20     | 2.876 ms | 20    | 6.95K     |
| GHZ State  | 20     | 3.627 ms | 20    | 5.51K     |

## 4. Random Circuit Benchmark (Depth = 20)

| Qubits | Total Gates | Duration  | Gates/sec |
|--------|-------------|-----------|-----------|
| 8      | 240         | 1.69 ms   | 141.68K   |
| 12     | 360         | 2.03 ms   | 177.06K   |
| 16     | 480         | 11.70 ms  | 41.03K    |
| 18     | 540         | 39.60 ms  | 13.64K    |
| 20     | 600         | 153.90 ms | 3.90K     |

## 5. Stress Test: Maximum Qubit Count

| Qubits | Amplitudes | Memory Target | Status | Init Time | Gate Time |
|--------|------------|---------------|--------|-----------|-----------|
| 20     | 1,048,576  | 0.02 GB       | [OK]   | 3.5 ms    | 0.58 ms   |
| 22     | 4,194,304  | 0.06 GB       | [OK]   | 7.5 ms    | 2.44 ms   |
| 24     | 16,777,216 | 0.25 GB       | [OK]   | 29.1 ms   | 17.75 ms  |
| 26     | 67,108,864 | 1.00 GB       | [OK]   | 117.9 ms  | 90.37 ms  |

## Linux Performance (Cuda)

**Date:** 2026-02-26
**Environment:** Linux - Cuda Backend (GPU)

### 1. Scaling Test: State Vector Size vs Performance

| Qubits | State Size | Memory (KB) | Init Time | H Gate Time |
|--------|------------|-------------|-----------|-------------|
| 4      | 16         | 0.2         | 103.101 ms| 0.0095 ms   |
| 8      | 256        | 4.0         | 0.035 ms  | 0.0069 ms   |
| 12     | 4,096      | 64.0        | 0.024 ms  | 0.0130 ms   |
| 16     | 65,536     | 1,024.0     | 0.025 ms  | 0.0098 ms   |
| 18     | 262,144    | 4,096.0     | 0.175 ms  | 0.0184 ms   |
| 20     | 1,048,576  | 16,384.0    | 0.240 ms  | 0.1079 ms   |
| 22     | 4,194,304  | 65,536.0    | 0.349 ms  | 0.3951 ms   |

### 2. Gate Throughput Test (16 qubits, 100 iterations)

| Gate       | Total Gates | Duration | Gates/sec | µs/gate |
|------------|-------------|----------|-----------|---------|
| Hadamard   | 1,600       | 0.0217 s | 73.80K    | 13.55   |
| Pauli-X    | 1,600       | 0.0145 s | 110.20K   | 9.07    |
| Pauli-Y    | 1,600       | 0.0180 s | 89.09K    | 11.23   |
| Pauli-Z    | 1,600       | 0.0162 s | 98.69K    | 10.13   |
| Rotation-Y | 1,600       | 0.0395 s | 40.54K    | 24.67   |
| Rotation-Z | 1,600       | 0.0582 s | 27.48K    | 36.38   |
| CNOT       | 1,500       | 0.0134 s | 112.26K   | 8.91    |

### 3. Entanglement Benchmarks

| Circuit    | Qubits | Time     | Gates | Gates/sec |
|------------|--------|----------|-------|-----------|
| Bell Pairs | 8      | 0.066 ms | 8     | 120.63K   |
| GHZ State  | 8      | 0.063 ms | 8     | 127.25K   |
| Bell Pairs | 12     | 0.094 ms | 12    | 128.26K   |
| GHZ State  | 12     | 0.089 ms | 12    | 134.77K   |
| Bell Pairs | 16     | 0.147 ms | 16    | 108.93K   |
| GHZ State  | 16     | 0.132 ms | 16    | 121.44K   |
| Bell Pairs | 20     | 1.458 ms | 20    | 13.72K    |
| GHZ State  | 20     | 1.444 ms | 20    | 13.85K    |

### 4. Random Circuit Benchmark (circuit depth = 20)

| Qubits | Total Gates | Duration | Gates/sec |
|--------|-------------|----------|-----------|
| 8      | 240         | 2.45 ms  | 98.06K    |
| 12     | 360         | 3.57 ms  | 100.82K   |
| 16     | 480         | 7.02 ms  | 68.36K    |
| 18     | 540         | 20.02 ms | 26.97K    |
| 20     | 600         | 83.79 ms | 7.16K     |

### 5. Stress Test: Maximum Qubit Count

| Qubits | State Size (Amps) | Memory (GB) | Status | Init Time | Gate Time |
|--------|-------------------|-------------|--------|-----------|-----------|
| 20     | 1,048,576         | 0.02        | [OK]   | 0.2 ms    | 0.23 ms   |
| 22     | 4,194,304         | 0.06        | [OK]   | 0.8 ms    | 0.78 ms   |
| 24     | 16,777,216        | 0.25        | [OK]   | 0.9 ms    | 2.96 ms   |
| 26     | 67,108,864        | 1.00        | [OK]   | 3.7 ms    | 13.05 ms  |

## macOS Performance (Metal Backend (GPU))

**Date:** 2026-03-02
**Environment:** macOS (Apple M3) - Metal Backend (GPU)

### 1. Scaling Test: State Vector Size vs Performance

| Qubits | State Size  | Memory (KB) | Init Time  | H Gate Time |
|--------|-------------|-------------|------------|-------------|
| 4      | 16          | 0.2         | 28.375 ms  | 0.0299 ms   |
| 8      | 256         | 4.0         | 0.339 ms   | 0.0170 ms   |
| 12     | 4,096       | 64.0        | 0.349 ms   | 0.0239 ms   |
| 16     | 65,536      | 1,024.0     | 0.368 ms   | 0.0224 ms   |
| 18     | 262,144     | 4,096.0     | 2.646 ms   | 0.0270 ms   |
| 20     | 1,048,576   | 16,384.0    | 2.252 ms   | 0.0843 ms   |
| 22     | 4,194,304   | 65,536.0    | 16.166 ms  | 0.3128 ms   |

### 2. Gate Throughput Test (16 qubits, 100 iterations)

| Gate       | Total Gates | Duration | Gates/sec | µs/gate |
|------------|-------------|----------|-----------|---------|
| Hadamard   | 1,600       | 0.0818 s | 19.55K    | 51.14   |
| Pauli-X    | 1,600       | 0.0361 s | 44.30K    | 22.57   |
| Pauli-Y    | 1,600       | 0.0354 s | 45.25K    | 22.10   |
| Pauli-Z    | 1,600       | 0.0342 s | 46.77K    | 21.38   |
| Rotation-Y | 1,600       | 0.0361 s | 44.32K    | 22.56   |
| Rotation-Z | 1,600       | 0.0364 s | 43.95K    | 22.76   |
| CNOT       | 1,500       | 0.0319 s | 46.96K    | 21.29   |

### 3. Entanglement Benchmarks

| Circuit    | Qubits | Time     | Gates | Gates/sec |
|------------|--------|----------|-------|-----------|
| Bell Pairs | 8      | 0.039 ms | 8     | 204.25K   |
| GHZ State  | 8      | 0.041 ms | 8     | 193.55K   |
| Bell Pairs | 12     | 0.065 ms | 12    | 184.85K   |
| GHZ State  | 12     | 0.054 ms | 12    | 220.52K   |
| Bell Pairs | 16     | 0.077 ms | 16    | 207.68K   |
| GHZ State  | 16     | 0.079 ms | 16    | 203.28K   |
| Bell Pairs | 20     | 0.110 ms | 20    | 181.89K   |
| GHZ State  | 20     | 0.204 ms | 20    | 97.96K    |

### 4. Random Circuit Benchmark (circuit depth = 20)

| Qubits | Total Gates | Duration | Gates/sec |
|--------|-------------|----------|-----------|
| 8      | 240         | 4.08 ms  | 58.87K    |
| 12     | 360         | 7.55 ms  | 47.65K    |
| 16     | 480         | 10.01 ms | 47.95K    |
| 18     | 540         | 15.06 ms | 35.86K    |
| 20     | 600         | 72.01 ms | 8.33K     |

### 5. Stress Test: Maximum Qubit Count

| Qubits | State Size (Amps) | Memory (GB) | Status | Init Time | Gate Time |
|--------|-------------------|-------------|--------|-----------|-----------|
| 20     | 1,048,576         | 0.02        | [OK]   | 3.1 ms    | 0.06 ms   |
| 22     | 4,194,304         | 0.06        | [OK]   | 8.1 ms    | 0.06 ms   |
| 24     | 16,777,216        | 0.25        | [OK]   | 30.2 ms   | 0.06 ms   |
| 26     | 67,108,864        | 1.00        | [OK]   | 156.1 ms  | 0.09 ms   |

### 6. C++ Memory Wall Benchmark (Bandwidth)

```text
Unable to determine clock rate from sysctl: hw.cpufrequency: No such file or directory
This does not affect benchmark measurements, only the metadata output.
***WARNING*** Failed to set thread affinity. Estimated CPU frequency may be incorrect.
2026-03-02T10:52:57-08:00
Running /Users/sahil/projects/QubitEngine/benchmarks/build/memory_benchmark
Run on (8 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x8)
Load Average: 2.63, 3.51, 6.15
------------------------------------------------------------------------------
Benchmark                    Time             CPU   Iterations UserCounters...
------------------------------------------------------------------------------
BM_ApplyHadamard/20       1.07 ms        0.469 ms         1496 bytes_per_second=66.6051Gi/s
BM_ApplyHadamard/25       21.6 ms         15.3 ms           45 bytes_per_second=65.5638Gi/s
BM_ApplyHadamard/28       1115 ms          732 ms            1 bytes_per_second=10.9235Gi/s
