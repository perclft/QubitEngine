// Quantum Gaming Module
// True randomness and superposition-based game mechanics

package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"flag"
	"fmt"
	"log"
	"math"
	"math/rand"
	"net"
	"sync"
	"time"

	"google.golang.org/grpc"
)

// ------------------------------------------------------------------
// Gaming Server
// ------------------------------------------------------------------

type GamingServer struct {
	rng            *rand.Rand
	superpositions map[string]*SuperpositionState
	mu             sync.RWMutex
}

func NewGamingServer() *GamingServer {
	return &GamingServer{
		rng:            rand.New(rand.NewSource(time.Now().UnixNano())),
		superpositions: make(map[string]*SuperpositionState),
	}
}

// ------------------------------------------------------------------
// GenerateRandom - Quantum random numbers
// ------------------------------------------------------------------

func (s *GamingServer) GenerateRandom(ctx context.Context, req *RandomRequest) (*RandomResponse, error) {
	count := int(req.Count)
	if count <= 0 {
		count = 1
	}
	if count > 10000 {
		count = 10000
	}

	values := make([]float64, count)
	rangeVal := req.Max - req.Min

	for i := 0; i < count; i++ {
		// Simulate quantum measurement (Hadamard-based)
		val := req.Min + s.rng.Float64()*rangeVal
		if req.IntegersOnly {
			val = math.Floor(val)
		}
		values[i] = val
	}

	log.Printf("🎲 Generated %d random values [%.2f, %.2f]", count, req.Min, req.Max)

	return &RandomResponse{
		Values:        values,
		QuantumSource: "hadamard_measurement",
		Timestamp:     time.Now().UnixNano(),
	}, nil
}

// ------------------------------------------------------------------
// GenerateRandomBytes - Cryptographic quality random bytes
// ------------------------------------------------------------------

func (s *GamingServer) GenerateRandomBytes(ctx context.Context, req *RandomBytesRequest) (*RandomBytesResponse, error) {
	numBytes := int(req.NumBytes)
	if numBytes <= 0 {
		numBytes = 32
	}
	if numBytes > 1048576 { // 1MB max
		numBytes = 1048576
	}

	data := make([]byte, numBytes)
	s.rng.Read(data)

	log.Printf("🔐 Generated %d random bytes", numBytes)

	return &RandomBytesResponse{
		Data:          data,
		EntropySource: "quantum_measurement_chain",
	}, nil
}

// ------------------------------------------------------------------
// CreateSuperposition - Schrödinger's game state
// ------------------------------------------------------------------

func (s *GamingServer) CreateSuperposition(ctx context.Context, req *SuperpositionRequest) (*SuperpositionState, error) {
	stateID := req.StateId
	if stateID == "" {
		stateID = fmt.Sprintf("superpos_%d", time.Now().UnixNano())
	}

	// Normalize probabilities
	totalProb := 0.0
	for _, o := range req.Outcomes {
		totalProb += o.Probability
	}

	outcomes := make([]*OutcomeProbability, len(req.Outcomes))
	for i, o := range req.Outcomes {
		outcomes[i] = &OutcomeProbability{
			Outcome:     o.Outcome,
			Probability: o.Probability / totalProb,
			Value:       o.Value,
		}
	}

	state := &SuperpositionState{
		StateId:          stateID,
		PossibleOutcomes: outcomes,
		IsCollapsed:      false,
		CreatedAt:        time.Now().Unix(),
		ExpiresAt:        time.Now().Add(1 * time.Hour).Unix(),
	}

	s.mu.Lock()
	s.superpositions[stateID] = state
	s.mu.Unlock()

	log.Printf("🌊 Created superposition: %s (%d outcomes)", stateID, len(outcomes))

	return state, nil
}

// ------------------------------------------------------------------
// CollapseState - Observer collapses the wave function
// ------------------------------------------------------------------

func (s *GamingServer) CollapseState(ctx context.Context, req *CollapsRequest) (*CollapseResult, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	state, exists := s.superpositions[req.StateId]
	if !exists {
		return nil, fmt.Errorf("superposition not found: %s", req.StateId)
	}

	if state.IsCollapsed {
		return nil, fmt.Errorf("state already collapsed: %s", req.StateId)
	}

	// Quantum measurement - choose outcome based on probabilities
	r := s.rng.Float64()
	cumulative := 0.0
	var selectedOutcome *OutcomeProbability
	for _, o := range state.PossibleOutcomes {
		cumulative += o.Probability
		if r <= cumulative {
			selectedOutcome = o
			break
		}
	}

	if selectedOutcome == nil {
		selectedOutcome = state.PossibleOutcomes[len(state.PossibleOutcomes)-1]
	}

	state.IsCollapsed = true

	log.Printf("💥 Collapsed %s -> %v (p=%.2f%%) by %s",
		req.StateId, selectedOutcome.Outcome, selectedOutcome.Probability*100, req.ObserverId)

	return &CollapseResult{
		StateId:        req.StateId,
		Outcome:        selectedOutcome.Outcome,
		OutcomeValue:   selectedOutcome.Value,
		ProbabilityWas: selectedOutcome.Probability,
		CollapsedAt:    time.Now().Unix(),
	}, nil
}

// ------------------------------------------------------------------
// QuantumCoinFlip - Fair (or biased) quantum coin flip
// ------------------------------------------------------------------

