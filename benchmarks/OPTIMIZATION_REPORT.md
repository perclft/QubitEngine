# Optimization Status Report

## Metal Support (GPU)

**Status**: RESOLVED

**Diagnosis**:
The `metal` compilation toolchain was missing.

**Resolution**:
Running `xcodebuild -downloadComponent MetalToolchain` successfully downloaded and installed the missing components. Metal shader compilation is now enabled and verified with `verify_metal` and benchmarks.

## Memory Wall Optimization

**Strategy**: Switch from Double Precision (`complex<double>`) to Single Precision (`complex<float>`).
**Rationale**:

- Reduces memory traffic by 50% (16 bytes -> 8 bytes per amplitude).
- Should theoretically double the effective bandwidth for memory-bound quantum operations like `applyHadamard`.
- Sufficient precision for most NISQ-era algorithms.
