package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"time"

	pb "github.com/perclft/QubitEngine/cli/internal/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

func main() {
	serverAddr := flag.String("server", "localhost:50051", "The server address in the format of host:port")
	flag.Parse()

	// 1. Connect to gRPC Server
	// We use insecure credentials because we are running inside a private cluster/network
	conn, err := grpc.NewClient(*serverAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	c := pb.NewQuantumComputeClient(conn)

	// 2. Define the Circuit (Bell State: H(0) -> CNOT(0,1))
	fmt.Println("Sending circuit: 2 Qubits, H(0), CNOT(0->1)...")
	
	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()

	req := &pb.CircuitRequest{
		NumQubits: 2,
		Operations: []*pb.GateOperation{
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 0},
			{Type: pb.GateOperation_CNOT, ControlQubit: 0, TargetQubit: 1},
		},
	}

	// 3. Send Request
	r, err := c.RunCircuit(ctx, req)
	if err != nil {
		log.Fatalf("could not run circuit: %v", err)
	}

	// 4. Print Results
	fmt.Println("Result State Vector:")
	for i, amp := range r.StateVector {
		// Only print non-zero amplitudes for cleanliness
		mag := amp.Real*amp.Real + amp.Imag*amp.Imag
		if mag > 0.0001 {
			fmt.Printf("|%d>: (%.3f + %.3fi)\n", i, amp.Real, amp.Imag)
		}
	}
}
