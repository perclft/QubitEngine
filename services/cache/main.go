package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"sync/atomic"
	"syscall"
	"time"

	"github.com/go-redis/redis/v8"
	api "github.com/perclft/QubitEngine/api/generated"
	pb "github.com/perclft/QubitEngine/services/cache/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
)

// ------------------------------------------------------------------
// Cache Server
// ------------------------------------------------------------------

type CacheServer struct {
	pb.UnimplementedResultCacheServer
	rdb        *redis.Client
	defaultTTL time.Duration
	hits       int64
	misses     int64
}

func NewCacheServer(rdb *redis.Client, defaultTTL time.Duration) *CacheServer {
	return &CacheServer{
		rdb:        rdb,
		defaultTTL: defaultTTL,
	}
}

// ------------------------------------------------------------------
// CacheResult - Store a circuit result
// ------------------------------------------------------------------

func (s *CacheServer) CacheResult(ctx context.Context, req *pb.CacheRequest) (*pb.CacheResponse, error) {
	if req.CircuitHash == "" {
		return nil, status.Error(codes.InvalidArgument, "circuit_hash required")
	}

	cacheKey := fmt.Sprintf("cache:%s", req.CircuitHash)

	ttl := s.defaultTTL
	if req.TtlSeconds > 0 {
		ttl = time.Duration(req.TtlSeconds) * time.Second
	}

	now := time.Now().Unix()
	entry := &pb.CachedEntry{
		Result: &pb.StateResult{
			StateVector: make([]*pb.ComplexNumber, len(req.Result.StateVector)),
			ServerId:    req.Result.ServerId,
		},
		CachedAt:  now,
		ExpiresAt: now + int64(ttl.Seconds()),
		HitCount:  0,
	}

	for i, c := range req.Result.StateVector {
		entry.Result.StateVector[i] = &pb.ComplexNumber{Real: c.Real, Imag: c.Imag}
	}

	data, err := proto.Marshal(entry)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to serialize: %v", err)
	}

	if err := s.rdb.Set(ctx, cacheKey, data, ttl).Err(); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to cache: %v", err)
	}

	log.Printf("💾 Cached result: %s (qubits=%d, ops=%d, TTL=%v)",
		req.CircuitHash[:16], req.NumQubits, req.NumOperations, ttl)

	return &pb.CacheResponse{
		Success:  true,
		Message:  "Result cached successfully",
		CacheKey: cacheKey,
	}, nil
}

// ------------------------------------------------------------------
// GetCachedResult - Retrieve a cached result
// ------------------------------------------------------------------

func (s *CacheServer) GetCachedResult(ctx context.Context, req *pb.CacheLookup) (*pb.CacheHit, error) {
	cacheKey := fmt.Sprintf("cache:%s", req.CircuitHash)

	data, err := s.rdb.Get(ctx, cacheKey).Bytes()
	if err == redis.Nil {
		atomic.AddInt64(&s.misses, 1)
		return &pb.CacheHit{Found: false}, nil
	}
	if err != nil {
		return nil, status.Errorf(codes.Internal, "redis error: %v", err)
	}

	var entry pb.CachedEntry
	if err := proto.Unmarshal(data, &entry); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to parse cache: %v", err)
	}

	// Increment hit count
	entry.HitCount++
	atomic.AddInt64(&s.hits, 1)

	// Update the entry with new hit count
	updatedData, _ := proto.Marshal(&entry)
	s.rdb.Set(ctx, cacheKey, updatedData, 0) // Keep existing TTL

	log.Printf("✅ Cache HIT: %s (hits=%d)", req.CircuitHash[:16], entry.HitCount)

	return &pb.CacheHit{
		Found:     true,
		Result:    toStateResponse(entry.Result),
		CachedAt:  entry.CachedAt,
		ExpiresAt: entry.ExpiresAt,
		HitCount:  entry.HitCount,
	}, nil
}

// ------------------------------------------------------------------
// InvalidateCache - Remove a cached result
// ------------------------------------------------------------------

