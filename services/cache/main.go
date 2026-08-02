package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"flag"
	"fmt"
	"log/slog"
	"net"
	"os"
	"os/signal"
	"sync/atomic"
	"syscall"
	"time"
	"strings"

	"github.com/go-redis/redis/v8"
	"github.com/perclft/QubitEngine/api/auth"
	api "github.com/perclft/QubitEngine/api/generated"
	pb "github.com/perclft/QubitEngine/services/cache/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/health"
	"google.golang.org/grpc/health/grpc_health_v1"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"

	"go.opentelemetry.io/contrib/instrumentation/google.golang.org/grpc/otelgrpc"
	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/exporters/otlp/otlptrace/otlptracegrpc"
	"go.opentelemetry.io/otel/propagation"
	"go.opentelemetry.io/otel/sdk/resource"
	sdktrace "go.opentelemetry.io/otel/sdk/trace"
	semconv "go.opentelemetry.io/otel/semconv/v1.4.0"
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

	slog.Info("Cached result", "hash", shortHash(req.CircuitHash), "qubits", req.NumQubits, "ops", req.NumOperations, "ttl", ttl)

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
	if err := s.rdb.Set(ctx, cacheKey, updatedData, redis.KeepTTL).Err(); err != nil {
		slog.Warn("Failed to update cache hit count in Redis", "hash", shortHash(req.CircuitHash), "error", err)
	}

	slog.Info("Cache HIT", "hash", shortHash(req.CircuitHash), "hits", entry.HitCount)

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
		slog.Info("Cache invalidated", "hash", shortHash(req.CircuitHash))
		return &pb.CacheResponse{Success: true, Message: "Cache invalidated"}, nil
	}

	return &pb.CacheResponse{Success: false, Message: "Key not found"}, nil
}

// ------------------------------------------------------------------
// GetCacheStats - Get cache statistics
// ------------------------------------------------------------------

func (s *CacheServer) GetCacheStats(ctx context.Context, req *pb.CacheEmpty) (*pb.CacheStats, error) {
	// Count cache entries using SCAN (non-blocking, unlike KEYS)
	var totalEntries int64
	var cursor uint64
	for {
		keys, nextCursor, err := s.rdb.Scan(ctx, cursor, "cache:*", 100).Result()
		if err != nil {
			break
		}
		totalEntries += int64(len(keys))
		cursor = nextCursor
		if cursor == 0 {
			break
		}
	}

	// Get memory info
	info, _ := s.rdb.Info(ctx, "memory").Result()
	var memUsed int64 = 0
	lines := strings.Split(info, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if strings.HasPrefix(line, "used_memory:") {
			fmt.Sscanf(line, "used_memory:%d", &memUsed)
			break
		}
	}

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

func shortHash(h string) string {
	if len(h) > 16 {
		return h[:16]
	}
	return h
}

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

func initTracer() (*sdktrace.TracerProvider, error) {
	ctx := context.Background()
	exp, err := otlptracegrpc.New(ctx, otlptracegrpc.WithInsecure())
	if err != nil {
		return nil, err
	}

	tp := sdktrace.NewTracerProvider(
		sdktrace.WithBatcher(exp),
		sdktrace.WithResource(resource.NewWithAttributes(
			semconv.SchemaURL,
			semconv.ServiceNameKey.String("qubit-cache"),
		)),
	)
	otel.SetTracerProvider(tp)
	otel.SetTextMapPropagator(propagation.NewCompositeTextMapPropagator(propagation.TraceContext{}, propagation.Baggage{}))
	return tp, nil
}

func main() {
	redisAddr := flag.String("redis-addr", "localhost:6379", "Redis address")
	port := flag.Int("port", 50054, "gRPC port")
	ttlMinutes := flag.Int("default-ttl", 60, "Default cache TTL in minutes")
	flag.Parse()

	// Initialize OpenTelemetry
	tp, err := initTracer()
	if err != nil {
		slog.Error("Failed to initialize tracer", "error", err)
	} else {
		defer tp.Shutdown(context.Background())
		slog.Info("OpenTelemetry tracer initialized")
	}

	// Connect to Redis
	rdb := redis.NewClient(&redis.Options{
		Addr:     *redisAddr,
		Password: "",
		DB:       1, // Use different DB than scheduler
	})

	ctx := context.Background()
	if err := rdb.Ping(ctx).Err(); err != nil {
		slog.Error("Failed to connect to Redis", "error", err)
		os.Exit(1)
	}
	slog.Info("Connected to Redis (DB 1 - Cache)")

	// Create server
	defaultTTL := time.Duration(*ttlMinutes) * time.Minute
	server := NewCacheServer(rdb, defaultTTL)

	// Start gRPC server
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		slog.Error("Failed to listen", "error", err)
		os.Exit(1)
	}

	validateToken := func(ctx context.Context) error {
		token, err := auth.ExtractTokenFromContext(ctx)
		if err != nil {
			return err
		}
		_, err = auth.ValidateToken(token)
		if err != nil {
			return status.Errorf(codes.Unauthenticated, "invalid authorization token: %v", err)
		}
		return nil
	}

	authInterceptor := func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
		if strings.HasPrefix(info.FullMethod, "/grpc.health.v1.Health/") {
			return handler(ctx, req)
		}
		if err := validateToken(ctx); err != nil {
			return nil, err
		}
		return handler(ctx, req)
	}

	streamAuthInterceptor := func(srv interface{}, ss grpc.ServerStream, info *grpc.StreamServerInfo, handler grpc.StreamHandler) error {
		if strings.HasPrefix(info.FullMethod, "/grpc.health.v1.Health/") {
			return handler(srv, ss)
		}
		if err := validateToken(ss.Context()); err != nil {
			return err
		}
		return handler(srv, ss)
	}

	grpcServer := grpc.NewServer(
		grpc.StatsHandler(otelgrpc.NewServerHandler()),
		grpc.UnaryInterceptor(authInterceptor),
		grpc.StreamInterceptor(streamAuthInterceptor),
	)
	pb.RegisterResultCacheServer(grpcServer, server)

	// Register health check service
	healthServer := health.NewServer()
	grpc_health_v1.RegisterHealthServer(grpcServer, healthServer)
	healthServer.SetServingStatus("", grpc_health_v1.HealthCheckResponse_SERVING)

	slog.Info("Result Cache starting", "port", *port, "redis", *redisAddr, "ttl", defaultTTL)

	// Graceful shutdown on SIGINT/SIGTERM
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, os.Interrupt, syscall.SIGTERM)

	go func() {
		if err := grpcServer.Serve(lis); err != nil {
			slog.Error("Failed to serve", "error", err)
		os.Exit(1)
		}
	}()

	sig := <-quit
	slog.Info("Shutdown signal received", "signal", sig)
	grpcServer.GracefulStop()
	rdb.Close()
	slog.Info("Cache service shut down gracefully")
}
