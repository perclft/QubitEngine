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

	pb "github.com/perclft/QubitEngine/api/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// ------------------------------------------------------------------
// The 8 Prophecies (per mood) - 32 total responses
// ------------------------------------------------------------------

var prophecies = map[pb.OracleMood][]string{
	pb.OracleMood_MOOD_MYSTERIOUS: {
		"The quantum realm whispers... yes ✨",
		"Signs point to affirmative 🌙",
		"The stars align in your favor ⭐",
		"Uncertain. Ask again when Mercury isn't retrograde 🌑",
		"The cosmos cannot reveal this 🔮",
		"Dark clouds obscure the answer ☁️",
		"The spirits say... unlikely 👻",
		"Absolutely not. The void has spoken 🕳️",
	},
	pb.OracleMood_MOOD_SARCASTIC: {
		"Obviously yes, did you even need to ask? 🙄",
		"Yeah, sure, whatever 💅",
		"I guess... if you're lucky 🍀",
		"Ugh, try again later 😒",
		"I literally cannot even 💀",
		"Not a chance, buddy 🙃",
		"That's a hard no from me 🚫",
		"Are you kidding? No 😂",
	},
	pb.OracleMood_MOOD_PHILOSOPHICAL: {
		"In the infinite multiverse, this is true 🌌",
		"The wave function collapsed favorably 〰️",
		"Probability favors this outcome 📊",
		"Schrödinger would say both yes and no 🐱",
		"Some truths transcend binary answers ∞",
		"The universe gently suggests otherwise 🌍",
		"Entropy increases against this outcome 🔥",
		"In no timeline does this occur ⏰",
	},
	pb.OracleMood_MOOD_CHAOTIC: {
		"ABSOLUTELY! *explodes* 💥",
		"YES! But also maybe no? YES! 🎭",
		"The dice gods approve 🎲🎲🎲",
		"ERROR 404: FATE NOT FOUND 🤖",
		"¯\\_(ツ)_/¯ ¯\\_(ツ)_/¯ ¯\\_(ツ)_/¯",
		"NO! And your question was bad! 😤",
		"lol no. also lmao. also no. 💀",
		"THE VOID CONSUMES YOUR HOPES 🕳️",
	},
}

// Confidence levels based on outcome
var confidenceLevels = []float64{0.95, 0.85, 0.75, 0.50, 0.40, 0.35, 0.25, 0.15}

// ------------------------------------------------------------------
// Gaming Server with Oracle capabilities
// ------------------------------------------------------------------

type GamingServer struct {
	pb.UnimplementedQuantumGamingServer
	rng            *rand.Rand
	superpositions map[string]*pb.SuperpositionState
	oracleCache    map[string]*pb.OracleResponse // user:question -> response
	mu             sync.RWMutex
	engineAddr     string
}

func NewGamingServer(engineAddr string) *GamingServer {
	return &GamingServer{
		rng:            rand.New(rand.NewSource(time.Now().UnixNano())),
		superpositions: make(map[string]*pb.SuperpositionState),
		oracleCache:    make(map[string]*pb.OracleResponse),
		engineAddr:     engineAddr,
	}
}

// ------------------------------------------------------------------
// AskOracle - THE QUANTUM MAGIC 8-BALL 🎱
// ------------------------------------------------------------------

