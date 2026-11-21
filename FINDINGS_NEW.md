# Additional Findings (Post-Review Audit)

This document tracks issues discovered during remediation that were not part of the original review finding list.

---

### Finding NEW-01: CPU Adjoint Differentiation Omits S and T Gate Inverses

- **Location**: `backend/src/QuantumDifferentiator.cpp` (`applyGateInverseToState`)
- **Severity**: High (Silent Numerical Corruption)
- **Discovered During**: Phase 0 adjudication of S/T inverse phase claim

#### Description
In `QuantumDifferentiator.cpp`, the CPU adjoint differentiation backward pass calls `applyGateInverseToState` to un-apply gates from both ket $|\psi\rangle$ and bra $|\lambda\rangle$. The switch statement in `applyGateInverseToState` handled rotation gates and Clifford gates (`H`, `X`, `Y`, `Z`, `CNOT`, `SWAP`, `CZ`, `RY`, `RX`, `RZ`), but omitted `RecordedGate::PHASE_S` and `RecordedGate::PHASE_T`, hitting `default: break;`.

As a consequence, in any circuit where $S$ or $T$ gates appeared after parameterized gates, CPU adjoint differentiation silently failed to invert $S$ and $T$, producing incorrect gradients without raising an error:
- $R_Y(\theta) \to S$ with observable $X$: Adjoint CPU gave `+0.82533561` vs Finite Difference `+0.00000000` (divergence: `0.82533561`).
- $R_Y(\theta) \to S$ with observable $Z$: Adjoint CPU gave `-0.00000000` vs Finite Difference `-0.56464247` (divergence: `0.56464247`).
- $R_Y(\theta) \to T$ with observable $X$: Adjoint CPU gave `+0.82533561` vs Finite Difference `+0.58360041` (divergence: `0.24173520`).
- $R_Y(\theta) \to S \to T$ with observable $Z$: Adjoint CPU gave `+0.39926252` vs Finite Difference `-0.56464247` (divergence: `0.96390499`).

#### Resolution
Added `RecordedGate::PHASE_S` and `RecordedGate::PHASE_T` handlers to `applyGateInverseToState` in `QuantumDifferentiator.cpp`, applying the corresponding $R_Z(-\pi/2)$ and $R_Z(-\pi/4)$ rotations. Because both bra and ket evolve through the same path, the global phase factors cancel and gradients match finite difference to machine precision ($< 10^{-15}$).

#### Tests
- Regression test added: `AdjointTest.PhaseSandT_MatchesFiniteDifference` in `backend/tests/AdjointTests.cpp` (verifies adjoint CPU vs central finite difference $\le 10^{-6}$).

---

### Finding NEW-02: MPSBackend Reduced Density Matrix Assumes Unverified Canonical Form

- **Location**: `backend/src/backends/MPSBackend.cpp` (`getReducedDensityMatrix1Q`, `getReducedDensityMatrix2Q`)
- **Severity**: Medium (Numerical Inaccuracy on Uncanonicalized MPS States)
- **Discovered During**: Phase 1 scope verification of MPS full contraction

#### Description
`MPSBackend::getReducedDensityMatrix1Q` and `getReducedDensityMatrix2Q` trace out the environment of target qubits by assuming that uncontracted environment tensors evaluate to identity matrices $\mathbb{I}$. This assumption holds strictly if and only if the MPS has been swept into mixed-canonical form (with the target site at the orthogonality center) via QR or SVD gauge sweeps. 

In `MPSBackend`, gates and noise channels update local tensors via SVD truncation without a full canonicalization sweep. Consequently, calling `getReducedDensityMatrix1Q` or `2Q` after two-qubit operations can produce unnormalized or inaccurate reduced density matrices.

#### Status
Open. In Phase 1, `MPSBackend::getProbabilities()` was resolved by switching to full state-vector contraction (`getStateVector()`), which evaluates the complete tensor chain directly and is mathematically invariant to the internal gauge (completely sidestepping the canonical-form requirement). However, `getReducedDensityMatrix1Q` and `getReducedDensityMatrix2Q` retain the uncanonicalized environment shortcut and remain to be updated when full mixed-canonical gauge sweeps or MPO contraction routines are implemented.

---

### Finding NEW-03: Committed Dummy Secret History Retention vs. Rewrite Decision

- **Location**: `deploy/k8s/auth-secret.yaml` (commit `c8e77b21`)
- **Severity**: Low (Repository Cleanliness / Scanner False Positive)
- **Discovered During**: Phase 2 review of Kubernetes secrets

#### Description
`deploy/k8s/auth-secret.yaml` originally contained the base64 string `dGhpcy1pcy1hLXJlYWwtc2VjcmV0LXRva2Vu`, which decodes to the literal string `"this-is-a-real-secret-token"`. Git archaeology confirms this was introduced in commit `c8e77b21` during PR setup as a mock testing placeholder, not an active credential in any infrastructure or external system.

