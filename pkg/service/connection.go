package service

import (
	"fmt"
	"log"
	"os"

	pb "github.com/perclft/QubitEngine/api/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/credentials/insecure"
)

// ConnectToEngine establishes a gRPC connection to the Quantum Engine on the specified address.
// It returns the client connection (which should be closed by the caller) and the QuantumComputeClient.
func ConnectToEngine(address string) (*grpc.ClientConn, pb.QuantumComputeClient, error) {
	certPath := os.Getenv("QUBIT_ENGINE_CERT_PATH")
	var creds grpc.DialOption
	if certPath != "" {
		c, err := credentials.NewClientTLSFromFile(certPath, "")
		if err != nil {
			log.Printf("⚠️ Failed to load TLS certificate %s: %v", certPath, err)
			return nil, nil, fmt.Errorf("failed to load TLS certificate: %w", err)
		}
		creds = grpc.WithTransportCredentials(c)
	} else {
		creds = grpc.WithTransportCredentials(insecure.NewCredentials())
	}

	conn, err := grpc.NewClient(address, creds)
	if err != nil {
		log.Printf("⚠️ Failed to connect to engine at %s: %v", address, err)
		return nil, nil, fmt.Errorf("failed to connect to engine: %w", err)
	}

	client := pb.NewQuantumComputeClient(conn)
	return conn, client, nil
}
