This is a comprehensive, modular CONTRIBUTING.md template designed for systems-level software

It assumes a high level of technical competence from contributors, enforces strict version control hygiene (Conventional Commits), and prioritizes performance/testing standards.

Contributing Guidelines
Thank you for your interest in contributing. This document outlines the standards and workflows required to maintain the stability and performance of this project.

1. Governance & Code of Conduct
This project adheres to a strict code of conduct. We value technical merit, clarity, and mutual respect.

Zero Tolerance: Harassment or discrimination of any kind is not tolerated.

Scope: This applies to issues, pull requests, and associated communication channels.

Please review the CODE_OF_CONDUCT.md [Link] before contributing.

2. Development Workflow
We utilize a Fork & Pull workflow.

Branching Strategy
main / master: The stable production branch. Do not commit directly.

Feature Branches: Create branches from main using the format: type/short-description.

Examples: feat/quantum-gate-logic, fix/memory-leak-tensor, refactor/scheduler.

Environment & Dependencies
Ensure your local environment matches the Dockerfile or nix-shell configuration (if applicable).

Rust: strict adherence to cargo fmt and cargo clippy.

Go: strict adherence to gofmt.

3. Pull Request (PR) Protocol
All PRs must meet the following criteria to be considered for review:

Atomic Commits: Squash intermediate "wip" or "fix" commits before requesting a review.

Pass CI/CD: All automated workflows (GitHub Actions/GitLab CI) must pass.

Test Coverage:

New features must include unit tests.

Bug fixes must include a regression test.

Performance Impact:

For simulation or systems code, include benchmark results in the PR description comparing base vs head.

Commit Message Convention
We enforce Conventional Commits. This enables automated semantic versioning and changelog generation.

Structure:

Plaintext

<type>(<scope>): <subject>

<body>

<footer>
Types:

feat: A new feature

fix: A bug fix

perf: A code change that improves performance

docs: Documentation only changes

refactor: A code change that neither fixes a bug nor adds a feature

test: Adding missing tests or correcting existing tests

chore: Changes to the build process or auxiliary tools

Example: perf(engine): optimize state-vector multiplication via SIMD intrinsics

4. Issue Reporting
Bug Reports
Do not open an issue without a Minimal, Reproducible Example (MRE).

Environment: OS, Architecture, Toolchain version.

Logs: Stack traces or panic outputs (use code blocks).

Reproduction: A snippet of code or command sequence that triggers the failure.

Feature Requests (RFCs)
For significant architectural changes or new features:

Open an issue titled RFC: <Proposal Name>.

Detail the motivation (why is this needed?).

Detail the design (interface changes, data structures, complexity analysis).

5. Style Guides
Rust
Follow the Rust API Guidelines.

Macros should be documented clearly.

unsafe blocks must have a comment explaining the invariant being upheld (// SAFETY: ...).

Go
Follow Effective Go.

Error handling must be explicit; do not ignore returned errors.

6. Licensing & DCO
By contributing, you agree that your code is licensed under the project's license (MIT/Apache 2.0).

Developer Certificate of Origin (DCO): All commits must be signed off to certify you have the right to submit the code.

Use git commit -s to automatically sign-off.
