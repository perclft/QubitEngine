package ipc

import (
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"testing"
)

// maybeFatalSkip skips the test locally, but fails it when CI=true so that a
// missing binary can never produce a green CI run.
func maybeFatalSkip(t *testing.T, msg string) {
	t.Helper()
	if os.Getenv("CI") == "true" {
		t.Fatalf("%s", msg)
	}
	t.Skip(msg)
}

// binPath returns the full path to a binary in the project's bin directory.
// name is the base filename without extension. bin/Release is checked first,
// then bin, so a stale copy in bin/ never wins over a fresh Release build.
func binPath(t *testing.T, name string) string {
	t.Helper()
	_, currentFile, _, _ := runtime.Caller(0)
	rootDir := filepath.Join(filepath.Dir(currentFile), "..", "..")
	ext := ""
	if runtime.GOOS == "windows" {
		ext = ".exe"
	}
	for _, dir := range []string{filepath.Join("bin", "Release"), "bin"} {
		p := filepath.Join(rootDir, dir, name+ext)
		if _, err := os.Stat(p); err == nil {
			return p
		}
	}
	maybeFatalSkip(t, name+ext+" binary not found under bin/Release or bin")
	return ""
}

// mpiExecPath returns the path to the MPI launcher appropriate for the platform.
func mpiExecPath(t *testing.T) string {
	t.Helper()
	if runtime.GOOS == "windows" {
		// Prefer mpiexec.exe or mpirun.exe from PATH.
		for _, cand := range []string{"mpiexec.exe", "mpirun.exe"} {
			if p, err := exec.LookPath(cand); err == nil {
				return p
			}
		}
		// Common install locations for MS-MPI.
		for _, p := range []string{
			`C:\Program Files\Microsoft MPI\Bin\mpiexec.exe`,
			`C:\Program Files (x86)\Microsoft MPI\Bin\mpiexec.exe`,
		} {
			if _, err := os.Stat(p); err == nil {
				return p
			}
		}
		maybeFatalSkip(t, "mpiexec not found on Windows")
		return ""
	}

	// Non-Windows: OpenMPI/MPICH provide mpirun and/or mpiexec.
	for _, cand := range []string{"mpirun", "mpiexec"} {
		if p, err := exec.LookPath(cand); err == nil {
			return p
		}
	}
	maybeFatalSkip(t, "mpirun/mpiexec not found on PATH")
	return ""
}
