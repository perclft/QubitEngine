package service

import (
	"fmt"
	"log"

	pb "github.com/perclft/QubitEngine/api/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// ConnectToEngine establishes a gRPC connection to the Quantum Engine on the specified address.
// It returns the client connection (which should be closed by the caller) and the QuantumComputeClient.
func ConnectToEngine(address string) (*grpc.ClientConn, pb.QuantumComputeClient, error) {
	conn, err := grpc.Dial(address, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Printf("⚠️ Failed to connect to engine at %s: %v", address, err)
		return nil, nil, fmt.Errorf("failed to connect to engine: %w", err)
	}

	client := pb.NewQuantumComputeClient(conn)
	return conn, client, nil
}
