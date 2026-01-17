package e2e

import (
	"context"
	"fmt"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
	"time"

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
	// Return result 0 (all false) -> Outcome 0 (Mysterious 1)
	return &pb.StateResponse{
		StateVector: []*pb.StateResponse_ComplexNumber{
			{Real: 1.0, Imag: 0},
		},
		ClassicalResults: map[uint32]bool{0: false, 1: false, 2: false},
		ServerId:         "mock-engine",
	}, nil
}

func TestGamingIntegration(t *testing.T) {
	// 1. Start Mock Engine
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

	// 2. Compile Gaming Binary
	tmpDir, err := os.MkdirTemp("", "gaming-e2e")
	if err != nil {
		t.Fatal(err)
	}
	defer os.RemoveAll(tmpDir)

	binPath := filepath.Join(tmpDir, "gaming")
	// Test runs from modules/gaming/tests/e2e
	rootDir, _ := filepath.Abs("../../")
	cmdBuild := exec.Command("go", "build", "-o", binPath, ".")
	cmdBuild.Dir = rootDir
	if out, err := cmdBuild.CombinedOutput(); err != nil {
		t.Fatalf("failed to build gaming: %v\n%s", err, out)
	}

	// 3. Run Gaming Server
	gamingPort := 50065
	cmdRun := exec.Command(binPath,
		"-port", fmt.Sprintf("%d", gamingPort),
		"-engine-addr", engineAddr,
	)
	cmdRun.Stdout = os.Stdout
	cmdRun.Stderr = os.Stderr

	if err := cmdRun.Start(); err != nil {
		t.Fatalf("failed to start gaming module: %v", err)
	}
	defer func() {
		cmdRun.Process.Kill()
	}()

	// Wait for start
	time.Sleep(2 * time.Second)

	// 4. Connect to Gaming Server
	conn, err := grpc.Dial(fmt.Sprintf("localhost:%d", gamingPort), grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("failed to dial gaming: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumGamingClient(conn)

	// 5. Ask Oracle (should trigger Engine call)
	req := &pb.OracleRequest{
		Question: "Will tests pass?",
		UserId:   "tester",
		Mood:     pb.OracleMood_MOOD_MYSTERIOUS,
	}

	resp, err := client.AskOracle(context.Background(), req)
	if err != nil {
		t.Fatalf("AskOracle failed: %v", err)
	}

	t.Logf("Oracle Answer: %s (Outcome: %d)", resp.Prophecy, resp.OutcomeIndex)

	// 6. Verify Engine was called
	if len(mock.receivedRequests) == 0 {
		t.Fatal("Engine was NOT called! Gaming module might be falling back to pseudo-random.")
	}

	if mock.receivedRequests[0].NumQubits != 3 {
		t.Errorf("Expected 3 qubits for Oracle, got %d", mock.receivedRequests[0].NumQubits)
	}

	t.Log("✅ Gaming Integration Test Passed!")
}
