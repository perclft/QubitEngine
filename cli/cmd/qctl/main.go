package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"os"
	"time"

	pb "github.com/perclft/QubitEngine/cli/internal/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
)

func main() {
	serverAddr := flag.String("server", "localhost:50051", "host:port")
	flag.Parse()

	conn, err := grpc.NewClient(*serverAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	c := pb.NewQuantumComputeClient(conn)

	// Circuit: Bell State + Measurement
	// 1. H(0)
	// 2. CNOT(0, 1)
	// 3. MEASURE(0)
	// 4. MEASURE(1)
	fmt.Println("Sending circuit: Bell State + Measurement...")
	
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	req := &pb.CircuitRequest{
		NumQubits: 2,
		Operations: []*pb.GateOperation{
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 0},
			{Type: pb.GateOperation_CNOT, ControlQubit: 0, TargetQubit: 1},
			{Type: pb.GateOperation_MEASURE, TargetQubit: 0},
			{Type: pb.GateOperation_MEASURE, TargetQubit: 1},
		},
	}

	r, err := c.RunCircuit(ctx, req)
	if err != nil {
		st, _ := status.FromError(err)
		log.Fatalf("RPC Failed: %s", st.Message())
		os.Exit(1)
	}

	// Print Classical Results
	fmt.Println("\n--- Classical Measurement Results ---")
	for qubit, val := range r.ClassicalResults {
		bit := 0
		if val {
			bit = 1
		}
		fmt.Printf("Qubit %d collapsed to: |%d>\n", qubit, bit)
	}

	// Print Wavefunction (Should be collapsed to single state)
	fmt.Println("\n--- Final Wavefunction (Collapsed) ---")
	for i, amp := range r.StateVector {
		mag := amp.Real*amp.Real + amp.Imag*amp.Imag
		if mag > 0.0001 {
			fmt.Printf("State |%d>: (%.3f + %.3fi)\n", i, amp.Real, amp.Imag)
		}
	}
}