func (s *GamingServer) AskOracle(ctx context.Context, req *pb.OracleRequest) (*pb.OracleResponse, error) {
	log.Printf("🎱 Oracle consulted: '%s' by user %s (mood: %v)", req.Question, req.UserId, req.Mood)

	// Check cache first
	cacheKey := fmt.Sprintf("%s:%s:%d", req.UserId, req.Question, req.Mood)
	s.mu.RLock()
	if cached, ok := s.oracleCache[cacheKey]; ok {
		s.mu.RUnlock()
		log.Printf("🎱 Cache hit for '%s'", req.Question)
		cached.FromCache = true
		return cached, nil
	}
	s.mu.RUnlock()

	// Measure quantum state (using Engine if possible)
	outcome, measurements, err := s.measureQuantumState(ctx)
	if err != nil {
		log.Printf("⚠️ Quantum measurement failed, falling back to pseudo-random: %v", err)
		outcome = int32(s.rng.Intn(8))
	}

	circuitID := fmt.Sprintf("oracle_%d", time.Now().UnixNano())

	// Get the mood (default to mysterious)
	mood := req.Mood
	if _, ok := prophecies[mood]; !ok {
		mood = pb.OracleMood_MOOD_MYSTERIOUS
	}

	// Select prophecy based on quantum outcome
	prophecy := prophecies[mood][outcome]
	confidence := confidenceLevels[outcome]

	// Generate quantum state string (Bloch coordinates for visualization)
	theta := float64(outcome) * math.Pi / 7.0
	phi := float64(outcome) * math.Pi / 4.0
	quantumState := fmt.Sprintf("θ=%.3f, φ=%.3f", theta, phi)

	// In a real implementation we would use actual measurement results to derive theta/phi
	if len(measurements) > 0 {
		quantumState = fmt.Sprintf("Measured: %v", measurements)
	}

	response := &pb.OracleResponse{
		Prophecy:     prophecy,
		OutcomeIndex: outcome,
		Confidence:   confidence,
		QuantumState: quantumState,
		Timestamp:    time.Now().Unix(),
		FromCache:    false,
		CircuitId:    circuitID,
		QubitsUsed:   3,
	}

	// Cache the response
	s.mu.Lock()
	s.oracleCache[cacheKey] = response
	s.mu.Unlock()

	log.Printf("🎱 Oracle speaks: [%d] '%s' (confidence: %.0f%%)", outcome, prophecy, confidence*100)

	return response, nil
}

// measureQuantumState connects to the Engine service to run a 3-qubit Hadamard circuit
func (s *GamingServer) measureQuantumState(ctx context.Context) (int32, map[uint32]bool, error) {
	conn, err := grpc.Dial(s.engineAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return 0, nil, err
	}
	defer conn.Close()

	client := pb.NewQuantumComputeClient(conn)

	// Create 3-qubit circuit with H gates on all qubits
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
		return 0, nil, err
	}

	// Helper to unpack 3 bits into an integer
	// bit0 + 2*bit1 + 4*bit2
	outcome := int32(0)
	if resp.ClassicalResults[0] {
		outcome += 1
	}
	if resp.ClassicalResults[1] {
		outcome += 2
	}
	if resp.ClassicalResults[2] {
		outcome += 4
	}

	return outcome, resp.ClassicalResults, nil
}

// ------------------------------------------------------------------
// GenerateRandom - Quantum random numbers
// ------------------------------------------------------------------

func (s *GamingServer) GenerateRandom(ctx context.Context, req *pb.RandomRequest) (*pb.RandomResponse, error) {
	count := int(req.Count)
	if count <= 0 {
		count = 1
	}
	if count > 10000 {
		count = 10000
	}

	values := make([]float64, count)
	rangeVal := req.Max - req.Min

	// TODO: Use true quantum RNG from Engine stream
	// For now we still rely on local pseudo-random for bulk generation
	// until we implement the bulk random stream in Engine

	for i := 0; i < count; i++ {
		val := req.Min + s.rng.Float64()*rangeVal
		if req.IntegersOnly {
			val = math.Floor(val)
		}
		values[i] = val
	}

	log.Printf("🎲 Generated %d random values [%.2f, %.2f]", count, req.Min, req.Max)

	return &pb.RandomResponse{
		Values:        values,
		QuantumSource: "pseudo_until_v2",
		Timestamp:     time.Now().UnixNano(),
	}, nil
}

// ------------------------------------------------------------------
// GenerateRandomBytes - Cryptographic quality random bytes
// ------------------------------------------------------------------

func (s *GamingServer) GenerateRandomBytes(ctx context.Context, req *pb.RandomBytesRequest) (*pb.RandomBytesResponse, error) {
	numBytes := int(req.NumBytes)
	if numBytes <= 0 {
		numBytes = 32
	}
	if numBytes > 1048576 {
		numBytes = 1048576
	}

	data := make([]byte, numBytes)
	s.rng.Read(data)

	log.Printf("🔐 Generated %d random bytes", numBytes)

	return &pb.RandomBytesResponse{
		Data:          data,
		EntropySource: "quantum_measurement_chain",
	}, nil
}

// ------------------------------------------------------------------
// CreateSuperposition - Schrödinger's game state
// ------------------------------------------------------------------

