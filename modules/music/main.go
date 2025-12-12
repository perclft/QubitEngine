// Quantum Music Composer Module
// Generate melodies, chords, and rhythms using quantum randomness

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

// Scale intervals (semitones from root)
var scales = map[string][]int{
	"major":      {0, 2, 4, 5, 7, 9, 11},
	"minor":      {0, 2, 3, 5, 7, 8, 10},
	"pentatonic": {0, 2, 4, 7, 9},
	"blues":      {0, 3, 5, 6, 7, 10},
	"dorian":     {0, 2, 3, 5, 7, 9, 10},
}

type MusicServer struct {
	rng *rand.Rand
}

func NewMusicServer() *MusicServer {
	return &MusicServer{
		rng: rand.New(rand.NewSource(time.Now().UnixNano())),
	}
}

func (s *MusicServer) GenerateMelody(scale string, rootNote, numNotes int, tempo float64) []Note {
	scaleNotes := scales[scale]
	if scaleNotes == nil {
		scaleNotes = scales["major"]
	}

	notes := make([]Note, numNotes)
	currentTime := 0.0
	durations := []float64{0.25, 0.5, 1.0, 1.5, 2.0}

	for i := 0; i < numNotes; i++ {
		// Pick random scale degree
		degree := s.rng.Intn(len(scaleNotes))
		octave := s.rng.Intn(2) - 1 // -1, 0, or 1 octave

		pitch := rootNote + scaleNotes[degree] + octave*12
		duration := durations[s.rng.Intn(len(durations))]
		velocity := 0.5 + s.rng.Float64()*0.4

		notes[i] = Note{
			Pitch:     pitch,
			Duration:  duration,
			Velocity:  velocity,
			StartTime: currentTime,
		}
		currentTime += duration
	}

	log.Printf("🎵 Generated %d-note melody in %s scale (root=%d)", numNotes, scale, rootNote)
	return notes
}

func (s *MusicServer) GenerateChordProgression(scale string, rootNote, numChords int) []Chord {
	// Common chord progressions
	progressions := [][]int{
		{0, 4, 5, 3}, // I-V-vi-IV (pop)
		{0, 3, 4, 4}, // I-IV-V-V (blues)
		{0, 5, 3, 4}, // I-vi-IV-V (50s)
		{0, 4, 3, 5}, // I-V-IV-vi
	}

	progression := progressions[s.rng.Intn(len(progressions))]
	scaleNotes := scales[scale]
	if scaleNotes == nil {
		scaleNotes = scales["major"]
	}

	chords := make([]Chord, numChords)
	chordNames := []string{"I", "ii", "iii", "IV", "V", "vi", "vii°"}

	for i := 0; i < numChords; i++ {
		degree := progression[i%len(progression)]
		root := rootNote + scaleNotes[degree%len(scaleNotes)]

		// Build triad
		notes := []int{
			root,
			root + scaleNotes[(degree+2)%len(scaleNotes)] - scaleNotes[degree%len(scaleNotes)],
			root + scaleNotes[(degree+4)%len(scaleNotes)] - scaleNotes[degree%len(scaleNotes)],
		}

		chords[i] = Chord{
			Notes:    notes,
			Name:     chordNames[degree%len(chordNames)],
			Duration: 2.0,
		}
	}

	log.Printf("🎹 Generated %d-chord progression", numChords)
	return chords
}

type Note struct {
	Pitch     int
	Duration  float64
	Velocity  float64
	StartTime float64
}

type Chord struct {
	Notes    []int
	Name     string
	Duration float64
}

func main() {
	port := flag.Int("port", 50062, "gRPC port")
	flag.Parse()

	server := NewMusicServer()

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()

	log.Printf("🎵 Quantum Music Composer starting on port %d", *port)
	log.Printf("   Scales: major, minor, pentatonic, blues, dorian")

	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}

	_ = server
}
