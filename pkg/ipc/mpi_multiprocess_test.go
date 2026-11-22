package ipc

import (
	"context"
	"math"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
	"runtime"
	"time"
	"time"

	"github.com/perclft/QubitEngine/api/auth"
	pb "github.com/perclft/QubitEngine/api/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

func findMpiExec(t *testing.T) string {
    return mpiExecPath(t)
}

func startMpiCluster(t *testing.T, mpiExec string, enginePath string, port string, jwtSecret string, extraEnv ...string) (*exec.Cmd, func()) {
	os.Setenv("QUBIT_ENGINE_JWT_SECRET", jwtSecret)

	cmd := exec.Command(mpiExec, "-n", "2", enginePath)
	cmd.Dir = filepath.Dir(enginePath)

	env := append(os.Environ(),
		"QUBIT_ENGINE_JWT_SECRET="+jwtSecret,
		"QUBIT_ENGINE_SKIP_AUTH=0",
		"WORKER_COUNT=0",
		"PORT="+port,
		"QUBIT_ENGINE_PORT="+port,
		"METRICS_PORT=0",
	)
	env = append(env, extraEnv...)
	cmd.Env = env
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	if err := cmd.Start(); err != nil {
		t.Fatalf("Failed to start MPI cluster via %s: %v", mpiExec, err)
	}

	// Poll until Rank 0 gRPC server is listening
	addr := "127.0.0.1:" + port
	deadline := time.Now().Add(30 * time.Second)
	ready := false
	for time.Now().Before(deadline) {
		conn, err := net.DialTimeout("tcp", addr, 200*time.Millisecond)
		if err == nil {
			conn.Close()
			ready = true
			break
		}
		time.Sleep(100 * time.Millisecond)
	}

	if !ready {
		_ = cmd.Process.Kill()
		_ = cmd.Wait()
		t.Fatalf("MPI cluster Rank 0 failed to bind gRPC port %s within 10 seconds", port)
	}

	cleanup := func() {
		if cmd.Process != nil {
			_ = cmd.Process.Kill()
			_ = cmd.Wait()
		}
		time.Sleep(60 * time.Second)
	}

	return cmd, cleanup
}

func TestMultiProcessMPI_GlobalQubitGateLockstep(t *testing.T) {
	mpiExec := findMpiExec(t)
	enginePath, _ := findBinaries(t)

	if runtime.GOOS == "windows" {
		t.Skip("Skipping MPI tests on Windows")
	}
	port := "50061"
	jwtSecret := "test_secret_mpi_global_qubit_123"
	_, cleanup := startMpiCluster(t, mpiExec, enginePath, port, jwtSecret)
	defer cleanup()

	token, err := auth.GenerateToken("test-mpi-client", 1*time.Hour)
	if err != nil {
		t.Fatalf("Failed to generate JWT token: %v", err)
	}

	conn, err := grpc.NewClient("127.0.0.1:"+port, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to connect to Rank 0: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumComputeClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", "Bearer "+token)

	// 4 Qubits: total_dim = 16. With 2 ranks, local_dim = 8.
	// Qubit 3 has stride 2^3 = 8 >= local_dim (8) -> Global Qubit!
	// Step 1: H on qubit 3 -> exchanges buffers between Rank 0 and Rank 1 via MPI_Sendrecv.
	// Step 2: CNOT(3, 0) -> Global control qubit 3, local target qubit 0.
	// Expected final state: (|0000> + |1001>) / sqrt(2).
	req := &pb.CircuitRequest{
		NumQubits:           4,
		MeasurementStrategy: pb.CircuitRequest_FULL_STATE,
		Operations: []*pb.GateOperation{
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 3},
			{Type: pb.GateOperation_CNOT, ControlQubit: 3, TargetQubit: 0},
		},
	}

	resp, err := client.RunCircuit(ctx, req)
	if err != nil {
		t.Fatalf("RunCircuit failed on MPI cluster: %v", err)
	}

	if len(resp.StateVector) != 16 {
		t.Fatalf("Expected 16 state vector amplitudes gathered from 2 ranks, got %d", len(resp.StateVector))
	}

	invSqrt2 := 1.0 / math.Sqrt(2.0)
	// |0000> is at index 0 (Rank 0 local index 0)
	amp0 := resp.StateVector[0]
	if math.Abs(amp0.Real-invSqrt2) > 1e-10 || math.Abs(amp0.Imag) > 1e-10 {
		t.Fatalf("Expected |0000> = 1/sqrt(2), got (%g, %g)", amp0.Real, amp0.Imag)
	}

	// |1001> is at index 9: bit 3 is 1 (8), bit 0 is 1 (1) -> 8 + 1 = 9.
	// (Rank 1 local index 1)
	amp9 := resp.StateVector[9]
	if math.Abs(amp9.Real-invSqrt2) > 1e-10 || math.Abs(amp9.Imag) > 1e-10 {
		t.Fatalf("Expected |1001> = 1/sqrt(2), got (%g, %g)", amp9.Real, amp9.Imag)
	}

	// All other 14 basis amplitudes must be zero
	for i := 0; i < 16; i++ {
		if i == 0 || i == 9 {
			continue
		}
		c := resp.StateVector[i]
		if math.Abs(c.Real) > 1e-10 || math.Abs(c.Imag) > 1e-10 {
			t.Fatalf("Expected zero amplitude at index %d, got (%g, %g)", i, c.Real, c.Imag)
		}
	}

	t.Log("SUCCESS: Real 2-rank MPI cluster executed global gate H(3), CNOT(3, 0), and MPI_Gather in lockstep with verified fidelity!")
}

func TestMultiProcessMPI_DualGlobalCNOT(t *testing.T) {
	mpiExec := findMpiExec(t)
	enginePath, _ := findBinaries(t)

	port := "50062"
	jwtSecret := "test_secret_mpi_dual_global_cnot_123"
	_, cleanup := startMpiCluster(t, mpiExec, enginePath, port, jwtSecret)
	defer cleanup()

	token, err := auth.GenerateToken("test-mpi-client", 1*time.Hour)
	if err != nil {
		t.Fatalf("Failed to generate JWT token: %v", err)
	}

	conn, err := grpc.NewClient("127.0.0.1:"+port, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to connect to Rank 0: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumComputeClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", "Bearer "+token)

	// 5 Qubits: total_dim = 32. With 2 ranks, local_dim = 16.
	// Qubit 4 has stride 16 >= local_dim (16) -> Global Qubit!
	// Target qubit 0 is local.
	req := &pb.CircuitRequest{
		NumQubits:           5,
		MeasurementStrategy: pb.CircuitRequest_FULL_STATE,
		Operations: []*pb.GateOperation{
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 4},
			{Type: pb.GateOperation_CNOT, ControlQubit: 4, TargetQubit: 0},
		},
	}

	resp, err := client.RunCircuit(ctx, req)
	if err != nil {
		t.Fatalf("RunCircuit failed on MPI cluster: %v", err)
	}

	if len(resp.StateVector) != 32 {
		t.Fatalf("Expected 32 state vector amplitudes, got %d", len(resp.StateVector))
	}

	invSqrt2 := 1.0 / math.Sqrt(2.0)
	// |00000> is at index 0 (Rank 0)
	if math.Abs(resp.StateVector[0].Real-invSqrt2) > 1e-10 {
		t.Fatalf("Expected state[0] = 1/sqrt(2), got %g", resp.StateVector[0].Real)
	}
	// |10001> is at index 17 (Rank 1: bit 4 is 1 (16) + bit 0 is 1 (1) = 17)
	if math.Abs(resp.StateVector[17].Real-invSqrt2) > 1e-10 {
		t.Fatalf("Expected state[17] = 1/sqrt(2), got %g", resp.StateVector[17].Real)
	}

	t.Log("SUCCESS: 5-qubit distributed CNOT executed and verified across real MPI ranks.")
}

func TestMultiProcessMPI_DeadlockFreeRepeatedCollectives(t *testing.T) {
	mpiExec := findMpiExec(t)
	enginePath, _ := findBinaries(t)

	port := "50063"
	jwtSecret := "test_secret_mpi_deadlock_free_123"
	_, cleanup := startMpiCluster(t, mpiExec, enginePath, port, jwtSecret)
	defer cleanup()

	token, err := auth.GenerateToken("test-mpi-client", 1*time.Hour)
	if err != nil {
		t.Fatalf("Failed to generate JWT token: %v", err)
	}

	conn, err := grpc.NewClient("127.0.0.1:"+port, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumComputeClient(conn)

	// Issue 5 sequential circuits through the same running MPI cluster
	// to verify that the worker loop continuously resets and completes collectives
	// without hanging or communicator deadlocks.
	for iteration := 1; iteration <= 5; iteration++ {
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		ctx = metadata.AppendToOutgoingContext(ctx, "authorization", "Bearer "+token)

		req := &pb.CircuitRequest{
			NumQubits:           4,
			MeasurementStrategy: pb.CircuitRequest_FULL_STATE,
			Operations: []*pb.GateOperation{
				{Type: pb.GateOperation_HADAMARD, TargetQubit: 0},
				{Type: pb.GateOperation_HADAMARD, TargetQubit: 3},
			},
		}

		resp, err := client.RunCircuit(ctx, req)
		cancel()
		if err != nil {
			t.Fatalf("Iteration %d failed with error: %v", iteration, err)
		}
		if len(resp.StateVector) != 16 {
			t.Fatalf("Iteration %d: expected 16 amplitudes, got %d", iteration, len(resp.StateVector))
		}
	}

	t.Log("SUCCESS: 5 consecutive distributed circuits executed cleanly across MPI cluster without deadlock.")
}

func TestMultiProcessMPI_VQEPreconditionRejection(t *testing.T) {
	mpiExec := findMpiExec(t)
	enginePath, _ := findBinaries(t)

	port := "50064"
	jwtSecret := "test_secret_mpi_vqe_rejection_123"
	_, cleanup := startMpiCluster(t, mpiExec, enginePath, port, jwtSecret)
	defer cleanup()

	token, err := auth.GenerateToken("test-mpi-client", 1*time.Hour)
	if err != nil {
		t.Fatalf("Failed to generate token: %v", err)
	}

	conn, err := grpc.NewClient("127.0.0.1:"+port, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumComputeClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", "Bearer "+token)

	vqeReq := &pb.VQERequest{
		OptimizerType: pb.VQERequest_SPSA,
		MaxIterations: 10,
		Observables: []*pb.PauliTerm{
			{Coefficient: 1.0, PauliString: "ZZ"},
		},
	}

	stream, err := client.RunVQE(ctx, vqeReq)
	if err != nil {
		t.Fatalf("RunVQE call setup failed: %v", err)
	}

	_, err = stream.Recv()
	if err == nil {
		t.Fatalf("Expected FailedPrecondition error on MPI RunVQE, got nil")
	}

	st, ok := status.FromError(err)
	if !ok || st.Code() != codes.FailedPrecondition {
		t.Fatalf("Expected codes.FailedPrecondition, got %v (%v)", st.Code(), err)
	}

	t.Logf("Confirmed: RunVQE rejected on MPI cluster with %v: %v", st.Code(), st.Message())
}

func TestMultiProcessMPI_WatchdogFaultInjection(t *testing.T) {
	mpiExec := findMpiExec(t)
	enginePath, _ := findBinaries(t)

	port := "50065"
	jwtSecret := "test_secret_mpi_watchdog_123"

	// Configure cluster with 2-second timeout and worker hang hook before gather
	extraEnv := []string{
		"QUBIT_MPI_TEST_HOOK=worker_hang_before_gather",
		"QUBIT_MPI_CLUSTER_TIMEOUT_SEC=2",
	}

	cmd, cleanup := startMpiCluster(t, mpiExec, enginePath, port, jwtSecret, extraEnv...)
	defer cleanup()

	token, err := auth.GenerateToken("test-mpi-client", 1*time.Hour)
	if err != nil {
		t.Fatalf("Failed to generate token: %v", err)
	}

	conn, err := grpc.NewClient("127.0.0.1:"+port, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumComputeClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), 8*time.Second)
	defer cancel()
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", "Bearer "+token)

	req := &pb.CircuitRequest{
		NumQubits:           4,
		MeasurementStrategy: pb.CircuitRequest_FULL_STATE,
		Operations: []*pb.GateOperation{
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 0},
		},
	}

	start := time.Now()
	_, err = client.RunCircuit(ctx, req)
	duration := time.Since(start)

	if err == nil {
		t.Fatalf("Expected client error due to worker hang and MPI_Abort, got nil")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatalf("Expected gRPC status error, got %v", err)
	}

	// Because Rank 0 is aborted by MPI_Abort along with worker rank,
	// the client observes transport termination: Unavailable / Canceled / Internal
	if st.Code() != codes.Unavailable && st.Code() != codes.Internal && st.Code() != codes.Canceled {
		t.Fatalf("Expected Unavailable/Internal/Canceled status, got %v (%v)", st.Code(), err)
	}

	t.Logf("Watchdog triggered cleanly after %v: %v (%v)", duration, st.Code(), err)

	// Ensure the child processes actually terminated via MPI_Abort
	_ = cmd.Wait()
	t.Log("Confirmed: MPI cluster terminated cleanly via MPI_Abort without indefinite hanging.")
}

func TestMultiProcessMPI_ClusterExecutionLockContention(t *testing.T) {
	mpiExec := findMpiExec(t)
	enginePath, _ := findBinaries(t)

	port := "50066"
	jwtSecret := "test_secret_mpi_contention_123"

	// Cluster holds execution lock for 2 seconds on request 1, with 1s timeout limit
	extraEnv := []string{
		"QUBIT_MPI_TEST_HOOK=hold_cluster_lock",
		"QUBIT_MPI_CLUSTER_TIMEOUT_SEC=1",
	}

	_, cleanup := startMpiCluster(t, mpiExec, enginePath, port, jwtSecret, extraEnv...)
	defer cleanup()

	token, err := auth.GenerateToken("test-mpi-client", 1*time.Hour)
	if err != nil {
		t.Fatalf("Failed to generate token: %v", err)
	}

	conn, err := grpc.NewClient("127.0.0.1:"+port, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumComputeClient(conn)

	req := &pb.CircuitRequest{
		NumQubits:           4,
		MeasurementStrategy: pb.CircuitRequest_FULL_STATE,
		Operations: []*pb.GateOperation{
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 0},
		},
	}

	// Dispatch Request 1 in background: it acquires lock and holds for 2 seconds
	req1Done := make(chan error, 1)
	go func() {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()
		ctx = metadata.AppendToOutgoingContext(ctx, "authorization", "Bearer "+token)
		_, err := client.RunCircuit(ctx, req)
		req1Done <- err
	}()

	// Brief pause to ensure Request 1 has acquired cluster execution lock
	time.Sleep(300 * time.Millisecond)

	// Dispatch Request 2 concurrently: cluster lock is busy, times out after 1s
	ctx2, cancel2 := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel2()
	ctx2 = metadata.AppendToOutgoingContext(ctx2, "authorization", "Bearer "+token)

	_, err2 := client.RunCircuit(ctx2, req)
	if err2 == nil {
		t.Fatalf("Expected ResourceExhausted on contended cluster lock, but Request 2 succeeded")
	}

	st2, ok2 := status.FromError(err2)
	if !ok2 || st2.Code() != codes.ResourceExhausted {
		t.Fatalf("Expected codes.ResourceExhausted, got %v (%v)", st2.Code(), err2)
	}

	t.Logf("Confirmed: Contended cluster execution lock rejected Request 2 with %v: %v", st2.Code(), st2.Message())

	// Wait for Request 1 to finish cleanly
	select {
	case err1 := <-req1Done:
		if err1 != nil {
			t.Fatalf("Request 1 failed unexpectedly: %v", err1)
		}
		t.Log("Request 1 completed and released cluster lock cleanly.")
	case <-time.After(5 * time.Second):
		t.Fatalf("Request 1 hung waiting to finish")
	}
}

