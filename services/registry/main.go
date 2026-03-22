package main

import (
	"context"
	"database/sql"
	"encoding/json"
	"flag"
	"fmt"
	"log/slog"
	"net"
	"os"
	"os/signal"
	"strconv"
	"syscall"
	"time"

	"github.com/google/uuid"
	_ "github.com/lib/pq"
	api "github.com/perclft/QubitEngine/api/generated"
	pb "github.com/perclft/QubitEngine/services/registry/generated"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/health"
	"google.golang.org/grpc/health/grpc_health_v1"
	"google.golang.org/grpc/status"

	"go.opentelemetry.io/contrib/instrumentation/google.golang.org/grpc/otelgrpc"
	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/exporters/otlp/otlptrace/otlptracegrpc"
	"go.opentelemetry.io/otel/propagation"
	"go.opentelemetry.io/otel/sdk/resource"
	sdktrace "go.opentelemetry.io/otel/sdk/trace"
	semconv "go.opentelemetry.io/otel/semconv/v1.4.0"
)

// CircuitRecord represents a row in the circuits table
type CircuitRecord struct {
	ID            string    `json:"id"`
	Name          string    `json:"name"`
	Description   string    `json:"description"`
	Author        string    `json:"author"`
	Domain        string    `json:"domain"`
	Tags          []string  `json:"tags"`
	NumQubits     int32     `json:"num_qubits"`
	NumOperations int32     `json:"num_operations"`
	Version       int32     `json:"version"`
	CircuitJSON   string    `json:"circuit_json"` // Serialized CircuitRequest
	IsPublic      bool      `json:"is_public"`
	ForkCount     int32     `json:"fork_count"`
	RunCount      int32     `json:"run_count"`
	CreatedAt     time.Time `json:"created_at"`
	UpdatedAt     time.Time `json:"updated_at"`
}

// RegistryServer implements the CircuitRegistry gRPC service
type RegistryServer struct {
	pb.UnimplementedCircuitRegistryServer
	db *sql.DB
}

func NewRegistryServer(db *sql.DB) *RegistryServer {
	return &RegistryServer{db: db}
}

// InitDB creates the circuits table if it doesn't exist
func InitDB(db *sql.DB) error {
	schema := `
	CREATE TABLE IF NOT EXISTS circuits (
		id UUID PRIMARY KEY,
		name VARCHAR(255) NOT NULL,
		description TEXT,
		author VARCHAR(255) NOT NULL DEFAULT 'anonymous',
		domain VARCHAR(50) NOT NULL DEFAULT 'general',
		tags JSONB DEFAULT '[]',
		num_qubits INTEGER NOT NULL,
		num_operations INTEGER NOT NULL,
		version INTEGER NOT NULL DEFAULT 1,
		circuit_json JSONB NOT NULL,
		is_public BOOLEAN DEFAULT true,
		fork_count INTEGER DEFAULT 0,
		run_count INTEGER DEFAULT 0,
		created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
		updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
	);
	
	CREATE INDEX IF NOT EXISTS idx_circuits_domain ON circuits(domain);
	CREATE INDEX IF NOT EXISTS idx_circuits_author ON circuits(author);
	CREATE INDEX IF NOT EXISTS idx_circuits_public ON circuits(is_public);
	CREATE INDEX IF NOT EXISTS idx_circuits_tags ON circuits USING gin(tags);
	`
	_, err := db.Exec(schema)
	return err
}

// SaveCircuit saves a new circuit to the registry
func (s *RegistryServer) SaveCircuit(ctx context.Context, req *pb.SaveCircuitRequest) (*pb.CircuitMetadata, error) {
	id := uuid.New().String()
	now := time.Now()

	// Serialize circuit to JSON
	circuitJSON, err := json.Marshal(req.Circuit)
	if err != nil {
		return nil, status.Errorf(codes.InvalidArgument, "failed to serialize circuit: %v", err)
	}

	tagsJSON, _ := json.Marshal(req.Tags)

	_, err = s.db.ExecContext(ctx, `
		INSERT INTO circuits (id, name, description, domain, tags, num_qubits, num_operations, circuit_json, is_public, created_at, updated_at)
		VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)
	`,
		id,
		req.Name,
		req.Description,
		req.Domain,
		string(tagsJSON),
		req.Circuit.NumQubits,
		len(req.Circuit.Operations),
		string(circuitJSON),
		req.IsPublic,
		now,
		now,
	)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to save circuit: %v", err)
	}

	return &pb.CircuitMetadata{
		Id:            id,
		Name:          req.Name,
		Description:   req.Description,
		Author:        "anonymous",
		Domain:        req.Domain,
		Tags:          req.Tags,
		NumQubits:     req.Circuit.NumQubits,
		NumOperations: int32(len(req.Circuit.Operations)),
		Version:       1,
		IsPublic:      req.IsPublic,
		CreatedAt:     now.Unix(),
		UpdatedAt:     now.Unix(),
	}, nil
}