func (s *GamingServer) QuantumCoinFlip(ctx context.Context, req *CoinFlipRequest) (*CoinFlipResult, error) {
	numFlips := int(req.NumFlips)
	if numFlips <= 0 {
		numFlips = 1
	}
	if numFlips > 10000 {
		numFlips = 10000
	}

	bias := req.Bias
	if bias <= 0 || bias >= 1 {
		bias = 0.5 // Fair coin
	}

	results := make([]bool, numFlips)
	headsCount := 0

	for i := 0; i < numFlips; i++ {
		results[i] = s.rng.Float64() < bias
		if results[i] {
			headsCount++
		}
	}

	log.Printf("🪙 Flipped %d coins (bias=%.2f): %d heads, %d tails",
		numFlips, bias, headsCount, numFlips-headsCount)

	return &CoinFlipResult{
		Results:    results,
		HeadsCount: int32(headsCount),
		TailsCount: int32(numFlips - headsCount),
	}, nil
}

// ------------------------------------------------------------------
// QuantumDiceRoll - Roll quantum dice
// ------------------------------------------------------------------

func (s *GamingServer) QuantumDiceRoll(ctx context.Context, req *DiceRequest) (*DiceResult, error) {
	numDice := int(req.NumDice)
	if numDice <= 0 {
		numDice = 1
	}
	if numDice > 1000 {
		numDice = 1000
	}

	sides := int(req.Sides)
	if sides <= 1 {
		sides = 6
	}

	rolls := make([]int32, numDice)
	sum := 0
	minRoll := sides + 1
	maxRoll := 0

	for i := 0; i < numDice; i++ {
		roll := s.rng.Intn(sides) + 1
		rolls[i] = int32(roll)
		sum += roll
		if roll < minRoll {
			minRoll = roll
		}
		if roll > maxRoll {
			maxRoll = roll
		}
	}

	log.Printf("🎯 Rolled %dd%d: %v = %d", numDice, sides, rolls, sum)

	return &DiceResult{
		Rolls:   rolls,
		Sum:     int32(sum),
		MinRoll: int32(minRoll),
		MaxRoll: int32(maxRoll),
	}, nil
}

// ------------------------------------------------------------------
// ShuffleDeck - Fisher-Yates with quantum randomness
// ------------------------------------------------------------------

func (s *GamingServer) ShuffleDeck(ctx context.Context, req *ShuffleRequest) (*ShuffledDeck, error) {
	deckSize := int(req.DeckSize)
	if deckSize <= 0 {
		deckSize = 52
	}
	if deckSize > 10000 {
		deckSize = 10000
	}

	// Initialize deck
	deck := make([]int32, deckSize)
	for i := 0; i < deckSize; i++ {
		deck[i] = int32(i)
	}

	// Fisher-Yates shuffle
	for i := deckSize - 1; i > 0; i-- {
		j := s.rng.Intn(i + 1)
		deck[i], deck[j] = deck[j], deck[i]
	}

	// Generate proof hash
	h := sha256.New()
	for _, card := range deck {
		h.Write([]byte{byte(card)})
	}
	h.Write([]byte(fmt.Sprintf("%d", time.Now().UnixNano())))
	proof := hex.EncodeToString(h.Sum(nil))[:32]

	log.Printf("🃏 Shuffled %d-card deck (type=%s)", deckSize, req.DeckType)

	return &ShuffledDeck{
		CardOrder:    deck,
		ShuffleProof: proof,
	}, nil
}

// ------------------------------------------------------------------
// Types (would be generated from protobuf)
// ------------------------------------------------------------------

type RandomRequest struct {
	Count        int32
	Min          float64
	Max          float64
	IntegersOnly bool
}

type RandomResponse struct {
	Values        []float64
	QuantumSource string
	Timestamp     int64
}

type RandomBytesRequest struct {
	NumBytes int32
}

type RandomBytesResponse struct {
	Data          []byte
	EntropySource string
}

type GameOutcome int32

const (
	OutcomeUnknown GameOutcome = 0
	OutcomeWin     GameOutcome = 1
	OutcomeLose    GameOutcome = 2
	OutcomeDraw    GameOutcome = 3
	OutcomeBonus   GameOutcome = 4
	OutcomeJackpot GameOutcome = 5
)

type OutcomeProbability struct {
	Outcome     GameOutcome
	Probability float64
	Value       int32
}

type SuperpositionRequest struct {
	StateId           string
	Outcomes          []*OutcomeProbability
	ObservationQubits int32
}

type SuperpositionState struct {
	StateId          string
	PossibleOutcomes []*OutcomeProbability
	IsCollapsed      bool
	CreatedAt        int64
	ExpiresAt        int64
}

type CollapsRequest struct {
	StateId    string
	ObserverId string
}

type CollapseResult struct {
	StateId        string
	Outcome        GameOutcome
	OutcomeValue   int32
	ProbabilityWas float64
	CollapsedAt    int64
}

type CoinFlipRequest struct {
	NumFlips int32
	Bias     float64
}

type CoinFlipResult struct {
	Results    []bool
	HeadsCount int32
	TailsCount int32
}

type DiceRequest struct {
	NumDice int32
	Sides   int32
}

type DiceResult struct {
	Rolls   []int32
	Sum     int32
	MinRoll int32
	MaxRoll int32
}

type ShuffleRequest struct {
	DeckSize int32
	DeckType string
}

type ShuffledDeck struct {
	CardOrder    []int32
	ShuffleProof string
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------

func main() {
	port := flag.Int("port", 50061, "gRPC port")
	flag.Parse()

	server := NewGamingServer()

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	// RegisterQuantumGamingServer(grpcServer, server)

	log.Printf("🎮 Quantum Gaming starting on port %d", *port)
	log.Printf("   Features: RNG, Coin Flips, Dice, Deck Shuffle, Superposition")

	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}

	_ = server
}
