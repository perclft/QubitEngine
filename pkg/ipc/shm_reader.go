package ipc

import (
	"context"
	"encoding/json"
	"fmt"

	"os"
	"time"

	pb "github.com/perclft/QubitEngine/api/generated"
)

// ShmDescriptor defines the JSON metadata received over gRPC in StateResponse.shm_descriptor
type ShmDescriptor struct {
	FormatVersion uint32 `json:"format_version"`
	SegmentName   string `json:"segment_name"`
	SizeBytes     uint64 `json:"size_bytes"`
	NumQubits     uint32 `json:"num_qubits"`
	DataType      string `json:"data_type"`
	CreatedAtMs   int64  `json:"created_at_ms"`
	AckToken      string `json:"ack_token"`
}

// ReadAndAcknowledgeStateVector parses a JSON descriptor, maps the shared memory segment,
// reads complex128 values directly into a Go slice, unmaps the segment, and sends an ACK to C++.
func ReadAndAcknowledgeStateVector(ctx context.Context, descJSON string, client pb.QuantumComputeClient) ([]complex128, error) {
	var desc ShmDescriptor
	if err := json.Unmarshal([]byte(descJSON), &desc); err != nil {
		return nil, fmt.Errorf("invalid SHM descriptor JSON: %w", err)
	}

	if desc.FormatVersion != 1 {
		return nil, fmt.Errorf("unsupported SHM format version %d (expected 1)", desc.FormatVersion)
	}

	if desc.SegmentName == "" || desc.SizeBytes == 0 {
		return nil, fmt.Errorf("empty SHM segment name or size")
	}

	// Staleness check: age must not exceed 3000ms
	nowMs := time.Now().UnixMilli()
	if desc.CreatedAtMs > 0 {
		age := nowMs - desc.CreatedAtMs
		if age > 3000 || age < -3000 {
			return nil, fmt.Errorf("stale SHM segment (age %dms exceeds 3000ms threshold)", age)
		}
	}

	data, err := readSegmentBytes(desc.SegmentName, desc.SizeBytes, desc.NumQubits)
	if err != nil {
		return nil, fmt.Errorf("failed to map/read SHM segment %s: %w", desc.SegmentName, err)
	}

	// Deterministic fault-injection hook for reader crash tests
	if hook := os.Getenv("QUBIT_SHM_TEST_HOOK"); hook == "pause_after_map" {
		fmt.Println("[SYNC:MAPPED_AWAITING_ACK]")
		os.Stdout.Sync()
		// Block indefinitely until terminated by the test runner
		select {}
	}

	// Send gRPC ACK to trigger immediate C++ segment unlinking
	if client != nil && desc.AckToken != "" {
		_, ackErr := client.AcknowledgeShmRead(ctx, &pb.ShmAckRequest{
			AckToken: desc.AckToken,
		})
		if ackErr != nil {
			// Log error but return data; C++ fallback timer will clean up segment if ACK RPC fails
			_ = ackErr
		}
	}

	return data, nil
}