// LoadCircuit retrieves a circuit by ID
func (s *RegistryServer) LoadCircuit(ctx context.Context, req *pb.LoadCircuitRequest) (*api.CircuitRequest, error) {
	var circuitJSON string
	err := s.db.QueryRowContext(ctx, `
		SELECT circuit_json FROM circuits WHERE id = $1
	`, req.CircuitId).Scan(&circuitJSON)

	if err == sql.ErrNoRows {
		return nil, status.Errorf(codes.NotFound, "circuit not found: %s", req.CircuitId)
	}
	if err != nil {
		return nil, status.Errorf(codes.Internal, "database error: %v", err)
	}

	// Increment run count
	s.db.ExecContext(ctx, `UPDATE circuits SET run_count = run_count + 1 WHERE id = $1`, req.CircuitId)

	var circuit api.CircuitRequest
	if err := json.Unmarshal([]byte(circuitJSON), &circuit); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to deserialize circuit: %v", err)
	}

	return &circuit, nil
}

// ListCircuits returns circuits matching the given filters
func (s *RegistryServer) ListCircuits(ctx context.Context, req *pb.ListCircuitsRequest) (*pb.CircuitList, error) {
	// Build WHERE clause for filtering
	whereClause := " WHERE 1=1"
	countArgs := []interface{}{}
	args := []interface{}{}
	argIdx := 1

	if req.Domain != "" {
		whereClause += fmt.Sprintf(" AND domain = $%d", argIdx)
		countArgs = append(countArgs, req.Domain)
		args = append(args, req.Domain)
		argIdx++
	}
	if req.Author != "" {
		whereClause += fmt.Sprintf(" AND author = $%d", argIdx)
		countArgs = append(countArgs, req.Author)
		args = append(args, req.Author)
		argIdx++
	}
	if req.PublicOnly {
		whereClause += " AND is_public = true"
	}

	// Get total count of matching rows
	var totalCount int32
	countQuery := "SELECT COUNT(*) FROM circuits" + whereClause
	if err := s.db.QueryRowContext(ctx, countQuery, countArgs...).Scan(&totalCount); err != nil {
		return nil, status.Errorf(codes.Internal, "count query failed: %v", err)
	}

	// Pagination
	pageSize := int(req.PageSize)
	if pageSize <= 0 || pageSize > 100 {
		pageSize = 20
	}
	page := int(req.Page)
	if page <= 0 {
		page = 1
	}
	offset := (page - 1) * pageSize

	query := `SELECT id, name, description, author, domain, tags, num_qubits, num_operations, version, is_public, fork_count, run_count, created_at, updated_at FROM circuits` + whereClause
	query += fmt.Sprintf(" ORDER BY created_at DESC LIMIT %d OFFSET %d", pageSize, offset)

	rows, err := s.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "query failed: %v", err)
	}
	defer rows.Close()

	var circuits []*pb.CircuitMetadata
	for rows.Next() {
		var m pb.CircuitMetadata
		var tagsJSON string
		var createdAt, updatedAt time.Time

		err := rows.Scan(
			&m.Id, &m.Name, &m.Description, &m.Author, &m.Domain, &tagsJSON,
			&m.NumQubits, &m.NumOperations, &m.Version, &m.IsPublic,
			&m.ForkCount, &m.RunCount, &createdAt, &updatedAt,
		)
		if err != nil {
			continue
		}

		json.Unmarshal([]byte(tagsJSON), &m.Tags)
		m.CreatedAt = createdAt.Unix()
		m.UpdatedAt = updatedAt.Unix()
		circuits = append(circuits, &m)
	}

	return &pb.CircuitList{
		Circuits:   circuits,
		TotalCount: totalCount,
		Page:       int32(page),
		PageSize:   int32(pageSize),
	}, nil
}

