# Scientific validation benchmarks

This directory documents **reproducible comparisons** between QubitEngine and published or reference simulators. It complements `benchmarks/` at the repo root, which tracks **performance** (bandwidth, gate throughput), not correctness against external results.

## Automated cross-simulator checks (CI)

State-vector fidelity vs Qiskit Aer is enforced in C++:

| Circuit | Golden file | Test |
|---------|-------------|------|
| Bell (2Q) | `backend/tests/validation/golden/bell.json` | `ValidationTest.CrossSimulatorBell` |
| GHZ (4Q) | `backend/tests/validation/golden/ghz_4q.json` | `ValidationTest.CrossSimulatorGHZ4` |
| QFT (4Q) | `backend/tests/validation/golden/qft_4q.json` | `ValidationTest.CrossSimulatorQFT4` |
| Random depth-5 (4Q) | `backend/tests/validation/golden/random_4q_d5.json` | (generated; extend tests as needed) |

### Regenerating golden vectors

```bash
pip install qiskit qiskit-aer
python scripts/generate_golden_vectors.py
```

Linux CI runs this step before `ctest`. Locally, run it whenever circuits or Qiskit versions change.

### Running validation tests

```bash
cmake --build build --target validation_tests
cd build && ctest -C Release -R ValidationTests --output-on-failure
```

Golden paths are baked in at configure time (`QUBIT_ENGINE_GOLDEN_DIR`); `ctest` does not depend on the current working directory.

---

## Published benchmark reproduction (planned)

The roadmap calls for reproducing **2–3 well-known benchmark circuits** and recording fidelity or sampling statistics next to published numbers. Use this table as a living log; add a subsection per experiment when complete.

| Benchmark | Reference | QubitEngine backend | Status | Notes |
|-----------|-----------|---------------------|--------|-------|
| Random circuit sampling (RCS) | Google quantum supremacy (2019) | MPS / state-vector | **Planned** | Document depth, qubit count, fidelity metric |
| Utility-scale chemistry / dynamics | IBM utility-scale papers | CPU / GPU + noise | **Planned** | Match noise model and shot count |
| Mirror / invertibility stress | Internal | `ValidationTest.MirrorCircuit` | **Done** | See `backend/tests/validation/MirrorCircuitTests.cpp` |

### Template for a completed entry

Create `docs/benchmarks/<name>.md` with:

1. **Citation** — paper, year, figure/table referenced  
2. **Circuit** — OpenQASM file or generator script path  
3. **Reference result** — published fidelity, XEB, or sampling statistic  
4. **QubitEngine command** — CLI, Python, or test target used  
5. **Measured result** — numbers, date, commit hash, platform  
6. **Deviation** — absolute/relative error and known limitations (truncation, noise approximations)

---

## Related code

- Golden generator: `scripts/generate_golden_vectors.py`
- Cross-simulator tests: `backend/tests/validation/CrossSimulatorValidation.cpp`
- Algorithm correctness: `backend/tests/validation/AlgorithmValidation.cpp`
- Performance benchmarks: `benchmarks/benchmark_results.md`
