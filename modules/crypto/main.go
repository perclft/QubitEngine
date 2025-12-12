// Quantum Cryptography Module
// BB84 QKD Protocol Implementation

package main

import (
	"crypto/sha256"
	"flag"
	"fmt"
	"log"
	"math/rand"
	"net"
	"sync"
	"time"

	"google.golang.org/grpc"
)

type Basis int

const (
	BasisRectilinear Basis = 0 // Z basis: |0⟩, |1⟩
	BasisDiagonal    Basis = 1 // X basis: |+⟩, |-⟩
)

type BB84Session struct {
	ID          string
	AliceBits   []int
	AliceBases  []Basis
	BobBases    []Basis
	BobMeasures []int
	SharedKey   []byte
	ErrorRate   float64
}

type CryptoServer struct {
	rng      *rand.Rand
	sessions map[string]*BB84Session
	mu       sync.RWMutex
}

func NewCryptoServer() *CryptoServer {
	return &CryptoServer{
		rng:      rand.New(rand.NewSource(time.Now().UnixNano())),
		sessions: make(map[string]*BB84Session),
	}
}

// StartBB84Alice - Alice prepares and sends qubits
func (s *CryptoServer) StartBB84Alice(sessionID string, numBits int) *BB84Session {
	bits := make([]int, numBits)
	bases := make([]Basis, numBits)

	for i := 0; i < numBits; i++ {
		bits[i] = s.rng.Intn(2)         // Random bit (0 or 1)
		bases[i] = Basis(s.rng.Intn(2)) // Random basis
	}

	session := &BB84Session{
		ID:         sessionID,
		AliceBits:  bits,
		AliceBases: bases,
	}

	s.mu.Lock()
	s.sessions[sessionID] = session
	s.mu.Unlock()

	log.Printf("🔐 BB84 Alice started: %s (%d qubits)", sessionID, numBits)
	return session
}

// StartBB84Bob - Bob receives and measures qubits
func (s *CryptoServer) StartBB84Bob(sessionID string) *BB84Session {
	s.mu.Lock()
	defer s.mu.Unlock()

	session, exists := s.sessions[sessionID]
	if !exists {
		return nil
	}

	numBits := len(session.AliceBits)
	bobBases := make([]Basis, numBits)
	bobMeasures := make([]int, numBits)

	for i := 0; i < numBits; i++ {
		bobBases[i] = Basis(s.rng.Intn(2)) // Random basis choice

		if bobBases[i] == session.AliceBases[i] {
			// Same basis: get correct result
			bobMeasures[i] = session.AliceBits[i]
		} else {
			// Different basis: random result
			bobMeasures[i] = s.rng.Intn(2)
		}
	}

	session.BobBases = bobBases
	session.BobMeasures = bobMeasures

	log.Printf("🔐 BB84 Bob measured: %s", sessionID)
	return session
}

// ReconcileBB84 - Sift key based on matching bases
func (s *CryptoServer) ReconcileBB84(sessionID string) *BB84Session {
	s.mu.Lock()
	defer s.mu.Unlock()

	session, exists := s.sessions[sessionID]
	if !exists {
		return nil
	}

	var siftedKey []byte
	errors := 0
	matched := 0

	for i := 0; i < len(session.AliceBases); i++ {
		if session.AliceBases[i] == session.BobBases[i] {
			matched++
			siftedKey = append(siftedKey, byte(session.BobMeasures[i]))
			if session.AliceBits[i] != session.BobMeasures[i] {
				errors++
			}
		}
	}

	// Hash to final key
	h := sha256.Sum256(siftedKey)
	session.SharedKey = h[:]
	session.ErrorRate = float64(errors) / float64(matched)

	log.Printf("🔐 BB84 Reconciled: %s (sifted=%d, error_rate=%.2f%%)",
		sessionID, matched, session.ErrorRate*100)

	return session
}

func main() {
	port := flag.Int("port", 50063, "gRPC port")
	flag.Parse()

	server := NewCryptoServer()

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()

	log.Printf("🔐 Quantum Crypto starting on port %d", *port)
	log.Printf("   Protocols: BB84 QKD, QRNG, OTP")

	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}

	_ = server
}
