package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"time"

	"github.com/perclft/QubitEngine/api/auth"
	pb "github.com/perclft/QubitEngine/api/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/protobuf/types/known/emptypb"
)

// tokenAuth implements grpc.PerRPCCredentials to inject the auth header.
type tokenAuth struct {
	token string
}

func (t tokenAuth) GetRequestMetadata(ctx context.Context, uri ...string) (map[string]string, error) {
	return map[string]string{
		"authorization": "Bearer " + t.token,
	}, nil
}

func (t tokenAuth) RequireTransportSecurity() bool {
	return false
}

func main() {
	var conn *grpc.ClientConn
	var err error
	
	// Ensure JWT secret is set for test environments
	if os.Getenv("QUBIT_ENGINE_JWT_SECRET") == "" {
		os.Setenv("QUBIT_ENGINE_JWT_SECRET", "e2e_ping_test_secret_1234567890")
	}

	token, err := auth.GenerateToken("test-user", 24*time.Hour)
	if err != nil {
		log.Fatalf("Failed to generate token: %v", err)
	}
	
	// The cluster might take a few seconds to boot up entirely.
	for i := 0; i < 15; i++ {
		conn, err = grpc.NewClient("localhost:50053", 
			grpc.WithTransportCredentials(insecure.NewCredentials()),
			grpc.WithPerRPCCredentials(tokenAuth{token: token}),
		)
		if err == nil {
			break
		}
		log.Printf("Waiting for Scheduler gRPC on port 50053... (%d/15)\n", i+1)
		time.Sleep(2 * time.Second)
	}
	
	if err != nil {
		log.Fatalf("Failed to connect to scheduler: %v", err)
	}
	defer conn.Close()

	client := pb.NewQuantumSchedulerClient(conn)
	
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	metrics, err := client.GetClusterMetrics(ctx, &emptypb.Empty{})
	if err != nil {
		log.Fatalf("Failed to get cluster metrics: %v", err)
	}

	fmt.Printf("✅ Successfully connected to Scheduler via gRPC!\n")
	fmt.Printf("Cluster Metrics — Active Workers: %d, QueueDepth: %d\n", metrics.ActiveWorkers, metrics.QueueDepth)
}
