package main

import (
	"context"
	"crypto/sha256"
	"flag"
	"fmt"
	"log"
	"math/rand"
	"net"
	"sync"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	pb "github.com/perclft/QubitEngine/modules/crypto/generated/crypto"
	engine "github.com/perclft/QubitEngine/modules/crypto/generated/engine"
)

type Basis int

// ... (BB84Session struct remains, see below) ...
type BB84Session struct {
	ID          string
	AliceBits   []int32
	AliceBases  []pb.Basis
	BobBases    []pb.Basis
	BobMeasures []int32
	SharedKey   []byte
	ErrorRate   float64
	EveProb     float64 // Probability of eavesdropping per qubit
}

type CryptoServer struct {
	pb.UnimplementedQuantumCryptoServer
	rng          *rand.Rand
	sessions     map[string]*BB84Session
	mu           sync.RWMutex
	engineClient engine.QuantumComputeClient
}

func NewCryptoServer(engineClient engine.QuantumComputeClient) *CryptoServer {
	return &CryptoServer{
		rng:          rand.New(rand.NewSource(time.Now().UnixNano())),
		sessions:     make(map[string]*BB84Session),
		engineClient: engineClient,
	}
}

// StartBB84Alice prepares bits and bases, "sending" them conceptually (storing in session)
func (s *CryptoServer) StartBB84Alice(ctx context.Context, req *pb.BB84AliceRequest) (*pb.BB84AliceState, error) {
	numBits := int(req.NumBits)
	bits := make([]int32, numBits)
	bases := make([]pb.Basis, numBits)

	for i := 0; i < numBits; i++ {
		bits[i] = int32(s.rng.Intn(2))
		bases[i] = pb.Basis(s.rng.Intn(2))
	}

	session := &BB84Session{
		ID:         req.SessionId,
		AliceBits:  bits,
		AliceBases: bases,
		EveProb:    req.EavesdropProbability,
	}

	s.mu.Lock()
	s.sessions[req.SessionId] = session
	s.mu.Unlock()

	log.Printf("🔐 Alice started session %s: %d bits (Eve prob: %.2f)", req.SessionId, numBits, req.EavesdropProbability)
	return &pb.BB84AliceState{
		SessionId: req.SessionId,
		Bits:      bits,
		Bases:     bases,
	}, nil
}

// StartBB84Bob receives qubits. Here we simulate the quantum channel + Eve + Bob's measurement
func (s *CryptoServer) StartBB84Bob(ctx context.Context, req *pb.BB84BobRequest) (*pb.BB84BobState, error) {
	s.mu.RLock()
	session, ok := s.sessions[req.SessionId]
	s.mu.RUnlock()
	if !ok {
		return nil, fmt.Errorf("session not found")
	}

	numBits := len(session.AliceBits)
	bobBases := make([]pb.Basis, numBits)
	// We will build a circuit to simulate the whole process for each qubit/batch
	// Or simpler: One big circuit?
	// QubitEngine handles ~30 qubits. If numBits > 30, we must batch.
	// Let's assume typical demo is 10-20 bits. Or we batch 30 at a time.

	// Generate Bob's bases first
	for i := 0; i < numBits; i++ {
		bobBases[i] = pb.Basis(s.rng.Intn(2))
	}

	results := make([]int32, numBits)

	// Process in batches of 20 to be safe
	batchSize := 20
	for i := 0; i < numBits; i += batchSize {
		end := i + batchSize
		if end > numBits {
			end = numBits
		}
		currentBatch := end - i

		ops := make([]*engine.GateOperation, 0)

		// 1. Alice Prepares
		for j := 0; j < currentBatch; j++ {
			idx := i + j
			qubit := uint32(j)

			// X if bit is 1
			if session.AliceBits[idx] == 1 {
				ops = append(ops, &engine.GateOperation{
					Type:        engine.GateOperation_PAULI_X,
					TargetQubit: qubit,
				})
			}
			// H if basis is Diagonal (1)
			if session.AliceBases[idx] == pb.Basis_BASIS_DIAGONAL {
				ops = append(ops, &engine.GateOperation{
					Type:        engine.GateOperation_HADAMARD,
					TargetQubit: qubit,
				})
			}

			// 2. Eve Intercepts (Simulated per qubit)
			if session.EveProb > 0 && s.rng.Float64() < session.EveProb {
				// Eve picks random basis
				eveBasis := pb.Basis(s.rng.Intn(2))
				if eveBasis == pb.Basis_BASIS_DIAGONAL {
					ops = append(ops, &engine.GateOperation{
						Type:        engine.GateOperation_HADAMARD,
						TargetQubit: qubit,
					})
				}
				// Eve Measures (Collapse)
				ops = append(ops, &engine.GateOperation{
					Type:              engine.GateOperation_MEASURE,
					TargetQubit:       qubit,
					ClassicalRegister: uint32(j + 100), // Dump to unused register
				})
				// If Eve measured in X basis (Diagonal), she put it in |+> or |-> which is fine.
				// If she used Z basis, she put it in |0> or |1>.
				// The key is that the state Collapsed.
				// We must "Undo" Eve's basis rotation if we want to forward the 'photon'?
				// BB84: Eve measures and resends.
				// If Eve measures with Z, she sends the Z result.
				// If Eve measures with X, she sends the X result.
				// Our simulation: The qubit REMAINS in the state Eve left it in.
				// If Eve measured in X, it is |+> or |->.
				// If she applied H then Measure, it is |0> or |1>.
				// Wait, if she applied H then Measure, the qubit is |0> or |1>.
				// But she needs to resend in the basis she measured.
				// If result was 0 (|0>), and she measured in Diagonal, she effectively found |+>.
				// So she should send |+>.
				// To send |+>, she applies H to |0>.
				// So: If Eve basis was Diagonal, and she measured, she needs to apply H AGAIN to "resend" in Diagonal basis.
				// Logic:
				//   Init -> [H (if Diag)] -> Measure
				//   Resend: If Diag, apply H?
				//   Yes. H|0> = |+>. H|1> = |->.
				if eveBasis == pb.Basis_BASIS_DIAGONAL {
					ops = append(ops, &engine.GateOperation{
						Type:        engine.GateOperation_HADAMARD,
						TargetQubit: qubit,
					})
				}
			}

			// 3. Bob Measures
			// Apply H if Bob basis is Diagonal
			if bobBases[idx] == pb.Basis_BASIS_DIAGONAL {
				ops = append(ops, &engine.GateOperation{
					Type:        engine.GateOperation_HADAMARD,
					TargetQubit: qubit,
				})
			}
			// Measure
			ops = append(ops, &engine.GateOperation{
				Type:              engine.GateOperation_MEASURE,
				TargetQubit:       qubit,
				ClassicalRegister: uint32(j), // Store in register j
			})
		}

		// Run batch
		resp, err := s.engineClient.RunCircuit(ctx, &engine.CircuitRequest{
			NumQubits:  int32(currentBatch),
			Operations: ops,
		})
		if err != nil {
			return nil, fmt.Errorf("engine error: %v", err)
		}

		// Collect results
		for j := 0; j < currentBatch; j++ {
			// In proto map, key is uint32
			val := resp.ClassicalResults[uint32(j)]
			if val {
				results[i+j] = 1
			} else {
				results[i+j] = 0
			}
		}
	}

	s.mu.Lock()
	session.BobBases = bobBases
	session.BobMeasures = results
	s.mu.Unlock()

	log.Printf("🔐 Bob measured session %s", req.SessionId)
	return &pb.BB84BobState{
		SessionId:    req.SessionId,
		Bases:        bobBases,
		Measurements: results,
	}, nil
}

