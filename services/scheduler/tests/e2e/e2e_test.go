package e2e

import (
	"context"
	"fmt"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	pb "github.com/perclft/QubitEngine/api/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// Mock Engine
type mockEngine struct {
	pb.UnimplementedQuantumComputeServer
	receivedRequests []*pb.CircuitRequest
}

func (m *mockEngine) RunCircuit(ctx context.Context, req *pb.CircuitRequest) (*pb.StateResponse, error) {
	m.receivedRequests = append(m.receivedRequests, req)
	return &pb.StateResponse{
		StateVector: []*pb.StateResponse_ComplexNumber{
			{Real: 0.707, Imag: 0},
			{Real: 0.707, Imag: 0},
		},
		ClassicalResults: map[uint32]bool{0: true},
		ServerId:         "mock-engine",
	}, nil
}

func TestSchedulerIntegration(t *testing.T) {
	// 1. Start MiniRedis
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("failed to start miniredis: %v", err)
	}
	defer mr.Close()

	// 2. Start Mock Engine
	engineLis, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("failed to listen for mock engine: %v", err)
	}
	defer engineLis.Close()

	s := grpc.NewServer()
	mock := &mockEngine{}
	pb.RegisterQuantumComputeServer(s, mock)
	go func() {
		if err := s.Serve(engineLis); err != nil {
			// ignore close error
		}
	}()
	engineAddr := engineLis.Addr().String()

	// 3. Compile Scheduler Binary
	tmpDir, err := os.MkdirTemp("", "scheduler-e2e")
	if err != nil {
		t.Fatal(err)
	}
	defer os.RemoveAll(tmpDir)

	binName := "scheduler"
	if runtime.GOOS == "windows" {
		binName += ".exe"
	}
	binPath := filepath.Join(tmpDir, binName)
	// Using absolute path to main.go. Assuming test runs from services/scheduler
	rootDir, _ := filepath.Abs("../../")
	cmdBuild := exec.Command("go", "build", "-o", binPath, ".")
	cmdBuild.Dir = rootDir
	if out, err := cmdBuild.CombinedOutput(); err != nil {
		t.Fatalf("failed to build scheduler: %v\n%s", err, out)
	}

	// 4. Run Scheduler
	schedulerPort := 50055
	cmdRun := exec.Command(binPath,
		"-port", fmt.Sprintf("%d", schedulerPort),
		"-redis-addr", mr.Addr(),
		"-engine-addr", engineAddr,
	)
	cmdRun.Stdout = os.Stdout
	cmdRun.Stderr = os.Stderr

	if err := cmdRun.Start(); err != nil {
		t.Fatalf("failed to start scheduler: %v", err)
	}
	defer func() {
		cmdRun.Process.Kill()
	}()

	// Wait for start
	time.Sleep(2 * time.Second)

	// 5. Connect to Scheduler
	conn, err := grpc.Dial(fmt.Sprintf("localhost:%d", schedulerPort), grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("failed to dial scheduler: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumSchedulerClient(conn)

	// 6. Submit Job
	circuit := &pb.CircuitRequest{
		NumQubits: 2,
		Operations: []*pb.GateOperation{
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 0},
		},
	}

	submitReq := &pb.JobRequest{
		Circuit:  circuit,
		Priority: pb.JobPriority_PRIORITY_NORMAL,
		Shots:    100,
		UserId:   "test-user",
	}

	jobHandle, err := client.SubmitJob(context.Background(), submitReq)
	if err != nil {
		t.Fatalf("SubmitJob failed: %v", err)
	}
	t.Logf("Job Submitted: %s", jobHandle.JobId)

	// 7. Poll for Completion
	for i := 0; i < 10; i++ {
		status, err := client.GetJobStatus(context.Background(), jobHandle)
		if err != nil {
			t.Fatalf("GetJobStatus failed: %v", err)
		}

		t.Logf("Job Status: %s", status.State)

		if status.State == pb.JobState_STATE_COMPLETED {
			break
		}
		if status.State == pb.JobState_STATE_FAILED {
			t.Fatalf("Job Failed: %s", status.ErrorMessage)
		}

		time.Sleep(500 * time.Millisecond)
	}

	// 8. Verify Engine was called
	if len(mock.receivedRequests) == 0 {
		t.Fatal("Engine was NOT called!")
	}

	if mock.receivedRequests[0].NumQubits != 2 {
		t.Errorf("Expected 2 qubits, got %d", mock.receivedRequests[0].NumQubits)
	}

	t.Log("✅ Integration Test Passed!")
}