#### Resolution & Decision
- **Decision**: Accept the dead token in historical commits as a permanent, confirmed-harmless artifact rather than performing a history rewrite (`git filter-repo` / `git filter-branch`). Rewriting history would alter all commit SHAs from `c8e77b21` to `HEAD`, break all contributor forks/branches, and require force-pushing.
- **Interim & Working Tree Mitigation**: Replaced the working tree file `deploy/k8s/auth-secret.yaml` with a sanitized `stringData` template with clear placeholders, and wired `envFrom` into `engine-deployment.yaml`.

---

### Finding NEW-04: MPI Partitioned State Simulation Test Limitation

- **Location**: `backend/tests/CpuBackendTests.cpp` (`CpuBackendTest.NoiseChannelGlobalQubitStrideCheck`)
- **Severity**: Informational (Test Architecture Limitation)
- **Status**: Closed / Superseded (Phase 4)
- **Discovered During**: Phase 2 verification of MPI noise stride protection

#### Description
The unit test `CpuBackendTest.NoiseChannelGlobalQubitStrideCheck` verifies that target qubits exceeding local state partition stride (`(1ULL << target) >= state.size()`) throw `FeatureNotSupportedException` rather than executing an out-of-bounds stride loop. The test constructs this partitioned state using the test constructor `CpuBackend(num_qubits, local_rank, world_size)`.

#### Architectural Note (Superseded)
This test verified the mathematical stride boundary condition on a partitioned local state slice, but it was a synthetic constructor bypass; the real MPI init path's state partitioning and multi-node all-to-all communication remained unverified by unit tests without an active `mpirun` runner.

#### Phase 4 Resolution (Closed / Superseded)
In Phase 4, the real MPI initialization and state-partitioning path (`MPI_Init_thread`, `MPI_Comm_rank`, `MPI_Comm_size`, and lockstep SPMD broadcast loop) was implemented in `MpiCoordinator` and fully verified under live multi-process conditions via MS-MPI (`mpiexec.exe -n 2`) in `pkg/ipc/mpi_multiprocess_test.go`. The 6-test suite exercises real multi-rank state partitioning, global gate lockstep execution, global CNOT partner exchanges (`MPI_Sendrecv`), `MPI_Gather` state reconstruction, dynamic watchdog aborts, and cluster lock contention across separate OS processes without synthetic bypasses. This finding is formally closed and superseded by the live multi-process test suite.

---

### Finding NEW-05: Fabricated MPI Distributed Benchmark Figure and Dummy Script

- **Location**: `benchmarks/run_mpi_benchmark.sh`, `README.md:82`
- **Severity**: High (Fabricated Metric in Documentation & Dummy Benchmark Script)
- **Discovered During**: Phase 3 documentation honesty audit

#### Description
`README.md` previously claimed an **MPI (n=2)** distributed simulation bandwidth of **3.15 GB/s** for a 22-qubit state vector. Audit of the benchmark harness revealed that `benchmarks/run_mpi_benchmark.sh` does not execute any simulation workload, gate operation, or memory transfer. The core execution in the script consists of:
```bash
mpirun --allow-run-as-root -n 2 "$ENGINE_BIN" &
ENGINE_PID=$!
sleep 5 # Allow the cluster to form and simulate
kill -9 $ENGINE_PID
```
The script spawns the gRPC server daemon, idles for 5 seconds, kills the process, and unconditionally prints `"MPI Benchmark completed successfully!"`. No data volume, wall-clock duration, or throughput metric is calculated or reported.

Unlike the Metal/NEON benchmark entry (where valid 19.03 GB/s experimental data from `AntigravityMemoryBenchmark.cpp` on Apple Silicon was misattributed to the Metal backend instead of the ARM NEON CPU kernel), the 3.15 GB/s MPI figure was never measured under any experimental condition and was entirely fabricated.

#### Resolution
1. Deleted the 3.15 GB/s MPI bandwidth row from `README.md` in Phase 3.
2. Logged this finding to ensure the dummy benchmark script is replaced with a real distributed throughput measurement when the MPI SPMD execution path is integrated in Phase 4.

---

### Finding NEW-06: Distributed Multi-Node VQE Scoped to Single-Node Execution

- **Location**: `backend/src/services/VQEService.cpp`, `backend/src/QuantumDifferentiator.cpp`
- **Severity**: Medium (Scope Boundary / Unimplemented Multi-Node VQE)
- **Discovered During**: Phase 4 architectural review of MPI SPMD lifecycle

#### Description
In `QuantumDifferentiator.cpp`, `calculateGradients` partitions parameter evaluations across ranks using `QuantumRegister(n, /*force_local=*/true)` followed by `MPI_Allreduce`. However:
1. `calculateGradientsAdjoint` (Adjoint differentiation) has no distributed collective implementation.
2. When invoked through gRPC (`VQEService::RunVQE`), the optimization loop and ansatz evaluations run in user/service callbacks on Rank 0 alone. Ranks > 0 never participate in the VQE optimization loop, leading to all-zero gradients or deadlocks if collective calls are reached.

