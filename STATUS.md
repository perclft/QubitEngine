# STATUS.md – Phase 5 Sessions

| Session | Description | Status |
|---------|-------------|--------|
| **Session 0 – Snapshot & Setup** | Commit current tree, split if needed, delete leftover debris, create AGENTS.md & STATUS.md. | **Done** |
| **Session 1 – Linux/OpenMPI/POSIX verification (NEW‑07, NEW‑08)** | Add CI to run real Linux CI, execute MPI & SHM multiprocess tests on Linux, record results. | **In progress** |
| **Session 2 – Cross‑boundary integration suite (Task 2.1)** | Design Go scheduler worker pool, write failing‑first tests, implement workers, add Rust scheduler support, create cross‑process boundary test. | **In progress** |
| **Session 3 – Dead‑code & orphaned‑claim detection scripts** | Wire `check_dead_code.py` and `check_orphaned_claims.py` into CI, ensure scripts run on both OSes. | **In progress** |
| **Session 4 – Theatrical test fixes** | Fix JIT O4 bug (already resolved), ensure VQE precondition tests, ReverseCNOT, etc. pass. | **In progress** |
| **Session 5 – Move slow SHM benchmark out of default ctest** | Remove `PerStepLatencyBenchmark` from default test suite, ensure benchmark builds as separate target. | **In progress** |

All remaining work will continue in subsequent sessions, respecting the standing rules (no synthetic verification, fail‑first tests, out‑of‑scope findings → FINDINGS_NEW.md).