// ForkCircuit creates a copy of an existing circuit
func (s *RegistryServer) ForkCircuit(ctx context.Context, req *pb.ForkCircuitRequest) (*pb.CircuitMetadata, error) {
	// Load original
	original, err := s.LoadCircuit(ctx, &pb.LoadCircuitRequest{CircuitId: req.SourceCircuitId, Version: 0})
	if err != nil {
		return nil, err
	}

	// Save as new
	newMeta, err := s.SaveCircuit(ctx, &pb.SaveCircuitRequest{
		Name:        req.NewName,
		Description: fmt.Sprintf("Forked from %s", req.SourceCircuitId),
		Circuit:     original,
		Domain:      "general",
		IsPublic:    true,
	})
	if err != nil {
		return nil, err
	}

	// Increment fork count on original
	s.db.ExecContext(ctx, `UPDATE circuits SET fork_count = fork_count + 1 WHERE id = $1`, req.SourceCircuitId)

	return newMeta, nil
}

// DeleteCircuit removes a circuit from the registry
func (s *RegistryServer) DeleteCircuit(ctx context.Context, req *pb.DeleteCircuitRequest) (*pb.RegistryEmpty, error) {
	result, err := s.db.ExecContext(ctx, `DELETE FROM circuits WHERE id = $1`, req.CircuitId)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "delete failed: %v", err)
	}

	rows, _ := result.RowsAffected()
	if rows == 0 {
		return nil, status.Errorf(codes.NotFound, "circuit not found")
	}

	return &pb.RegistryEmpty{}, nil
}

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
			semconv.ServiceNameKey.String("qubit-registry"),
		)),
	)
	otel.SetTracerProvider(tp)
	otel.SetTextMapPropagator(propagation.NewCompositeTextMapPropagator(propagation.TraceContext{}, propagation.Baggage{}))
	return tp, nil
}

func getEnvOrDefault(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

func getEnvIntOrDefault(key string, fallback int) int {
	if v := os.Getenv(key); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			return n
		}
	}
	return fallback
}

func main() {
	dbHost := flag.String("db-host", getEnvOrDefault("DB_HOST", "localhost"), "PostgreSQL host")
	dbPort := flag.Int("db-port", getEnvIntOrDefault("DB_PORT", 5432), "PostgreSQL port")
	dbUser := flag.String("db-user", getEnvOrDefault("DB_USER", "qubit"), "PostgreSQL user")
	dbPass := flag.String("db-pass", getEnvOrDefault("DB_PASS", ""), "PostgreSQL password")
	dbName := flag.String("db-name", getEnvOrDefault("DB_NAME", "quantumcloud"), "PostgreSQL database")
	grpcPort := flag.Int("port", getEnvIntOrDefault("GRPC_PORT", 50052), "gRPC port")
	flag.Parse()

	// Initialize OpenTelemetry
	tp, err := initTracer()
	if err != nil {
		slog.Error("Failed to initialize tracer", "error", err)
	} else {
		defer tp.Shutdown(context.Background())
		slog.Info("OpenTelemetry tracer initialized")
	}

	// Initialize structured logger
	logger := slog.New(slog.NewJSONHandler(os.Stdout, nil))
	slog.SetDefault(logger)

	// Connect to PostgreSQL
	connStr := fmt.Sprintf("host=%s port=%d user=%s password=%s dbname=%s sslmode=disable",
		*dbHost, *dbPort, *dbUser, *dbPass, *dbName)

	db, err := sql.Open("postgres", connStr)
	if err != nil {
		slog.Error("Failed to connect to database", "error", err)
		os.Exit(1)
	}
	defer db.Close()

	if err := db.Ping(); err != nil {
		slog.Error("Database ping failed", "error", err)
		os.Exit(1)
	}

	// Initialize schema
	if err := InitDB(db); err != nil {
		slog.Error("Failed to initialize database", "error", err)
		os.Exit(1)
	}
	slog.Info("Database initialized successfully")

	// Start gRPC server
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *grpcPort))
	if err != nil {
		slog.Error("Failed to listen", "error", err)
		os.Exit(1)
	}

	server := grpc.NewServer(
		grpc.StatsHandler(otelgrpc.NewServerHandler()),
	)
	pb.RegisterCircuitRegistryServer(server, NewRegistryServer(db))

	// Register health check service
	healthServer := health.NewServer()
	grpc_health_v1.RegisterHealthServer(server, healthServer)
	healthServer.SetServingStatus("", grpc_health_v1.HealthCheckResponse_SERVING)

	slog.Info("Circuit Registry starting", "port", *grpcPort)

	// Graceful shutdown on SIGINT/SIGTERM
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, os.Interrupt, syscall.SIGTERM)

	go func() {
		if err := server.Serve(lis); err != nil {
			slog.Error("Failed to serve", "error", err)
			os.Exit(1)
		}
	}()

	sig := <-quit
	slog.Info("Shutdown signal received", "signal", sig.String())
	server.GracefulStop()
	db.Close()
	slog.Info("Registry shut down gracefully")
}
