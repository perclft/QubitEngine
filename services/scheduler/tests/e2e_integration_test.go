package tests

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/perclft/QubitEngine/api/auth"
	pb "github.com/perclft/QubitEngine/api/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// tokenAuth implements grpc.PerRPCCredentials
type tokenAuth struct {
	token string
}

func (t tokenAuth) GetRequestMetadata(ctx context.Context, uri ...string) (map[string]string, error) {
	return map[string]string{"authorization": "Bearer " + t.token}, nil
}

func (t tokenAuth) RequireTransportSecurity() bool { return false }

func TestFullStackIntegration(t *testing.T) {
	// Find root directory
	_, currentFile, _, _ := runtime.Caller(0)
	rootDir := filepath.Join(filepath.Dir(currentFile), "..", "..", "..")

	// 1. Check if C++ engine is built
	enginePath := filepath.Join(rootDir, "bin", "qubit_engine")
	if runtime.GOOS == "windows" {
		enginePath += ".exe"
	}
	if _, err := os.Stat(enginePath); os.IsNotExist(err) {
		t.Skipf("Skipping full-stack test: C++ engine not found at %s. Build it first.", enginePath)
	}

	// Setup Auth
	os.Setenv("QUBIT_ENGINE_JWT_SECRET", "e2e_test_secret_key_1234567890")
	testToken, _ := auth.GenerateToken("test-user", 3600*time.Second)

	// 2. Start MiniRedis
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("failed to start miniredis: %v", err)
	}
	defer mr.Close()

	// 3. Start C++ Engine
	enginePort := 50061
	cmdEngine := exec.Command(enginePath)
	cmdEngine.Env = append(os.Environ(),
		fmt.Sprintf("PORT=%d", enginePort),
		"REDIS_ADDR="+mr.Addr(),
		"QUBIT_ENGINE_JWT_SECRET=e2e_test_secret_key_1234567890",
		"QUBIT_ENGINE_SKIP_AUTH=0",
	)
	cmdEngine.Stdout = os.Stdout
	cmdEngine.Stderr = os.Stderr
	if err := cmdEngine.Start(); err != nil {
		t.Fatalf("failed to start engine: %v", err)
	}
	defer func() {
		cmdEngine.Process.Kill()
	}()

	// Wait for Engine to listen
	time.Sleep(2 * time.Second)

	// 4. Compile and Start Scheduler
	tmpDir, err := os.MkdirTemp("", "scheduler-full-e2e")
	if err != nil {
		t.Fatal(err)
	}
	defer os.RemoveAll(tmpDir)

	schedulerBin := filepath.Join(tmpDir, "scheduler")
	if runtime.GOOS == "windows" {
		schedulerBin += ".exe"
	}
	
	schedulerDir := filepath.Join(rootDir, "services", "scheduler")
	cmdBuild := exec.Command("go", "build", "-o", schedulerBin, "./cmd/scheduler")
	cmdBuild.Dir = schedulerDir
	if out, err := cmdBuild.CombinedOutput(); err != nil {
		t.Fatalf("failed to build scheduler: %v\n%s", err, out)
	}

	schedulerPort := 50062
	cmdSched := exec.Command(schedulerBin,
		"-port", fmt.Sprintf("%d", schedulerPort),
		"-redis-addr", mr.Addr(),
		"-engine-addr", fmt.Sprintf("127.0.0.1:%d", enginePort),
	)
	cmdSched.Stdout = os.Stdout
	cmdSched.Stderr = os.Stderr
	cmdSched.Env = append(os.Environ(),
		"QUBIT_ENGINE_JWT_SECRET=e2e_test_secret_key_1234567890",
		"QUBIT_ENGINE_SKIP_AUTH=0",
	)
	if err := cmdSched.Start(); err != nil {
		t.Fatalf("failed to start scheduler: %v", err)
	}
	defer func() {
		cmdSched.Process.Kill()
	}()

	time.Sleep(2 * time.Second)

	// 5. Connect and Test
	conn, err := grpc.Dial(fmt.Sprintf("127.0.0.1:%d", schedulerPort),
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithPerRPCCredentials(tokenAuth{token: testToken}),
	)
	if err != nil {
		t.Fatalf("failed to connect to scheduler: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumSchedulerClient(conn)

	// Submit Job
	circuit := &pb.CircuitRequest{
		NumQubits: 2,
		Operations: []*pb.GateOperation{
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 0},
			{Type: pb.GateOperation_CNOT, ControlQubit: 0, TargetQubit: 1},
		},
	}

	req := &pb.JobRequest{
		Circuit:  circuit,
		Priority: pb.JobPriority_PRIORITY_NORMAL,
		Shots:    100,
		UserId:   "test-user",
	}

	jobHandle, err := client.SubmitJob(context.Background(), req)
	if err != nil {
		t.Fatalf("SubmitJob failed: %v", err)
	}
	t.Logf("Job submitted: %s", jobHandle.JobId)

	// Poll until completion
	success := false
	for i := 0; i < 20; i++ {
		status, err := client.GetJobStatus(context.Background(), jobHandle)
		if err != nil {
			t.Fatalf("GetJobStatus failed: %v", err)
		}
		if status.State == pb.JobState_STATE_COMPLETED {
			success = true
			break
		}
		if status.State == pb.JobState_STATE_FAILED {
			t.Fatalf("Job failed: %s", status.ErrorMessage)
		}
		time.Sleep(500 * time.Millisecond)
	}

	if !success {
		t.Fatal("Job did not complete in time")
	}
	t.Log("✅ Full-Stack Integration Test Passed!")
}