#### Resolution & Scope Boundary
In Phase 4, distributed MPI integration is strictly scoped to circuit state-vector execution (`RunCircuit`). Multi-node distributed VQE is formally out of scope for Phase 4. When `world_size > 1`, `RunVQE` rejects requests with `FAILED_PRECONDITION` ("Distributed multi-node VQE optimization is not integrated in Phase 4; submit VQE workloads to single-node engine instances"), preventing deadlocks or silent numerical errors.

---

### Finding NEW-07: Windows-Only Multi-Process SHM Verification (Linux/POSIX Verification Outstanding)

- **Location**: `backend/src/ipc/SharedMemory.cpp` (POSIX `shm_open`/`mmap`), `pkg/ipc/shm_posix.go`, `cli-rs/src/grpc.rs`
- **Severity**: Informational (Environment Test Scope Limitation)
- **Discovered During**: Phase 4 Track A multi-process verification audit

#### Description
All multi-process integration tests and fault-injection suites for Track A (`TestMultiProcessSHM_EndToEnd`, `TestMultiProcessSHM_ReaderCrashFaultInjection`, `TestMultiProcessSHM_WriterCrashFaultInjection`, `TestMultiProcessSHM_RustClient`, `TestMultiProcessSHM_UnauthenticatedRequestRedaction`, and `TestMultiProcessSHM_BackpressureFallbackUnderLoad`) were executed against live child processes on Windows. These tests exercise the Win32 kernel file-mapping APIs (`CreateFileMappingA`, `OpenFileMappingA`, `MapViewOfFile`, `UnmapViewOfFile`, `CloseHandle`) under the `Local\qe_shm_...` namespace.

While the corresponding POSIX implementation paths (`shm_open`, `mmap`, `munmap`, `shm_unlink` under `/qe_shm_...`) are implemented across C++, Go (`pkg/ipc/shm_posix.go`), and Rust (`cli-rs`), they remain unexercised by multi-process integration tests in this verification suite because the test host is Windows.

#### Status & Implication
Open. Linux/POSIX multi-process verification remains outstanding. This is directly relevant to Track B and Kubernetes deployments, where the engine containers run on Linux. Verification of POSIX SHM IPC semantics (including permissions, descriptor cleanup on SIGKILL, and container shared memory `/dev/shm` sizing) must be completed in a Linux environment or containerized CI job prior to production Linux deployment.

---

### Finding NEW-08: Windows/MS-MPI-Only Distributed Test Coverage (Linux/OpenMPI Verification Outstanding)

- **Location**: `backend/src/mpi/MpiCoordinator.cpp`, `pkg/ipc/mpi_multiprocess_test.go`, `deploy/helm/qubit-engine/templates/mpijob.yaml`
- **Severity**: Medium (Environment & Implementation Scope Limitation)
- **Status**: Open
- **Discovered During**: Phase 4 Track B distributed multi-process verification audit

#### Description
All multi-process MPI integration and fault-injection tests in Phase 4 (`TestMultiProcessMPI_GlobalQubitGateLockstep`, `TestMultiProcessMPI_DualGlobalCNOT`, `TestMultiProcessMPI_DeadlockFreeRepeatedCollectives`, `TestMultiProcessMPI_VQEPreconditionRejection`, `TestMultiProcessMPI_WatchdogFaultInjection`, and `TestMultiProcessMPI_ClusterExecutionLockContention`) were executed on Windows using Microsoft MPI (`mpiexec.exe -n 2`, MS-MPI v10.1+).

However, the production Kubernetes deployment target (`deploy/helm/qubit-engine/templates/mpijob.yaml` and Linux container images) runs OpenMPI (4.x / 5.x) on Linux. Signal propagation, process group teardown, hang detection, and `MPI_Abort` semantics are not guaranteed identical across MPI implementations:
1. Under MS-MPI on Windows, `MPI_Abort` forcibly terminates all rank processes and closes network sockets immediately.
2. Under OpenMPI on Linux, `MPI_Abort` interacts with ORTE/PRTE daemon monitors (`orted`/`prted`), and signal delivery (`SIGABRT`/`SIGKILL`) across pod boundaries depends on container runtime signal forwarding and PMIx wireup.

Furthermore, the dynamic watchdog's payload scaling ($T = 15\text{s} + \lceil \text{bytes} / 100\text{ MB/s} \rceil$) and abort behavior have not been exercised under Linux container networking (e.g. CNI overlays like Calico, Cilium, or Flannel) or with OpenMPI multi-node interconnects (TCP / RoCE / InfiniBand).

#### Status & Implication
Open. Multi-node SPMD execution, dynamic watchdog aborts, and fault-injection behavior are unverified under OpenMPI on Linux. Running the distributed integration suite inside an OpenMPI Linux container environment or automated Kubernetes CI job is required prior to production Linux deployment.



