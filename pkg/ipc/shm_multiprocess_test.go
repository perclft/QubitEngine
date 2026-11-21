package ipc

import (
	"bufio"
	"context"
	"math"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
	"time"

	"github.com/perclft/QubitEngine/api/auth"
	pb "github.com/perclft/QubitEngine/api/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

func findBinaries(t *testing.T) (string, string) {
	enginePath := binPath(t, "qubit_engine")
	readerToolPath := binPath(t, "shm_reader_tool")
	return enginePath, readerToolPath
}

func startEngine(t *testing.T, enginePath string, secret string) (*exec.Cmd, func()) {
	cmd := exec.Command(enginePath)
	cmd.Dir = filepath.Dir(enginePath)
	cmd.Env = append(os.Environ(),
		"QUBIT_ENGINE_JWT_SECRET="+secret,
		"QUBIT_ENGINE_SKIP_AUTH=0",
		"WORKER_COUNT=0",
	)
	
	// Create pipe for engine logs
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	if err := cmd.Start(); err != nil {
		t.Fatalf("Failed to start engine subprocess: %v", err)
	}

	cleanup := func() {
		if cmd.Process != nil {
			_ = cmd.Process.Kill()
			_ = cmd.Wait()
		}
	}

	// Wait up to 5s for TCP server on 127.0.0.1:50051
	deadline := time.Now().Add(5 * time.Second)
	ready := false
	for time.Now().Before(deadline) {
		conn, err := net.DialTimeout("tcp", "127.0.0.1:50051", 200*time.Millisecond)
		if err == nil {
			conn.Close()
			ready = true
			break
		}
		time.Sleep(100 * time.Millisecond)
	}

	if !ready {
		cleanup()
		t.Fatalf("Engine subprocess did not become ready within 5 seconds")
	}

	return cmd, cleanup
}

func TestMultiProcessSHM_EndToEnd(t *testing.T) {
	enginePath, readerToolPath := findBinaries(t)
	jwtSecret := "test_secret_key_multi_process_shm_12345"
	os.Setenv("QUBIT_ENGINE_JWT_SECRET", jwtSecret)

	_, cleanupEngine := startEngine(t, enginePath, jwtSecret)
	defer cleanupEngine()

	token, err := auth.GenerateToken("test-client", 1*time.Hour)
	if err != nil {
		t.Fatalf("Failed to generate test token: %v", err)
	}

	// Run reader tool subprocess: 14 qubits (256 KB state vector payload)
	cmdReader := exec.Command(readerToolPath, "127.0.0.1:50051", token, "14")
	out, err := cmdReader.CombinedOutput()
	outputStr := string(out)

	if err != nil {
		t.Fatalf("Reader subprocess failed with exit code %v:\n%s", err, outputStr)
	}

	if !strings.Contains(outputStr, "[RESULT:SUCCESS]") {
		t.Fatalf("Reader subprocess did not report success:\n%s", outputStr)
	}
	t.Logf("Reader subprocess output:\n%s", outputStr)
}

func TestMultiProcessSHM_ReaderCrashFaultInjection(t *testing.T) {
	enginePath, readerToolPath := findBinaries(t)
	jwtSecret := "test_secret_key_fault_injection_12345"
	os.Setenv("QUBIT_ENGINE_JWT_SECRET", jwtSecret)

	_, cleanupEngine := startEngine(t, enginePath, jwtSecret)
	defer cleanupEngine()

	token, err := auth.GenerateToken("test-client", 1*time.Hour)
	if err != nil {
		t.Fatalf("Failed to generate test token: %v", err)
	}

	// Launch reader subprocess with QUBIT_SHM_TEST_HOOK=pause_after_map
	cmdReader := exec.Command(readerToolPath, "127.0.0.1:50051", token, "14")
	cmdReader.Env = append(os.Environ(), "QUBIT_SHM_TEST_HOOK=pause_after_map")

	stdoutPipe, err := cmdReader.StdoutPipe()
	if err != nil {
		t.Fatalf("Failed to get stdout pipe: %v", err)
	}
	cmdReader.Stderr = os.Stderr

	if err := cmdReader.Start(); err != nil {
		t.Fatalf("Failed to start reader subprocess: %v", err)
	}

	// Scan reader stdout until [SYNC:MAPPED_AWAITING_ACK] is detected
	scanner := bufio.NewScanner(stdoutPipe)
	syncDetected := false
	for scanner.Scan() {
		line := scanner.Text()
		t.Logf("[Reader Subprocess stdout] %s", line)
		if strings.Contains(line, "[SYNC:MAPPED_AWAITING_ACK]") {
			syncDetected = true
			break
		}
	}

	if !syncDetected {
		_ = cmdReader.Process.Kill()
		t.Fatalf("Did not detect [SYNC:MAPPED_AWAITING_ACK] from reader subprocess")
	}

	// Reader has mapped the segment and is blocked awaiting ACK.
	// Now simulate hard reader crash: KILL reader process
	t.Log("Synchronized: reader mapped segment. Hard-killing reader process...")
	if err := cmdReader.Process.Kill(); err != nil {
		t.Fatalf("Failed to kill reader subprocess: %v", err)
	}
	_ = cmdReader.Wait()
	t.Log("Reader process terminated.")

	// Wait 5.3 seconds for writer's 5.0-second safety cleanup timer to fire
	t.Log("Waiting 5.3 seconds for writer 5s safety cleanup timer to unlink segment...")
	time.Sleep(5300 * time.Millisecond)

	// Verify engine is healthy by running another SHM operation through a fresh reader process
	t.Log("Verifying engine SHM path is healthy and unblocked after reader crash...")
	cmdVerify := exec.Command(readerToolPath, "127.0.0.1:50051", token, "14")
	out, err := cmdVerify.CombinedOutput()
	outputStr := string(out)
	if err != nil {
		t.Fatalf("Engine SHM path failed after reader crash: %v\n%s", err, outputStr)
	}
	if !strings.Contains(outputStr, "[RESULT:SUCCESS]") {
		t.Fatalf("Engine SHM path did not succeed after reader crash:\n%s", outputStr)
	}
	t.Log("Engine successfully unlinked abandoned segment and served subsequent SHM request!")
}

func TestMultiProcessSHM_RustClient(t *testing.T) {
	_, currentFile, _, _ := runtime.Caller(0)
	rootDir := filepath.Join(filepath.Dir(currentFile), "..", "..")

	enginePath, _ := findBinaries(t)
	rustBinPath := filepath.Join(rootDir, "cli-rs", "target", "debug", "cli-rs.exe")
	if runtime.GOOS != "windows" {
		rustBinPath = filepath.Join(rootDir, "cli-rs", "target", "debug", "cli-rs")
	}

	if _, err := os.Stat(rustBinPath); err != nil {
		t.Skip("Rust cli-rs binary not found. Skipping Rust multi-process test.")
	}

	circuitPath := filepath.Join(rootDir, "circuits", "shm_14q_test.json")

	jwtSecret := "test_secret_key_rust_shm_12345"
	os.Setenv("QUBIT_ENGINE_JWT_SECRET", jwtSecret)

	_, cleanupEngine := startEngine(t, enginePath, jwtSecret)
	defer cleanupEngine()

	token, err := auth.GenerateToken("test-rust-client", 1*time.Hour)
	if err != nil {
		t.Fatalf("Failed to generate test token: %v", err)
	}

	cmdRust := exec.Command(rustBinPath,
		"--endpoint", "http://127.0.0.1:50051",
		"--headless-circuit", circuitPath,
	)
	cmdRust.Env = append(os.Environ(),
		"QUBIT_ENGINE_AUTH_TOKEN="+token,
	)

	out, err := cmdRust.CombinedOutput()
	outputStr := string(out)

	if err != nil {
		t.Fatalf("Rust client failed with exit code %v:\n%s", err, outputStr)
	}

	if !strings.Contains(outputStr, "[RUST_SHM:SUCCESS]") {
		t.Fatalf("Rust client did not report success:\n%s", outputStr)
	}
	t.Logf("Rust client output:\n%s", outputStr)
}

func TestMultiProcessSHM_WriterCrashFaultInjection(t *testing.T) {
	enginePath, _ := findBinaries(t)
	jwtSecret := "test_secret_key_writer_crash_12345"
	os.Setenv("QUBIT_ENGINE_JWT_SECRET", jwtSecret)

	cmd := exec.Command(enginePath)
	cmd.Dir = filepath.Dir(enginePath)
	cmd.Env = append(os.Environ(),
		"QUBIT_ENGINE_JWT_SECRET="+jwtSecret,
		"QUBIT_ENGINE_SKIP_AUTH=0",
		"WORKER_COUNT=0",
		"QUBIT_SHM_TEST_HOOK=pause_before_response",
	)

	stdoutPipe, err := cmd.StdoutPipe()
	if err != nil {
		t.Fatalf("Failed to get engine stdout pipe: %v", err)
	}
	cmd.Stderr = os.Stderr

	if err := cmd.Start(); err != nil {
		t.Fatalf("Failed to start engine subprocess: %v", err)
	}
	defer func() {
		if cmd.Process != nil {
			_ = cmd.Process.Kill()
			_ = cmd.Wait()
		}
	}()

	// Wait up to 5s for TCP server on 127.0.0.1:50051
	deadline := time.Now().Add(5 * time.Second)
	ready := false
	for time.Now().Before(deadline) {
		conn, err := net.DialTimeout("tcp", "127.0.0.1:50051", 200*time.Millisecond)
		if err == nil {
			conn.Close()
			ready = true
			break
		}
		time.Sleep(100 * time.Millisecond)
	}
	if !ready {
		t.Fatalf("Engine did not become ready")
	}

	token, err := auth.GenerateToken("test-client", 1*time.Hour)
	if err != nil {
		t.Fatalf("Failed to generate token: %v", err)
	}

	conn, err := grpc.NewClient("127.0.0.1:50051", grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumComputeClient(conn)

	// In a background goroutine, dispatch RunCircuit
	errChan := make(chan error, 1)
	go func() {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()
		ctx = metadata.AppendToOutgoingContext(ctx, "authorization", "Bearer "+token)

		req := &pb.CircuitRequest{
			NumQubits:           14,
			MeasurementStrategy: pb.CircuitRequest_FULL_STATE,
			UseShm:              true,
			Operations: []*pb.GateOperation{
				{Type: pb.GateOperation_HADAMARD, TargetQubit: 0},
			},
		}
		_, err := client.RunCircuit(ctx, req)
		errChan <- err
	}()

	// Scan engine stdout for [SYNC:SEGMENT_CREATED]
	scanner := bufio.NewScanner(stdoutPipe)
	syncDetected := false
	for scanner.Scan() {
		line := scanner.Text()
		t.Logf("[Engine stdout] %s", line)
		if strings.Contains(line, "[SYNC:SEGMENT_CREATED]") {
			syncDetected = true
			break
		}
	}

	if !syncDetected {
		t.Fatalf("Did not detect [SYNC:SEGMENT_CREATED] from engine stdout")
	}

	t.Log("Segment created by writer; hard-killing writer engine process...")
	if err := cmd.Process.Kill(); err != nil {
		t.Fatalf("Failed to kill engine process: %v", err)
	}
	_ = cmd.Wait()
	t.Log("Engine process killed.")

	// Await client result: must return transport error without hanging
	select {
	case err := <-errChan:
		if err == nil {
			t.Fatalf("Expected client error due to writer crash, got nil")
		}
		st, ok := status.FromError(err)
		if !ok {
			t.Fatalf("Expected gRPC status error, got %v", err)
		}
		if st.Code() != codes.Unavailable && st.Code() != codes.Internal && st.Code() != codes.Canceled {
			t.Fatalf("Expected Unavailable/Internal/Canceled code, got %v", st.Code())
		}
		t.Logf("Client cleanly detected writer crash with code %v: %v", st.Code(), err)
	case <-time.After(5 * time.Second):
		t.Fatalf("Client hung indefinitely waiting for response from crashed writer")
	}
}

func TestMultiProcessSHM_UnauthenticatedRequestRedaction(t *testing.T) {
	enginePath, _ := findBinaries(t)
	jwtSecret := "test_secret_key_auth_redaction_12345"
	os.Setenv("QUBIT_ENGINE_JWT_SECRET", jwtSecret)

	_, cleanupEngine := startEngine(t, enginePath, jwtSecret)
	defer cleanupEngine()

	conn, err := grpc.NewClient("127.0.0.1:50051", grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumComputeClient(conn)

	// Send request with NO authorization metadata and use_shm=true, N=14
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	req := &pb.CircuitRequest{
		NumQubits:           14,
		MeasurementStrategy: pb.CircuitRequest_FULL_STATE,
		UseShm:              true,
		Operations: []*pb.GateOperation{
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 0},
		},
	}

	resp, err := client.RunCircuit(ctx, req)
	if err == nil {
		t.Fatalf("Security violation: unauthenticated request was NOT rejected, returned resp: %v", resp)
	}

	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.Unauthenticated {
		t.Fatalf("Expected codes.Unauthenticated, got status %v (err: %v)", st.Code(), err)
	}

	if resp != nil && resp.ShmDescriptor != "" {
		t.Fatalf("Security violation: unauthenticated request received SHM descriptor: %s", resp.ShmDescriptor)
	}

	t.Logf("Confirmed: unauthenticated request is strictly rejected with %v (%v), preventing SHM descriptor leakage.", st.Code(), st.Message())
}

func TestMultiProcessSHM_BackpressureFallbackUnderLoad(t *testing.T) {
	enginePath, readerToolPath := findBinaries(t)
	jwtSecret := "test_secret_key_backpressure_12345"
	os.Setenv("QUBIT_ENGINE_JWT_SECRET", jwtSecret)

	_, cleanupEngine := startEngine(t, enginePath, jwtSecret)
	defer cleanupEngine()

	token, err := auth.GenerateToken("test-client", 1*time.Hour)
	if err != nil {
		t.Fatalf("Failed to generate token: %v", err)
	}

	// Spawn 3 concurrent readers that pause after map to saturate the 3-segment backpressure cap
	readers := make([]*exec.Cmd, 3)
	for i := 0; i < 3; i++ {
		cmd := exec.Command(readerToolPath, "127.0.0.1:50051", token, "14")
		cmd.Env = append(os.Environ(), "QUBIT_SHM_TEST_HOOK=pause_after_map")
		stdoutPipe, err := cmd.StdoutPipe()
		if err != nil {
			t.Fatalf("Failed to get pipe for reader %d: %v", i, err)
		}
		cmd.Stderr = os.Stderr
		if err := cmd.Start(); err != nil {
			t.Fatalf("Failed to start reader %d: %v", i, err)
		}
		readers[i] = cmd

		// Wait until reader emits [SYNC:MAPPED_AWAITING_ACK]
		scanner := bufio.NewScanner(stdoutPipe)
		synced := false
		for scanner.Scan() {
			if strings.Contains(scanner.Text(), "[SYNC:MAPPED_AWAITING_ACK]") {
				synced = true
				break
			}
		}
		if !synced {
			t.Fatalf("Reader %d failed to sync", i)
		}
	}
	defer func() {
		for _, r := range readers {
			if r != nil && r.Process != nil {
				_ = r.Process.Kill()
				_ = r.Wait()
			}
		}
	}()

	t.Log("Saturated 3 unacknowledged SHM segments in ActiveShmRegistry.")

	// Now issue 4th request over gRPC directly: must fall back to protobuf
	conn, err := grpc.NewClient("127.0.0.1:50051", grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumComputeClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", "Bearer "+token)

	req := &pb.CircuitRequest{
		NumQubits:           14,
		MeasurementStrategy: pb.CircuitRequest_FULL_STATE,
		UseShm:              true,
		Operations: []*pb.GateOperation{
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 0},
		},
	}

	resp, err := client.RunCircuit(ctx, req)
	if err != nil {
		t.Fatalf("4th request failed: %v", err)
	}

	// 1. SHM descriptor must be empty because backpressure limit was reached
	if resp.ShmDescriptor != "" {
		t.Fatalf("Expected empty shm_descriptor due to backpressure, but got: %s", resp.ShmDescriptor)
	}

	// 2. Protobuf state vector must contain full 2^14 = 16384 elements
	if len(resp.StateVector) != 16384 {
		t.Fatalf("Expected 16384 elements in protobuf fallback, got %d", len(resp.StateVector))
	}

	// 3. Verify numerical correctness of fallback state vector data:
	// Circuit applies Hadamard on qubit 0 on |0...0>, resulting in (|0> + |1>)/sqrt(2)
	invSqrt2 := 1.0 / math.Sqrt(2.0)
	diff0 := math.Abs(resp.StateVector[0].Real - invSqrt2)
	diff1 := math.Abs(resp.StateVector[1].Real - invSqrt2)
	if diff0 > 1e-12 || diff1 > 1e-12 || math.Abs(resp.StateVector[0].Imag) > 1e-12 || math.Abs(resp.StateVector[1].Imag) > 1e-12 {
		t.Fatalf("Numerical mismatch in protobuf fallback: amp[0]=(%g, %g), amp[1]=(%g, %g), expected real=%g",
			resp.StateVector[0].Real, resp.StateVector[0].Imag,
			resp.StateVector[1].Real, resp.StateVector[1].Imag, invSqrt2)
	}

	// Assert remaining amplitudes are all zero
	for i := 2; i < 16384; i++ {
		if math.Abs(resp.StateVector[i].Real) > 1e-12 || math.Abs(resp.StateVector[i].Imag) > 1e-12 {
			t.Fatalf("Expected zero amplitude at index %d, got (%g, %g)",
				i, resp.StateVector[i].Real, resp.StateVector[i].Imag)
		}
	}

	t.Log("Confirmed: backpressure cap forced clean fallback to protobuf state vector with verified numerical correctness.")
}
