package main

import (
	"context"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/go-redis/redis/v8"
	api "github.com/perclft/QubitEngine/api/generated"
	pb "github.com/perclft/QubitEngine/services/cache/generated"
)

func TestGetCachedResult_HitCountUpdate(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("failed to start miniredis: %v", err)
	}
	defer mr.Close()

	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	server := NewCacheServer(rdb, 10*time.Minute)

	req := &pb.CacheRequest{
		CircuitHash:   "test_hash_12345",
		NumQubits:     2,
		NumOperations: 2,
		Result: &api.StateResponse{
			StateVector: []*api.StateResponse_ComplexNumber{
				{Real: 0.7071, Imag: 0},
				{Real: 0, Imag: 0},
				{Real: 0, Imag: 0},
				{Real: 0.7071, Imag: 0},
			},
			ServerId: "node-1",
		},
	}

	ctx := context.Background()
	_, err = server.CacheResult(ctx, req)
	if err != nil {
		t.Fatalf("CacheResult failed: %v", err)
	}

	hit1, err := server.GetCachedResult(ctx, &pb.CacheLookup{CircuitHash: "test_hash_12345"})
	if err != nil || !hit1.Found || hit1.HitCount != 1 {
		t.Fatalf("First GetCachedResult failed, hit: %+v, err: %v", hit1, err)
	}

	hit2, err := server.GetCachedResult(ctx, &pb.CacheLookup{CircuitHash: "test_hash_12345"})
	if err != nil || !hit2.Found || hit2.HitCount != 2 {
		t.Fatalf("Second GetCachedResult failed, expected HitCount 2, got %d, err: %v", hit2.HitCount, err)
	}
}
