# AGENTS.md – Standing Rules & Build Commands

## Standing Rules (Phase 5 remediation)
1. **No synthetic bypasses** – verification must be performed with real multi‑process or cross‑boundary runs. A test that merely simulates a condition does *not* count as verified.
2. **Fail‑first tests** – every new invariant must be backed by a test that initially fails, then is made to pass by the implementation.
3. **Out‑of‑scope findings** – any discovery that is not part of the current session’s scope goes into `FINDINGS_NEW.md` and is not fixed in‑session.
4. **Commit hygiene** – large mixed commits must be split into logical units (per‑phase or per‑track) to allow selective reverts.
5. **Continuous integration** – CI scripts must run the real tests (no `t.Skip` on CI, no silent skips).

## One‑line Build & Test Commands
| Language | Build Command | Test Command |
|----------|--------------|--------------|
| **C++** | `cmake -S backend -B backend/build -DCMAKE_BUILD_TYPE=Release && cmake --build backend/build --config Release` | `ctest --test-dir backend/build -C Release` |
| **Go**   | `go build ./...` (build all Go modules) | `go test ./... -count=1` |
| **Rust** | `cargo build --manifest-path cli-rs/Cargo.toml` | `cargo test --manifest-path cli-rs/Cargo.toml` |
| **Python** | `python -m pip install -r python/requirements.txt` (if needed) | `python -m pytest python/tests` |

*All commands are intended to be run from the repository root (`C:\Users\percl\projects\QubitEngine`).*