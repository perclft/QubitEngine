# Optimization Status Report

## Metal Support (GPU)

**Status**: FAILED (System Environment Issue)

**Diagnosis**:
The `metal` compilation toolchain is missing or corrupted on this system, despite Xcode Command Line Tools being installed.

- `xcrun -sdk macosx metal` -> "missing Metal Toolchain"
- Direct execution of binary -> "missing Metal Toolchain"

**Resolution**:
Since we cannot fix the system environment (requires `sudo` or interactive `xcodebuild`), we are pivoting to **CPU optimizations**.

## Memory Wall Optimization

**Strategy**: Switch from Double Precision (`complex<double>`) to Single Precision (`complex<float>`).
**Rationale**:

- Reduces memory traffic by 50% (16 bytes -> 8 bytes per amplitude).
- Should theoretically double the effective bandwidth for memory-bound quantum operations like `applyHadamard`.
- Sufficient precision for most NISQ-era algorithms.
