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
	// Return result 0
	return &pb.StateResponse{
		StateVector: []*pb.StateResponse_ComplexNumber{
			{Real: 1.0, Imag: 0},
		},
		ClassicalResults: map[uint32]bool{0: false, 1: false, 2: false},
		ServerId:         "mock-engine",
	}, nil
}

func TestMusicIntegration(t *testing.T) {
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

	// 2. Compile Music Binary
	tmpDir, err := os.MkdirTemp("", "music-e2e")
	if err != nil {
		t.Fatal(err)
	}
	defer os.RemoveAll(tmpDir)

	binPath := filepath.Join(tmpDir, "music")
	// Test runs from modules/music/tests/e2e
	rootDir, _ := filepath.Abs("../../")
	cmdBuild := exec.Command("go", "build", "-o", binPath, ".")
	cmdBuild.Dir = rootDir
	if out, err := cmdBuild.CombinedOutput(); err != nil {
		t.Fatalf("failed to build music: %v\n%s", err, out)
	}

	// 3. Run Music Server
	musicPort := 50066
	cmdRun := exec.Command(binPath,
		"-port", fmt.Sprintf("%d", musicPort),
		"-engine-addr", engineAddr,
	)
	cmdRun.Stdout = os.Stdout
	cmdRun.Stderr = os.Stderr

	if err := cmdRun.Start(); err != nil {
		t.Fatalf("failed to start music module: %v", err)
	}
	defer func() {
		cmdRun.Process.Kill()
	}()

	// Wait for start
	time.Sleep(2 * time.Second)

	// 4. Connect to Music Server
	conn, err := grpc.Dial(fmt.Sprintf("localhost:%d", musicPort), grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("failed to dial music: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumComposerClient(conn)

	// 5. Generate Melody (should trigger multiple Engine calls, one per note)
	req := &pb.MelodyRequest{
		Scale:    pb.Scale_SCALE_MAJOR,
		NumNotes: 4,
	}

	resp, err := client.GenerateMelody(context.Background(), req)
	if err != nil {
		t.Fatalf("GenerateMelody failed: %v", err)
	}

	t.Logf("Generated Melody: %d notes", len(resp.Notes))
	if len(resp.Notes) != 4 {
		t.Errorf("Expected 4 notes, got %d", len(resp.Notes))
	}

	// 6. Verify Engine was called
	// Expect at least 4 calls (one per note)
	if len(mock.receivedRequests) < 4 {
		t.Errorf("Expected at least 4 engine calls, got %d", len(mock.receivedRequests))
	}

	if mock.receivedRequests[0].NumQubits != 3 {
		t.Errorf("Expected 3 qubits per note call, got %d", mock.receivedRequests[0].NumQubits)
	}

	t.Log("✅ Music Integration Test Passed!")
}
