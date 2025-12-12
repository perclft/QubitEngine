// Quantum Education Module
// Interactive learning, quizzes, and circuit library

package main

import (
	"flag"
	"fmt"
	"log"
	"math/rand"
	"net"
	"time"

	"google.golang.org/grpc"
)

// Lesson catalog
var lessons = map[string]*Lesson{
	"superposition_intro": {
		ID:    "superposition_intro",
		Topic: "SUPERPOSITION",
		Title: "Introduction to Quantum Superposition",
		Content: `# Quantum Superposition

Superposition is the fundamental quantum principle where a qubit can exist in 
multiple states simultaneously until measured.

## Key Concepts
- A classical bit is either 0 or 1
- A qubit can be |0⟩, |1⟩, or any superposition: α|0⟩ + β|1⟩
- |α|² and |β|² represent measurement probabilities
- Measurement "collapses" the superposition

## The Hadamard Gate
The Hadamard gate creates an equal superposition:
- H|0⟩ = (|0⟩ + |1⟩)/√2
- H|1⟩ = (|0⟩ - |1⟩)/√2
`,
		KeyConcepts:     []string{"Superposition", "Qubit States", "Measurement", "Hadamard Gate"},
		CircuitExamples: []string{"hadamard_single", "superposition_demo"},
		NextLessonID:    "entanglement_intro",
		EstimatedMin:    15,
	},
	"entanglement_intro": {
		ID:    "entanglement_intro",
		Topic: "ENTANGLEMENT",
		Title: "Quantum Entanglement Explained",
		Content: `# Quantum Entanglement

Entanglement creates correlations between qubits that cannot be explained classically.

## Bell States
The four maximally entangled two-qubit states:
- |Φ+⟩ = (|00⟩ + |11⟩)/√2
- |Φ-⟩ = (|00⟩ - |11⟩)/√2
- |Ψ+⟩ = (|01⟩ + |10⟩)/√2
- |Ψ-⟩ = (|01⟩ - |10⟩)/√2

## Creating Entanglement
1. Apply Hadamard to first qubit
2. Apply CNOT with first as control

## Spooky Action at a Distance
Einstein called entanglement "spooky action at a distance" because measuring 
one qubit instantly determines the other, regardless of distance.
`,
		KeyConcepts:     []string{"Entanglement", "Bell States", "CNOT Gate", "EPR Paradox"},
		CircuitExamples: []string{"bell_state", "ghz_state"},
		NextLessonID:    "gates_intro",
		EstimatedMin:    20,
	},
}

// Circuit library
var circuits = map[string]*Circuit{
	"hadamard_single": {
		ID:          "hadamard_single",
		Name:        "Single Hadamard",
		Description: "Apply Hadamard gate to create |+⟩ state",
		NumQubits:   1,
		Gates:       []GateStep{{Gate: "H", Qubits: []int{0}}},
		Output:      "|+⟩ = (|0⟩ + |1⟩)/√2",
	},
	"bell_state": {
		ID:          "bell_state",
		Name:        "Bell State |Φ+⟩",
		Description: "Create maximally entangled Bell state",
		NumQubits:   2,
		Gates: []GateStep{
			{Gate: "H", Qubits: []int{0}},
			{Gate: "CNOT", Qubits: []int{0, 1}},
		},
		Output: "|Φ+⟩ = (|00⟩ + |11⟩)/√2",
	},
	"ghz_state": {
		ID:          "ghz_state",
		Name:        "GHZ State (3 qubits)",
		Description: "Greenberger–Horne–Zeilinger state",
		NumQubits:   3,
		Gates: []GateStep{
			{Gate: "H", Qubits: []int{0}},
			{Gate: "CNOT", Qubits: []int{0, 1}},
			{Gate: "CNOT", Qubits: []int{0, 2}},
		},
		Output: "|GHZ⟩ = (|000⟩ + |111⟩)/√2",
	},
}

// Quiz questions
var questions = []Question{
	{
		ID:      "q1",
		Type:    "multiple_choice",
		Text:    "What state does H|0⟩ produce?",
		Options: []string{"|0⟩", "|1⟩", "(|0⟩ + |1⟩)/√2", "(|0⟩ - |1⟩)/√2"},
		Answer:  "2",
		Explain: "The Hadamard gate creates an equal superposition: H|0⟩ = |+⟩ = (|0⟩ + |1⟩)/√2",
	},
	{
		ID:      "q2",
		Type:    "true_false",
		Text:    "Measuring an entangled qubit affects its partner instantaneously.",
		Answer:  "true",
		Explain: "Entangled qubits share quantum correlations - measuring one instantly determines the other's state.",
	},
	{
		ID:      "q3",
		Type:    "multiple_choice",
		Text:    "Which gates create a Bell state from |00⟩?",
		Options: []string{"H, H", "CNOT, H", "H, CNOT", "X, CNOT"},
		Answer:  "2",
		Explain: "H on first qubit creates superposition, then CNOT entangles the pair.",
	},
}

type Lesson struct {
	ID              string
	Topic           string
	Title           string
	Content         string
	KeyConcepts     []string
	CircuitExamples []string
	NextLessonID    string
	EstimatedMin    int
}

type Circuit struct {
	ID          string
	Name        string
	Description string
	NumQubits   int
	Gates       []GateStep
	Output      string
}

type GateStep struct {
	Gate   string
	Qubits []int
	Param  float64
}

type Question struct {
	ID      string
	Type    string
	Text    string
	Options []string
	Answer  string
	Explain string
}

type EducationServer struct {
	rng *rand.Rand
}

func NewEducationServer() *EducationServer {
	return &EducationServer{
		rng: rand.New(rand.NewSource(time.Now().UnixNano())),
	}
}

func (s *EducationServer) GetLesson(id string) *Lesson {
	return lessons[id]
}

func (s *EducationServer) GetCircuit(id string) *Circuit {
	return circuits[id]
}

func (s *EducationServer) GenerateQuiz(topic string, numQuestions int) []Question {
	// Shuffle and select questions
	shuffled := make([]Question, len(questions))
	copy(shuffled, questions)
	s.rng.Shuffle(len(shuffled), func(i, j int) {
		shuffled[i], shuffled[j] = shuffled[j], shuffled[i]
	})

	if numQuestions > len(shuffled) {
		numQuestions = len(shuffled)
	}
	return shuffled[:numQuestions]
}

func main() {
	port := flag.Int("port", 50065, "gRPC port")
	flag.Parse()

	server := NewEducationServer()

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()

	log.Printf("📚 Quantum Education starting on port %d", *port)
	log.Printf("   Lessons: %d available", len(lessons))
	log.Printf("   Circuits: %d in library", len(circuits))
	log.Printf("   Questions: %d in quiz bank", len(questions))

	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}

	_ = server
}
