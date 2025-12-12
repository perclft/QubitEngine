package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net"
	"sync/atomic"
	"time"

	"github.com/go-redis/redis/v8"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// ------------------------------------------------------------------
// Cache Types
// ------------------------------------------------------------------

type CachedEntry struct {
	Result    *StateResult `json:"result"`
	CachedAt  int64        `json:"cached_at"`
	ExpiresAt int64        `json:"expires_at"`
	HitCount  int32        `json:"hit_count"`
}

type StateResult struct {
	StateVector []ComplexNumber `json:"state_vector"`
	ServerId    string          `json:"server_id"`
}

type ComplexNumber struct {
	Real float64 `json:"real"`
	Imag float64 `json:"imag"`
}

// ------------------------------------------------------------------
// Cache Server
// ------------------------------------------------------------------

type CacheServer struct {
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

func (s *CacheServer) CacheResult(ctx context.Context, req *CacheRequest) (*CacheResponse, error) {
	if req.CircuitHash == "" {
		return nil, status.Error(codes.InvalidArgument, "circuit_hash required")
	}

	cacheKey := fmt.Sprintf("cache:%s", req.CircuitHash)

	ttl := s.defaultTTL
	if req.TtlSeconds > 0 {
		ttl = time.Duration(req.TtlSeconds) * time.Second
	}

	now := time.Now().Unix()
	entry := &CachedEntry{
		Result: &StateResult{
			StateVector: make([]ComplexNumber, len(req.Result.StateVector)),
			ServerId:    req.Result.ServerId,
		},
		CachedAt:  now,
		ExpiresAt: now + int64(ttl.Seconds()),
		HitCount:  0,
	}

	for i, c := range req.Result.StateVector {
		entry.Result.StateVector[i] = ComplexNumber{Real: c.Real, Imag: c.Imag}
	}

	data, err := json.Marshal(entry)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to serialize: %v", err)
	}

	if err := s.rdb.Set(ctx, cacheKey, data, ttl).Err(); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to cache: %v", err)
	}

	log.Printf("💾 Cached result: %s (qubits=%d, ops=%d, TTL=%v)",
		req.CircuitHash[:16], req.NumQubits, req.NumOperations, ttl)

	return &CacheResponse{
		Success:  true,
		Message:  "Result cached successfully",
		CacheKey: cacheKey,
	}, nil
}

// ------------------------------------------------------------------
// GetCachedResult - Retrieve a cached result
// ------------------------------------------------------------------

func (s *CacheServer) GetCachedResult(ctx context.Context, req *CacheLookup) (*CacheHit, error) {
	cacheKey := fmt.Sprintf("cache:%s", req.CircuitHash)

	data, err := s.rdb.Get(ctx, cacheKey).Bytes()
	if err == redis.Nil {
		atomic.AddInt64(&s.misses, 1)
		return &CacheHit{Found: false}, nil
	}
	if err != nil {
		return nil, status.Errorf(codes.Internal, "redis error: %v", err)
	}

	var entry CachedEntry
	if err := json.Unmarshal(data, &entry); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to parse cache: %v", err)
	}

	// Increment hit count
	entry.HitCount++
	atomic.AddInt64(&s.hits, 1)

	// Update the entry with new hit count
	updatedData, _ := json.Marshal(entry)
	s.rdb.Set(ctx, cacheKey, updatedData, 0) // Keep existing TTL

	log.Printf("✅ Cache HIT: %s (hits=%d)", req.CircuitHash[:16], entry.HitCount)

	return &CacheHit{
		Found:     true,
		Result:    entry.Result.ToProto(),
		CachedAt:  entry.CachedAt,
		ExpiresAt: entry.ExpiresAt,
		HitCount:  entry.HitCount,
	}, nil
}

// ------------------------------------------------------------------
// InvalidateCache - Remove a cached result
// ------------------------------------------------------------------

func (s *CacheServer) InvalidateCache(ctx context.Context, req *CacheLookup) (*CacheResponse, error) {
	cacheKey := fmt.Sprintf("cache:%s", req.CircuitHash)

	deleted, err := s.rdb.Del(ctx, cacheKey).Result()
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to invalidate: %v", err)
	}

	if deleted > 0 {
		log.Printf("🗑️ Cache invalidated: %s", req.CircuitHash[:16])
		return &CacheResponse{Success: true, Message: "Cache invalidated"}, nil
	}

	return &CacheResponse{Success: false, Message: "Key not found"}, nil
}

// ------------------------------------------------------------------
// GetCacheStats - Get cache statistics
// ------------------------------------------------------------------

func (s *CacheServer) GetCacheStats(ctx context.Context, req *Empty) (*CacheStats, error) {
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

	return &CacheStats{
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

// ------------------------------------------------------------------
// Placeholder types (would be generated from protobuf)
// ------------------------------------------------------------------

type CacheRequest struct {
	CircuitHash   string
	NumQubits     int32
	NumOperations int32
	Result        *StateResponse
	TtlSeconds    int32
}

type StateResponse struct {
	StateVector []*Complex
	ServerId    string
}

type Complex struct {
	Real float64
	Imag float64
}

type CacheResponse struct {
	Success  bool
	Message  string
	CacheKey string
}

type CacheLookup struct {
	CircuitHash string
}

type CacheHit struct {
	Found     bool
	Result    *StateResponse
	CachedAt  int64
	ExpiresAt int64
	HitCount  int32
}

type Empty struct{}

type CacheStats struct {
	TotalEntries    int64
	TotalHits       int64
	TotalMisses     int64
	HitRate         float64
	MemoryUsedBytes int64
}

func (sr *StateResult) ToProto() *StateResponse {
	resp := &StateResponse{
		StateVector: make([]*Complex, len(sr.StateVector)),
		ServerId:    sr.ServerId,
	}
	for i, c := range sr.StateVector {
		resp.StateVector[i] = &Complex{Real: c.Real, Imag: c.Imag}
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
	// RegisterResultCacheServer(grpcServer, server)

	log.Printf("📦 Result Cache starting on port %d", *port)
	log.Printf("   Redis: %s (DB 1)", *redisAddr)
	log.Printf("   Default TTL: %v", defaultTTL)

	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}

	_ = server // Silence unused variable warning
}
