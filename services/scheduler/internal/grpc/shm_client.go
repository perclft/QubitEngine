package grpc

import (
	"context"
	"fmt"
	"log/slog"

	pb "github.com/perclft/QubitEngine/api/generated"
	"github.com/perclft/QubitEngine/pkg/ipc"
)

// ExecuteWithSHM executes a circuit via the C++ engine gRPC client, enabling
// zero-copy shared memory transfer if supported by the host OS and circuit size.
// Returns the StateResponse and the deserialized complex state vector.
func ExecuteWithSHM(ctx context.Context, client pb.QuantumComputeClient, req *pb.CircuitRequest) (*pb.StateResponse, []complex128, error) {
	if client == nil {
		return nil, nil, fmt.Errorf("engine client is nil")
	}

	// Auto-enable SHM if available on host and circuit qualifies (N >= 12)
	if req.NumQubits >= 12 && ipc.ProbeSharedMemoryAccess() {
		req.UseShm = true
	}

	resp, err := client.RunCircuit(ctx, req)
	if err != nil {
		return nil, nil, err
	}

	// If engine returned a shared-memory descriptor, map and read it
	if resp.ShmDescriptor != "" {
		vec, err := ipc.ReadAndAcknowledgeStateVector(ctx, resp.ShmDescriptor, client)
		if err == nil {
			return resp, vec, nil
		}
		slog.Warn("Failed to read SHM state vector, falling back to protobuf", "error", err)
	}

	// Fallback to inline protobuf array
	vec := make([]complex128, len(resp.StateVector))
	for i, c := range resp.StateVector {
		vec[i] = complex(c.Real, c.Imag)
	}

	return resp, vec, nil
}
