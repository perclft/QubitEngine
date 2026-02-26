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

## Apple Silicon Performance (Mac)

**Date:** 2026-02-26
**Environment:** macOS (M3 Air) - Metal Backend (GPU)

### 1. Scaling Test: State Vector Size vs Performance

| Qubits | State Size  | Memory (KB) | Init Time  | H Gate Time |
|--------|-------------|-------------|------------|-------------|
| 4      | 16          | 0.2         | 47.636 ms  | 0.0405 ms   |
| 8      | 256         | 4.0         | 0.479 ms   | 0.0172 ms   |
| 12     | 4,096       | 64.0        | 0.519 ms   | 0.0186 ms   |
| 16     | 65,536      | 1,024.0     | 1.041 ms   | 0.0219 ms   |
| 18     | 262,144     | 4,096.0     | 2.837 ms   | 0.0276 ms   |
| 20     | 1,048,576   | 16,384.0    | 12.458 ms  | 0.0873 ms   |
| 22     | 4,194,304   | 65,536.0    | 37.931 ms  | 0.2971 ms   |

### 2. Gate Throughput Test (16 qubits, 100 iterations)

| Gate       | Total Gates | Duration | Gates/sec | µs/gate |
|------------|-------------|----------|-----------|---------|
| Hadamard   | 1,600       | 0.0841 s | 19.02K    | 52.58   |
| Pauli-X    | 1,600       | 0.0352 s | 45.41K    | 22.02   |
| Pauli-Y    | 1,600       | 0.0352 s | 45.51K    | 21.98   |
| Pauli-Z    | 1,600       | 0.0344 s | 46.45K    | 21.53   |
| Rotation-Y | 1,600       | 0.0357 s | 44.77K    | 22.34   |
| Rotation-Z | 1,600       | 0.0356 s | 44.99K    | 22.23   |
| CNOT       | 1,500       | 0.0327 s | 45.82K    | 21.82   |

### 3. Entanglement Benchmarks

| Circuit    | Qubits | Time     | Gates | Gates/sec |
|------------|--------|----------|-------|-----------|
| Bell Pairs | 8      | 0.055 ms | 8     | 146.45K   |
| GHZ State  | 8      | 0.050 ms | 8     | 160.53K   |
| Bell Pairs | 12     | 0.069 ms | 12    | 174.44K   |
| GHZ State  | 12     | 0.070 ms | 12    | 172.04K   |
| Bell Pairs | 16     | 0.088 ms | 16    | 181.22K   |
| GHZ State  | 16     | 0.092 ms | 16    | 174.78K   |
| Bell Pairs | 20     | 0.133 ms | 20    | 149.95K   |
| GHZ State  | 20     | 0.174 ms | 20    | 115.00K   |

### 4. Random Circuit Benchmark (circuit depth = 20)

| Qubits | Total Gates | Duration | Gates/sec |
|--------|-------------|----------|-----------|
| 8      | 240         | 4.44 ms  | 54.10K    |
| 12     | 360         | 7.16 ms  | 50.27K    |
| 16     | 480         | 9.98 ms  | 48.08K    |
| 18     | 540         | 14.37 ms | 37.57K    |
| 20     | 600         | 80.65 ms | 7.44K     |

### 5. Stress Test: Maximum Qubit Count

| Qubits | State Size (Amps) | Memory (GB) | Status | Init Time | Gate Time |
|--------|-------------------|-------------|--------|-----------|-----------|
| 20     | 1,048,576         | 0.02        | [OK]   | 9.1 ms    | 0.07 ms   |
| 22     | 4,194,304         | 0.06        | [OK]   | 38.0 ms   | 0.05 ms   |
| 24     | 16,777,216        | 0.25        | [OK]   | 165.4 ms  | 0.06 ms   |
| 26     | 67,108,864        | 1.00        | [OK]   | 837.6 ms  | 0.11 ms   |

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
