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
	"syscall"
	"strings"
	"time"

	"github.com/go-redis/redis/v8"
	"github.com/perclft/QubitEngine/api/auth"
	pb "github.com/perclft/QubitEngine/api/generated"
	internal_grpc "github.com/perclft/QubitEngine/services/scheduler/internal/grpc"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/health"
	"google.golang.org/grpc/health/grpc_health_v1"
	"google.golang.org/grpc/peer"
	"google.golang.org/grpc/status"
	"github.com/improbable-eng/grpc-web/go/grpcweb"

	"go.opentelemetry.io/contrib/instrumentation/google.golang.org/grpc/otelgrpc"
	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/exporters/otlp/otlptrace/otlptracegrpc"
	"go.opentelemetry.io/otel/propagation"
	"go.opentelemetry.io/otel/sdk/resource"
	sdktrace "go.opentelemetry.io/otel/sdk/trace"
	semconv "go.opentelemetry.io/otel/semconv/v1.4.0"
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
			semconv.ServiceNameKey.String("qubit-scheduler"),
		)),
	)
	otel.SetTracerProvider(tp)
	otel.SetTextMapPropagator(propagation.NewCompositeTextMapPropagator(propagation.TraceContext{}, propagation.Baggage{}))
	return tp, nil
}

func main() {
	redisAddr := flag.String("redis-addr", "localhost:6379", "Redis address")
	engineAddr := flag.String("engine-addr", "engine:50051", "Engine gRPC address")
	port := flag.Int("port", 50053, "gRPC port")
	flag.Parse()

	logger := slog.New(slog.NewJSONHandler(os.Stdout, nil))
	slog.SetDefault(logger)

	// Initialize OpenTelemetry
	tp, err := initTracer()
	if err != nil {
		slog.Error("Failed to initialize tracer", "error", err)
	} else {
		defer tp.Shutdown(context.Background())
		slog.Info("OpenTelemetry tracer initialized")
	}

	// --- Auth Token Configuration ---
	slog.Info("Strict JWT Authentication is ENABLED. No bypasses allowed for client requests.")
	authToken := os.Getenv("QUBIT_ENGINE_AUTH_TOKEN")
	if authToken == "" {
		slog.Warn("QUBIT_ENGINE_AUTH_TOKEN is not set. Engine communication might fail if engine requires auth.")
	}

	rdb := redis.NewClient(&redis.Options{
		Addr:     *redisAddr,
		Password: os.Getenv("REDIS_PASSWORD"),
		DB:       0,
	})

	ctx := context.Background()
	if err := rdb.Ping(ctx).Err(); err != nil {
		slog.Error("Failed to connect to Redis", "error", err)
		os.Exit(1)
	}
	slog.Info("Connected to Redis")

	server := internal_grpc.NewSchedulerServer(rdb, *engineAddr, authToken)
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

	checkRateLimit := func(ctx context.Context, token string) error {
		var key string
		if token != "" {
			key = "rl:token:" + token + ":" + time.Now().Format("200601021504")
		} else {
			p, ok := peer.FromContext(ctx)
			if ok && p.Addr != nil {
				addrStr := p.Addr.String()
				// Always extract just the host/IP, stripping any port.
				// This prevents rate-limit bypass via different source ports.
				host, _, err := net.SplitHostPort(addrStr)
				if err != nil {
					// SplitHostPort failed — addr may be a bare IP or IPv6 without port.
					// Use the raw address but strip any IPv6 brackets.
					host = strings.TrimRight(strings.TrimLeft(addrStr, "["), "]")
				}
				key = "rl:ip:" + host + ":" + time.Now().Format("200601021504")
			} else {
				key = "rl:unknown:" + time.Now().Format("200601021504")
			}
		}

		val, err := rdb.Incr(ctx, key).Result()
		if err != nil {
			return status.Errorf(codes.Internal, "rate limiter error: %v", err)
		}
		if val == 1 {
			rdb.Expire(ctx, key, 60*time.Second)
		}
		if val > 120 { // 120 requests per minute
			return status.Errorf(codes.ResourceExhausted, "rate limit exceeded (120 req/min)")
		}
		return nil
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

		// Apply Rate Limiting
		if err := checkRateLimit(ctx, token); err != nil {
			return err
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
		grpc.StatsHandler(otelgrpc.NewServerHandler()),
		grpc.UnaryInterceptor(authInterceptor),
		grpc.StreamInterceptor(streamAuthInterceptor),
	)
	pb.RegisterQuantumSchedulerServer(grpcServer, server)

	// Register health check service
	healthServer := health.NewServer()
	grpc_health_v1.RegisterHealthServer(grpcServer, healthServer)
	healthServer.SetServingStatus("", grpc_health_v1.HealthCheckResponse_SERVING)

	slog.Info("Quantum Scheduler starting",
		"port", *port,
		"grpc-web", ":8080",
		"redis", *redisAddr,
		"engine", *engineAddr,
	)

	allowedOrigins := os.Getenv("ALLOWED_ORIGINS")
	wrappedGrpc := grpcweb.WrapServer(grpcServer, grpcweb.WithOriginFunc(func(origin string) bool {
		if allowedOrigins == "" || allowedOrigins == "*" {
			return true
		}
		for _, allowed := range strings.Split(allowedOrigins, ",") {
			if strings.TrimSpace(allowed) == origin {
				return true
			}
		}
		return false
	}))

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
