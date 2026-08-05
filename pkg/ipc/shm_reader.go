package ipc

import (
	"context"
	"encoding/json"
	"fmt"

	pb "github.com/perclft/QubitEngine/api/generated"
)

// ShmDescriptor defines the JSON metadata received over gRPC in StateResponse.shm_descriptor
type ShmDescriptor struct {
	SegmentName string `json:"segment_name"`
	SizeBytes   uint64 `json:"size_bytes"`
	NumQubits   uint32 `json:"num_qubits"`
	DataType    string `json:"data_type"`
	CreatedAtMs int64  `json:"created_at_ms"`
	AckToken    string `json:"ack_token"`
}

// ReadAndAcknowledgeStateVector parses a JSON descriptor, maps the shared memory segment,
// reads complex128 values directly into a Go slice, unmaps the segment, and sends an ACK to C++.
func ReadAndAcknowledgeStateVector(ctx context.Context, descJSON string, client pb.QuantumComputeClient) ([]complex128, error) {
	var desc ShmDescriptor
	if err := json.Unmarshal([]byte(descJSON), &desc); err != nil {
		return nil, fmt.Errorf("invalid SHM descriptor JSON: %w", err)
	}

	if desc.SegmentName == "" || desc.SizeBytes == 0 {
		return nil, fmt.Errorf("empty SHM segment name or size")
	}

	data, err := readSegmentBytes(desc.SegmentName, desc.SizeBytes, desc.NumQubits)
	if err != nil {
		return nil, fmt.Errorf("failed to map/read SHM segment %s: %w", desc.SegmentName, err)
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
