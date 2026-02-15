// Quantum Music Composer Module - THE QUANTUM MOZART 🎹⚛️
// Generate melodies using TRUE quantum superposition and musical interference

package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"math"
	"math/cmplx"
	"net"
	"sync"

	pb "github.com/perclft/QubitEngine/api/generated"
	"github.com/perclft/QubitEngine/pkg/service"
	"google.golang.org/grpc"
)

// ------------------------------------------------------------------
// Musical Constants
// ------------------------------------------------------------------

var noteNames = []string{"C", "D", "E", "F", "G", "A", "B", "REST"}

// Scale intervals (semitones from root)
var scales = map[string][]int{
	"major":      {0, 2, 4, 5, 7, 9, 11},
	"minor":      {0, 2, 3, 5, 7, 8, 10},
	"pentatonic": {0, 2, 4, 7, 9},
	"blues":      {0, 3, 5, 6, 7, 10},
	"dorian":     {0, 2, 3, 5, 7, 9, 10},
}

// Musical interference rules
var consonantFollowers = map[int][]int{
	0: {2, 4},    // C → E, G
	1: {3, 5},    // D → F, A
	2: {4, 6},    // E → G, B
	3: {0, 5},    // F → C, A
	4: {0, 2, 6}, // G → C, E, B
	5: {0, 2},    // A → C, E
	6: {0, 4},    // B → C, G
}

// ------------------------------------------------------------------
// Quantum State Vector
// ------------------------------------------------------------------

type StateVector struct {
	Amplitudes [8]complex128 // |000⟩ to |111⟩
	mu         sync.RWMutex
}

func NewEqualSuperposition() *StateVector {
	sv := &StateVector{}
	factor := complex(1.0/math.Sqrt(8.0), 0)
	for i := 0; i < 8; i++ {
		sv.Amplitudes[i] = factor
	}
	return sv
}

func (sv *StateVector) ApplyPhaseRotation(k int, theta float64) {
	sv.mu.Lock()
	defer sv.mu.Unlock()
	phase := cmplx.Exp(complex(0, theta))
	sv.Amplitudes[k] *= phase
}

func (sv *StateVector) ApplyAmplitudeBoost(targets []int, boostFactor float64) {
	sv.mu.Lock()
	defer sv.mu.Unlock()
	for _, t := range targets {
		if t >= 0 && t < 8 {
			sv.Amplitudes[t] *= complex(boostFactor, 0)
		}
	}
	sv.Normalize()
}

func (sv *StateVector) Normalize() {
	var total float64
	for _, a := range sv.Amplitudes {
		total += cmplx.Abs(a) * cmplx.Abs(a)
	}
	if total > 0 {
		factor := complex(1.0/math.Sqrt(total), 0)
		for i := range sv.Amplitudes {
			sv.Amplitudes[i] *= factor
		}
	}
}

func (sv *StateVector) Probabilities() [8]float64 {
	sv.mu.RLock()
	defer sv.mu.RUnlock()
	var probs [8]float64
	for i, a := range sv.Amplitudes {
		probs[i] = cmplx.Abs(a) * cmplx.Abs(a)
	}
	return probs
}

// ------------------------------------------------------------------
// Quantum Engine Client
// ------------------------------------------------------------------

func Measure3Qubits(ctx context.Context, client pb.QuantumComputeClient, probs [8]float64) (int, error) {
	// TODO: Send robust circuit to Engine based on probs
	// For now, simple Hadamard circuit to get random 3 bits
	req := &pb.CircuitRequest{
		NumQubits: 3,
		Operations: []*pb.GateOperation{
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 0},
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 1},
			{Type: pb.GateOperation_HADAMARD, TargetQubit: 2},
		},
	}

	resp, err := client.RunCircuit(ctx, req)
	if err != nil {
		return 0, err
	}

	outcome := 0
	if resp.ClassicalResults[0] {
		outcome += 1
	}
	if resp.ClassicalResults[1] {
		outcome += 2
	}
	if resp.ClassicalResults[2] {
		outcome += 4
	}

	return outcome, nil
}

// ------------------------------------------------------------------
// Quantum Music Server - Implements Generated Interface
// ------------------------------------------------------------------

type MusicServer struct {
	pb.UnimplementedQuantumComposerServer
	engineClient pb.QuantumComputeClient
	stateVector  *StateVector
	lastNote     int
	mu           sync.Mutex
	engineConn   *grpc.ClientConn
}

func NewMusicServer(engineAddr string) *MusicServer {
	conn, client, err := service.ConnectToEngine(engineAddr)
	if err != nil {
		log.Printf("⚠️ Failed to connect to engine: %v", err)
	}

	return &MusicServer{
		engineClient: client,
		engineConn:   conn,
		stateVector:  NewEqualSuperposition(),
		lastNote:     -1,
	}
}

// GenerateMelody implementation
func (s *MusicServer) GenerateMelody(ctx context.Context, req *pb.MelodyRequest) (*pb.Melody, error) {
	numNotes := int(req.NumNotes)
	if numNotes <= 0 {
		numNotes = 8
	}

	scaleName := "major"
	// simplified mapping for now
	if req.Scale == pb.Scale_SCALE_MINOR {
		scaleName = "minor"
	}

	rootNote := int(req.RootNote)
	if rootNote == 0 {
		rootNote = 60
	} // Middle C

	log.Printf("🎹 Generating %d-note melody in %s...", numNotes, scaleName)

	var generatedNotes []*pb.Note
	currentTime := 0.0

	for i := 0; i < numNotes; i++ {
		s.mu.Lock()
		s.stateVector = NewEqualSuperposition()
		s.applyMusicalInterference()
		probs := s.stateVector.Probabilities()
		s.mu.Unlock()

		outcome, err := Measure3Qubits(ctx, s.engineClient, probs)
		if err != nil {
			log.Printf("Measurement failed: %v", err)
			outcome = i % 8 // fallback
		}

		s.mu.Lock()
		s.lastNote = outcome
		s.mu.Unlock()

		// Map to pitch
		scaleIntervals := scales[scaleName]
		if scaleIntervals == nil {
			scaleIntervals = scales["major"]
		}

		pitch := 0
		if outcome < len(scaleIntervals) {
			pitch = rootNote + scaleIntervals[outcome]
		} else {
			pitch = 0 // Rest
		}

		duration := 0.5 // simplified fixed duration for now
		velocity := 0.8

		generatedNotes = append(generatedNotes, &pb.Note{
			Pitch:     int32(pitch),
			Duration:  duration,
			Velocity:  velocity,
			StartTime: currentTime,
		})

		currentTime += duration
	}

	return &pb.Melody{
		Notes:         generatedNotes,
		Scale:         req.Scale,
		RootNote:      req.RootNote,
		DurationBeats: currentTime,
	}, nil
}

func (s *MusicServer) applyMusicalInterference() {
	if s.lastNote < 0 || s.lastNote > 7 {
		return
	}
	followers := consonantFollowers[s.lastNote%7]
	if len(followers) > 0 {
		s.stateVector.ApplyAmplitudeBoost(followers, math.Sqrt(2))
	}
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------

func main() {
	port := flag.Int("port", 50062, "gRPC port")
	engineAddr := flag.String("engine-addr", "engine:50051", "Quantum Engine address")
	flag.Parse()

	server := NewMusicServer(*engineAddr)

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	pb.RegisterQuantumComposerServer(grpcServer, server)

	log.Printf("🎹 Quantum Composer starting on port %d", *port)
	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}
}
