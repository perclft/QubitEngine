package main

import (
	"context"
	"fmt"
	"math"
	"math/cmplx"
	"os"
	"strconv"
	"time"

	pb "github.com/perclft/QubitEngine/api/generated"
	"github.com/perclft/QubitEngine/pkg/ipc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/metadata"
)

func main() {
	if len(os.Args) < 4 {
		fmt.Fprintf(os.Stderr, "Usage: %s <endpoint> <token> <num_qubits>\n", os.Args[0])
		os.Exit(1)
	}

	endpoint := os.Args[1]
	token := os.Args[2]
	qubits, err := strconv.Atoi(os.Args[3])
	if err != nil {
		fmt.Fprintf(os.Stderr, "Invalid num_qubits: %v\n", err)
		os.Exit(1)
	}

	conn, err := grpc.NewClient(endpoint, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to connect: %v\n", err)
		os.Exit(1)
	}
	defer conn.Close()

	client := pb.NewQuantumComputeClient(conn)

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", "Bearer "+token)

	req := &pb.CircuitRequest{
		NumQubits:           int32(qubits),
		MeasurementStrategy: pb.CircuitRequest_FULL_STATE,
		UseShm:              true,
		Operations: []*pb.GateOperation{
			{
				Type:        pb.GateOperation_HADAMARD,
				TargetQubit: 0,
			},
		},
	}

	resp, err := client.RunCircuit(ctx, req)
	if err != nil {
		fmt.Fprintf(os.Stderr, "[RESULT:ERROR] RunCircuit failed: %v\n", err)
		os.Exit(1)
	}

	if resp.ShmDescriptor == "" {
		fmt.Fprintf(os.Stderr, "[RESULT:ERROR] shm_descriptor was empty (downgraded to protobuf)\n")
		os.Exit(2)
	}

	fmt.Printf("[SYNC:SHM_DESCRIPTOR_RECEIVED] %s\n", resp.ShmDescriptor)
	os.Stdout.Sync()

	stateVec, err := ipc.ReadAndAcknowledgeStateVector(ctx, resp.ShmDescriptor, client)
	if err != nil {
		fmt.Fprintf(os.Stderr, "[RESULT:ERROR] ReadAndAcknowledgeStateVector failed: %v\n", err)
		os.Exit(3)
	}

	expectedLen := 1 << qubits
	if len(stateVec) != expectedLen {
		fmt.Fprintf(os.Stderr, "[RESULT:ERROR] length mismatch: expected %d, got %d\n", expectedLen, len(stateVec))
		os.Exit(4)
	}

	invSqrt2 := 1.0 / math.Sqrt(2.0)
	diff0 := cmplx.Abs(stateVec[0] - complex(invSqrt2, 0))
	diff1 := cmplx.Abs(stateVec[1] - complex(invSqrt2, 0))
	if diff0 > 1e-12 || diff1 > 1e-12 {
		fmt.Fprintf(os.Stderr, "[RESULT:ERROR] amplitude mismatch: diff0=%g, diff1=%g\n", diff0, diff1)
		os.Exit(5)
	}

	fmt.Printf("[RESULT:SUCCESS] elements=%d\n", len(stateVec))
}
