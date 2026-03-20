package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/go-redis/redis/v8"
	pb "github.com/perclft/QubitEngine/api/generated"
	internal_grpc "github.com/perclft/QubitEngine/services/scheduler/internal/grpc"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"github.com/improbable-eng/grpc-web/go/grpcweb"
)

var (
	queueDepthMetric = promauto.NewGauge(prometheus.GaugeOpts{
		Name: "qubitengine_queue_depth",
		Help: "The current depth of the Redis jobs queue",
	})
	streamLagMetric = promauto.NewGauge(prometheus.GaugeOpts{
		Name: "qubitengine_stream_lag_seconds",
		Help: "Approximated processing lag over streams",
	})
)

func main() {
	redisAddr := flag.String("redis-addr", "localhost:6379", "Redis address")
	engineAddr := flag.String("engine-addr", "engine:50051", "Engine gRPC address")
	port := flag.Int("port", 50053, "gRPC port")
	flag.Parse()

	logger := slog.New(slog.NewJSONHandler(os.Stdout, nil))
	slog.SetDefault(logger)

	// --- Auth Token Configuration ---
	skipAuth := os.Getenv("QUBIT_ENGINE_SKIP_AUTH") == "1"
	authToken := os.Getenv("QUBIT_ENGINE_AUTH_TOKEN")
	if !skipAuth && authToken == "" {
		slog.Error("QUBIT_ENGINE_AUTH_TOKEN must be set. To disable auth for development, set QUBIT_ENGINE_SKIP_AUTH=1")
		os.Exit(1)
	}
	if skipAuth {
		slog.Warn("Authentication is DISABLED (QUBIT_ENGINE_SKIP_AUTH=1). Do not use in production.")
	}

	rdb := redis.NewClient(&redis.Options{
		Addr:     *redisAddr,
		Password: "",
		DB:       0,
	})

	ctx := context.Background()
	if err := rdb.Ping(ctx).Err(); err != nil {
		slog.Error("Failed to connect to Redis", "error", err)
		os.Exit(1)
	}
	slog.Info("Connected to Redis")

	server := internal_grpc.NewSchedulerServer(rdb, *engineAddr)
	if err := server.ConnectEngine(ctx); err != nil {
		slog.Error("Failed to connect to engine", "error", err)
		os.Exit(1)
	}
	server.StartWorkers(ctx)

	http.Handle("/metrics", promhttp.Handler())
	go func() {
		slog.Info("Starting Prometheus metrics server on :2112/metrics")
		if err := http.ListenAndServe(":2112", nil); err != nil {
			slog.Error("Metrics server failed", "error", err)
		}
	}()

	go func() {
		for {
			depth, err := rdb.ZCard(ctx, "queue:jobs").Result()
			if err == nil {
				queueDepthMetric.Set(float64(depth))
			}
			time.Sleep(5 * time.Second)
		}
	}()

	validateToken := func(ctx context.Context) error {
		if skipAuth {
			return nil
		}
		md, ok := metadata.FromIncomingContext(ctx)
		if !ok {
			return status.Errorf(codes.Unauthenticated, "metadata is not provided")
		}
		authHeader, ok := md["authorization"]
		if !ok || len(authHeader) == 0 {
			return status.Errorf(codes.Unauthenticated, "authorization token is not provided")
		}
		token := strings.TrimPrefix(authHeader[0], "Bearer ")
		if token != authToken {
			return status.Errorf(codes.Unauthenticated, "invalid authorization token")
		}
		return nil
	}

	authInterceptor := func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
		if err := validateToken(ctx); err != nil {
			return nil, err
		}
		return handler(ctx, req)
	}

	streamAuthInterceptor := func(srv interface{}, ss grpc.ServerStream, info *grpc.StreamServerInfo, handler grpc.StreamHandler) error {
		if err := validateToken(ss.Context()); err != nil {
			return err
		}
		return handler(srv, ss)
	}

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		slog.Error("Failed to listen", "error", err)
		os.Exit(1)
	}

	grpcServer := grpc.NewServer(
		grpc.UnaryInterceptor(authInterceptor),
		grpc.StreamInterceptor(streamAuthInterceptor),
	)
	pb.RegisterQuantumSchedulerServer(grpcServer, server)

	slog.Info("Quantum Scheduler starting",
		"port", *port,
		"grpc-web", ":8080",
		"redis", *redisAddr,
		"engine", *engineAddr,
	)

	wrappedGrpc := grpcweb.WrapServer(grpcServer, grpcweb.WithOriginFunc(func(origin string) bool { return true }))

	httpServer := &http.Server{
		Addr: ":8080",
		Handler: http.HandlerFunc(func(res http.ResponseWriter, req *http.Request) {
			if wrappedGrpc.IsGrpcWebRequest(req) {
				wrappedGrpc.ServeHTTP(res, req)
			} else {
				http.DefaultServeMux.ServeHTTP(res, req)
			}
		}),
	}
	
	go func() {
		if err := httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			slog.Error("gRPC-Web server failed", "error", err)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, os.Interrupt, syscall.SIGTERM)

	go func() {
		if err := grpcServer.Serve(lis); err != nil {
			slog.Error("Failed to serve", "error", err)
			os.Exit(1)
		}
	}()

	sig := <-quit
	slog.Info("Shutdown signal received", "signal", sig.String())

	shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	grpcServer.GracefulStop()
	httpServer.Shutdown(shutdownCtx)
	slog.Info("Scheduler shut down gracefully")
}
