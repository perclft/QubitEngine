package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"strings"
	"time"

	pb "github.com/perclft/QubitEngine/cli/internal/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// The new Circuit DSL
type CircuitFile struct {
	Name   string `json:"name"`
	Qubits int32  `json:"qubits"`
	Ops    []struct {
		Gate         string  `json:"gate"`
		Target       uint32  `json:"target"`
		Control      uint32  `json:"control"`
		Control2     uint32  `json:"control2"` // For Toffoli
		Angle        float64 `json:"angle"`    // For Rotations
		ClassicalReg uint32  `json:"classical_reg"`
	} `json:"ops"`
}

func main() {
	serverAddr := flag.String("server", "localhost:50051", "Engine Address")
	fileArg := flag.String("file", "", "Path to circuit JSON file")
	streamMode := flag.Bool("stream", false, "Enable Real-Time Streaming Visualization")
	flag.Parse()

	if *fileArg == "" {
		fmt.Println("❌ Usage: qctl -file <circuit.json> [-server host:port] [-stream]")
		os.Exit(1)
	}

	// 1. Read & Parse Circuit
	data, err := os.ReadFile(*fileArg)
	if err != nil {
		log.Fatalf("Failed to read file: %v", err)
	}

	var circuit CircuitFile
	if err := json.Unmarshal(data, &circuit); err != nil {
		log.Fatalf("Invalid JSON format: %v", err)
	}

	// 2. Connect to Engine
	conn, err := grpc.NewClient(*serverAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Fatalf("Connection failed: %v", err)
	}
	defer conn.Close()
	c := pb.NewQuantumComputeClient(conn)

	// 3. Build Proto Operations
	fmt.Printf("⚡ Submitting Circuit: '%s' (%d Qubits)\n", circuit.Name, circuit.Qubits)

	var pbOps []*pb.GateOperation
	for _, op := range circuit.Ops {
		pbOp := &pb.GateOperation{
			TargetQubit:        op.Target,
			ControlQubit:       op.Control,
			SecondControlQubit: op.Control2,
			Angle:              op.Angle,
			ClassicalRegister:  op.ClassicalReg,
		}

		switch strings.ToUpper(op.Gate) {
		case "H":
			pbOp.Type = pb.GateOperation_HADAMARD
		case "X":
			pbOp.Type = pb.GateOperation_PAULI_X
		case "CNOT":
			pbOp.Type = pb.GateOperation_CNOT
		case "M":
			pbOp.Type = pb.GateOperation_MEASURE
		// Phase 3: New Gates
		case "TOFFOLI", "CCNOT":
			pbOp.Type = pb.GateOperation_TOFFOLI
		case "S":
			pbOp.Type = pb.GateOperation_PHASE_S
		case "T":
			pbOp.Type = pb.GateOperation_PHASE_T
		case "RY":
			pbOp.Type = pb.GateOperation_ROTATION_Y
		case "RZ":
			pbOp.Type = pb.GateOperation_ROTATION_Z
		default:
			log.Fatalf("Unknown Gate Type: %s", op.Gate)
		}
		pbOps = append(pbOps, pbOp)
	}

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	if *streamMode {
		runStreaming(ctx, c, pbOps)
	} else {
		runStandard(ctx, c, circuit.Qubits, pbOps)
	}
}

func runStandard(ctx context.Context, c pb.QuantumComputeClient, qubits int32, ops []*pb.GateOperation) {
	start := time.Now()
	res, err := c.RunCircuit(ctx, &pb.CircuitRequest{
		NumQubits:  qubits,
		Operations: ops,
	})
	if err != nil {
		log.Fatalf("💥 Engine Error: %v", err)
	}
	duration := time.Since(start)

	fmt.Printf("✅ Done in %s\n", duration)
	printResults(res)
}

func runStreaming(ctx context.Context, c pb.QuantumComputeClient, ops []*pb.GateOperation) {
	fmt.Println("🌊 Connecting to Live Kernel Stream...")
	stream, err := c.StreamGates(ctx)
	if err != nil {
		log.Fatalf("Stream init failed: %v", err)
	}

	// Background thread to read responses
	waitc := make(chan struct{})
	go func() {
		step := 1
		for {
			in, err := stream.Recv()
			if err == io.EOF {
				close(waitc)
				return
			}
			if err != nil {
				log.Fatalf("Stream read failed: %v", err)
			}

			// Clear screen or just print separator
			fmt.Printf("\n--- [Step %d] Wavefunction Update ---\n", step)
			printStateVector(in.StateVector)
			printMeasurements(in.ClassicalResults)
			step++
		}
	}()

	// Send Gates
	for _, op := range ops {
		// Artificial delay for visualization effect (optional, removed for speed)
		// time.Sleep(500 * time.Millisecond)
		if err := stream.Send(op); err != nil {
			log.Fatalf("Failed to send gate: %v", err)
		}
	}
	stream.CloseSend()
	<-waitc
	fmt.Println("\n✅ Stream Completed.")
}

func printResults(res *pb.StateResponse) {
	fmt.Println("\n--- 🔬 Measurement Register ---")
	printMeasurements(res.ClassicalResults)

	fmt.Println("\n--- 🌊 Final Wavefunction (Non-Zero) ---")
	printStateVector(res.StateVector)
}

func printMeasurements(results map[uint32]bool) {
	if len(results) == 0 {
		return
	}
	for q, val := range results {
		bit := "0"
		if val {
			bit = "1"
		}
		fmt.Printf(" [Q%d] -> |%s>\n", q, bit)
	}
}

func printStateVector(vec []*pb.StateResponse_ComplexNumber) {
	for i, amp := range vec {
		mag := amp.Real*amp.Real + amp.Imag*amp.Imag
		if mag > 0.0001 {
			sign := "+"
			if amp.Imag < 0 {
				sign = "-"
			}
			fmt.Printf(" |%d> : (%.3f %s %.3fi)\n", i, amp.Real, sign, makePositive(amp.Imag))
		}
	}
}

func makePositive(f float64) float64 {
	if f < 0 {
		return -f
	}
	return f
}