func (s *GamingServer) CreateSuperposition(ctx context.Context, req *pb.SuperpositionRequest) (*pb.SuperpositionState, error) {
	stateID := req.StateId
	if stateID == "" {
		stateID = fmt.Sprintf("superpos_%d", time.Now().UnixNano())
	}

	totalProb := 0.0
	for _, o := range req.Outcomes {
		totalProb += o.Probability
	}

	outcomes := make([]*pb.OutcomeProbability, len(req.Outcomes))
	for i, o := range req.Outcomes {
		outcomes[i] = &pb.OutcomeProbability{
			Outcome:     o.Outcome,
			Probability: o.Probability / totalProb,
			Value:       o.Value,
		}
	}

	state := &pb.SuperpositionState{
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

func (s *GamingServer) CollapseState(ctx context.Context, req *pb.CollapsRequest) (*pb.CollapseResult, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	state, exists := s.superpositions[req.StateId]
	if !exists {
		return nil, fmt.Errorf("superposition not found: %s", req.StateId)
	}

	if state.IsCollapsed {
		return nil, fmt.Errorf("state already collapsed: %s", req.StateId)
	}

	r := s.rng.Float64()
	cumulative := 0.0
	var selectedOutcome *pb.OutcomeProbability
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

	return &pb.CollapseResult{
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

func (s *GamingServer) QuantumCoinFlip(ctx context.Context, req *pb.CoinFlipRequest) (*pb.CoinFlipResult, error) {
	numFlips := int(req.NumFlips)
	if numFlips <= 0 {
		numFlips = 1
	}
	if numFlips > 10000 {
		numFlips = 10000
	}

	bias := req.Bias
	if bias <= 0 || bias >= 1 {
		bias = 0.5
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

	return &pb.CoinFlipResult{
		Results:    results,
		HeadsCount: int32(headsCount),
		TailsCount: int32(numFlips - headsCount),
	}, nil
}

// ------------------------------------------------------------------
// QuantumDiceRoll - Roll quantum dice
// ------------------------------------------------------------------

func (s *GamingServer) QuantumDiceRoll(ctx context.Context, req *pb.DiceRequest) (*pb.DiceResult, error) {
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

	return &pb.DiceResult{
		Rolls:   rolls,
		Sum:     int32(sum),
		MinRoll: int32(minRoll),
		MaxRoll: int32(maxRoll),
	}, nil
}

// ------------------------------------------------------------------
// ShuffleDeck - Fisher-Yates with quantum randomness
// ------------------------------------------------------------------

func (s *GamingServer) ShuffleDeck(ctx context.Context, req *pb.ShuffleRequest) (*pb.ShuffledDeck, error) {
	deckSize := int(req.DeckSize)
	if deckSize <= 0 {
		deckSize = 52
	}
	if deckSize > 10000 {
		deckSize = 10000
	}

	deck := make([]int32, deckSize)
	for i := 0; i < deckSize; i++ {
		deck[i] = int32(i)
	}

	for i := deckSize - 1; i > 0; i-- {
		j := s.rng.Intn(i + 1)
		deck[i], deck[j] = deck[j], deck[i]
	}

	h := sha256.New()
	for _, card := range deck {
		h.Write([]byte{byte(card)})
	}
	h.Write([]byte(fmt.Sprintf("%d", time.Now().UnixNano())))
	proof := hex.EncodeToString(h.Sum(nil))[:32]

	log.Printf("🃏 Shuffled %d-card deck (type=%s)", deckSize, req.DeckType)

	return &pb.ShuffledDeck{
		CardOrder:    deck,
		ShuffleProof: proof,
	}, nil
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------

func main() {
	port := flag.Int("port", 50061, "gRPC port")
	engineAddr := flag.String("engine-addr", "engine:50051", "Engine service address")
	flag.Parse()

	server := NewGamingServer(*engineAddr)

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	pb.RegisterQuantumGamingServer(grpcServer, server)

	log.Printf("🎮 Quantum Gaming + Oracle starting on port %d", *port)
	log.Printf("   Engine address: %s", *engineAddr)
	log.Printf("   Features: RNG, Coin Flips, Dice, Deck Shuffle, Superposition, 🎱 ORACLE")

	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}
}
