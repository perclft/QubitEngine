# Branching Strategy

This project follows a strict branching strategy to ensure stability across different platforms while maintaining a rapid development cycle.

## Core Branches

### `main`
- **Purpose**: The stable release branch.
- **Protection**: Direct commits are forbidden. All changes must come via Pull Requests (PRs).
- **CI/CD**: Deploys to production/staging environment. must pass all tests on Linux and Windows.

### `develop`
- **Purpose**: Integration branch for new features.
- **Stability**: Should be stable enough for other developers to base work on.

## Platform-Specific Branches

Given the complex native dependencies (C++, CUDA, Direct3D, etc.), we maintain long-lived feature branches for platform porting efforts until they are fully stable.

### `feature/windows-port`
- **Purpose**: Development of Windows-native support (MSVC, PowerShell, VCPKG).
- **Upstream**: Rebase frequently from `main` to keep up with core engine changes.
- **Merge Criteria**:
    - Build passes on Windows (MSVC 2022).
    - All unit tests pass.
    - No regression in Linux build (verified via CI).

### `feature/mac-port`
- **Purpose**: Development of macOS support (Clang, Homebrew/Conan).

## Workflow

1. **New Feature**: Create `feature/my-feature` from `develop`.
2. **Platform Specifics**: If your change touches `backend/src` and relies on OS syscalls:
    - Implement the portable interface in `backend/src/common`.
    - Add implementation to `backend/src/platform/windows` or `backend/src/platform/linux`.
    - Do NOT use `#ifdef _WIN32` spaghetti code if possible. Prefer polymorphism or separate files.
3. **Pull Request**:
    - Open PR to `develop`.
    - Ensure `scripts/build.ps1` (Windows) and `Makefile` (Linux) both pass.

## Versioning

We adhere to [Semantic Versioning 2.0.0](https://semver.org/).

- **Major (X.y.z)**: Breaking API changes or massive architectural shifts.
- **Minor (x.Y.z)**: New features (e.g., new Quantum Gates, VQE optimizer).
- **Patch (x.y.Z)**: Bug fixes, performance improvements, internal refactoring.

Current Version: Read from [VERSION](../VERSION) file.