func (s *CacheServer) InvalidateCache(ctx context.Context, req *pb.CacheLookup) (*pb.CacheResponse, error) {
	cacheKey := fmt.Sprintf("cache:%s", req.CircuitHash)

	deleted, err := s.rdb.Del(ctx, cacheKey).Result()
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to invalidate: %v", err)
	}

	if deleted > 0 {
		log.Printf("🗑️ Cache invalidated: %s", req.CircuitHash[:16])
		return &pb.CacheResponse{Success: true, Message: "Cache invalidated"}, nil
	}

	return &pb.CacheResponse{Success: false, Message: "Key not found"}, nil
}

// ------------------------------------------------------------------
// GetCacheStats - Get cache statistics
// ------------------------------------------------------------------

func (s *CacheServer) GetCacheStats(ctx context.Context, req *pb.CacheEmpty) (*pb.CacheStats, error) {
	// Count cache entries
	keys, _ := s.rdb.Keys(ctx, "cache:*").Result()
	totalEntries := int64(len(keys))

	// Get memory info
	info, _ := s.rdb.Info(ctx, "memory").Result()
	var memUsed int64 = 0
	// Parse memory from info string (simplified)
	fmt.Sscanf(info, "used_memory:%d", &memUsed)

	hits := atomic.LoadInt64(&s.hits)
	misses := atomic.LoadInt64(&s.misses)
	total := hits + misses
	hitRate := 0.0
	if total > 0 {
		hitRate = float64(hits) / float64(total)
	}

	return &pb.CacheStats{
		TotalEntries:    totalEntries,
		TotalHits:       hits,
		TotalMisses:     misses,
		HitRate:         hitRate,
		MemoryUsedBytes: memUsed,
	}, nil
}

// ------------------------------------------------------------------
// Helper: Hash a circuit for cache key
// ------------------------------------------------------------------

func HashCircuit(numQubits int32, operations []byte) string {
	h := sha256.New()
	h.Write([]byte(fmt.Sprintf("%d", numQubits)))
	h.Write(operations)
	return hex.EncodeToString(h.Sum(nil))
}

func toStateResponse(sr *pb.StateResult) *api.StateResponse {
	resp := &api.StateResponse{
		StateVector: make([]*api.StateResponse_ComplexNumber, len(sr.StateVector)),
		ServerId:    sr.ServerId,
	}
	for i, c := range sr.StateVector {
		resp.StateVector[i] = &api.StateResponse_ComplexNumber{Real: c.Real, Imag: c.Imag}
	}
	return resp
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------

func main() {
	redisAddr := flag.String("redis-addr", "localhost:6379", "Redis address")
	port := flag.Int("port", 50054, "gRPC port")
	ttlMinutes := flag.Int("default-ttl", 60, "Default cache TTL in minutes")
	flag.Parse()

	// Connect to Redis
	rdb := redis.NewClient(&redis.Options{
		Addr:     *redisAddr,
		Password: "",
		DB:       1, // Use different DB than scheduler
	})

	ctx := context.Background()
	if err := rdb.Ping(ctx).Err(); err != nil {
		log.Fatalf("Failed to connect to Redis: %v", err)
	}
	log.Println("Connected to Redis (DB 1 - Cache)")

	// Create server
	defaultTTL := time.Duration(*ttlMinutes) * time.Minute
	server := NewCacheServer(rdb, defaultTTL)

	// Start gRPC server
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	pb.RegisterResultCacheServer(grpcServer, server)

	log.Printf("📦 Result Cache starting on port %d", *port)
	log.Printf("   Redis: %s (DB 1)", *redisAddr)
	log.Printf("   Default TTL: %v", defaultTTL)

	// Graceful shutdown on SIGINT/SIGTERM
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, os.Interrupt, syscall.SIGTERM)

	go func() {
		if err := grpcServer.Serve(lis); err != nil {
			log.Fatalf("Failed to serve: %v", err)
		}
	}()

	sig := <-quit
	log.Printf("Shutdown signal received: %v", sig)
	grpcServer.GracefulStop()
	rdb.Close()
	log.Println("Cache service shut down gracefully")
}