func (s *CryptoServer) ReconcileBB84(ctx context.Context, req *pb.ReconcileRequest) (*pb.BB84Key, error) {
	s.mu.RLock()
	session, ok := s.sessions[req.SessionId]
	s.mu.RUnlock()
	if !ok {
		return nil, fmt.Errorf("session not found")
	}

	var siftedKey []byte
	errors := 0
	matched := 0

	// Compare stored alice/bob data
	for i := 0; i < len(session.AliceBases); i++ {
		if session.AliceBases[i] == session.BobBases[i] {
			matched++
			siftedKey = append(siftedKey, byte(session.BobMeasures[i]))
			if session.AliceBits[i] != session.BobMeasures[i] {
				errors++
			}
		}
	}

	var errorRate float64 = 0.0
	if matched > 0 {
		errorRate = float64(errors) / float64(matched)
	}

	h := sha256.Sum256(siftedKey)
	secure := errorRate < 0.1 // Threshold (10%)

	log.Printf("🔐 Reconciled session %s: ErrRate=%.2f%%, Secure=%v", req.SessionId, errorRate*100, secure)

	return &pb.BB84Key{
		SessionId:    req.SessionId,
		SharedKey:    h[:],
		OriginalBits: int32(len(session.AliceBits)),
		SiftedBits:   int32(matched),
		ErrorRate:    errorRate,
		Secure:       secure,
	}, nil
}

// Stubs for others
func (s *CryptoServer) GenerateQuantumKey(ctx context.Context, req *pb.KeyRequest) (*pb.QuantumKey, error) {
	return nil, nil
}
func (s *CryptoServer) QuantumEncrypt(ctx context.Context, req *pb.EncryptRequest) (*pb.EncryptedMessage, error) {
	return nil, nil
}
func (s *CryptoServer) QuantumDecrypt(ctx context.Context, req *pb.DecryptRequest) (*pb.DecryptedMessage, error) {
	return nil, nil
}
func (s *CryptoServer) DetectEavesdropping(ctx context.Context, req *pb.EavesdropRequest) (*pb.EavesdropResult, error) {
	return nil, nil
}

func main() {
	port := flag.Int("port", 50063, "gRPC port")
	engineAddr := flag.String("engine-addr", "engine:50051", "Quantum Engine address")
	flag.Parse()

	conn, err := grpc.Dial(*engineAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Fatalf("Failed to connect to engine: %v", err)
	}
	defer conn.Close()

	engineClient := engine.NewQuantumComputeClient(conn)
	server := NewCryptoServer(engineClient)

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	pb.RegisterQuantumCryptoServer(grpcServer, server)

	log.Printf("🔐 Quantum Crypto starting on port %d", *port)
	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}
}
