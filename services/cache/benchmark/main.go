package main

import (
	"encoding/json"
	"fmt"
	"time"

	pb "github.com/perclft/QubitEngine/services/cache/generated"
	"google.golang.org/protobuf/proto"
)

// Define old structures for comparison
type OldCachedEntry struct {
	Result    *OldStateResult `json:"result"`
	CachedAt  int64           `json:"cached_at"`
	ExpiresAt int64           `json:"expires_at"`
	HitCount  int32           `json:"hit_count"`
}

type OldStateResult struct {
	StateVector []OldComplexNumber `json:"state_vector"`
	ServerId    string             `json:"server_id"`
}

type OldComplexNumber struct {
	Real float64 `json:"real"`
	Imag float64 `json:"imag"`
}

func createTestData(numQubits int) (*OldCachedEntry, *pb.CachedEntry) {
	dim := 1 << numQubits
	now := time.Now().Unix()

	oldEntry := &OldCachedEntry{
		Result: &OldStateResult{
			StateVector: make([]OldComplexNumber, dim),
			ServerId:    "test-server",
		},
		CachedAt:  now,
		ExpiresAt: now + 3600,
		HitCount:  0,
	}

	newEntry := &pb.CachedEntry{
		Result: &pb.StateResult{
			StateVector: make([]*pb.ComplexNumber, dim),
			ServerId:    "test-server",
		},
		CachedAt:  now,
		ExpiresAt: now + 3600,
		HitCount:  0,
	}

	for i := 0; i < dim; i++ {
		oldEntry.Result.StateVector[i] = OldComplexNumber{Real: 0.5, Imag: 0.5}
		newEntry.Result.StateVector[i] = &pb.ComplexNumber{Real: 0.5, Imag: 0.5}
	}

	return oldEntry, newEntry
}

func main() {
	// Benchmark for different qubit counts
	for _, qubits := range []int{10, 15, 20} {
		fmt.Printf("\n--- Benchmarking %d qubits (dim=%d) ---\n", qubits, 1<<qubits)
		oldEntry, newEntry := createTestData(qubits)

		// JSON
		start := time.Now()
		jsonData, _ := json.Marshal(oldEntry)
		jsonMarshalTime := time.Since(start)
		
		start = time.Now()
		var unpackedOld OldCachedEntry
		json.Unmarshal(jsonData, &unpackedOld)
		jsonUnmarshalTime := time.Since(start)

		// Proto
		start = time.Now()
		protoData, _ := proto.Marshal(newEntry)
		protoMarshalTime := time.Since(start)

		start = time.Now()
		var unpackedNew pb.CachedEntry
		proto.Unmarshal(protoData, &unpackedNew)
		protoUnmarshalTime := time.Since(start)

		fmt.Printf("JSON:  Size=%10d bytes | Marshal=%10v | Unmarshal=%10v\n", 
			len(jsonData), jsonMarshalTime, jsonUnmarshalTime)
		fmt.Printf("Proto: Size=%10d bytes | Marshal=%10v | Unmarshal=%10v\n", 
			len(protoData), protoMarshalTime, protoUnmarshalTime)
		fmt.Printf("Ratio: Size=%.2fx | Speed=%.2fx better\n", 
			float64(len(jsonData))/float64(len(protoData)),
			float64(jsonMarshalTime.Nanoseconds())/float64(protoMarshalTime.Nanoseconds()))
	}
}
