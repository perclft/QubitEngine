package main

import (
	"context"
	"database/sql"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net"
	"time"

	"github.com/google/uuid"
	_ "github.com/lib/pq"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
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
func (s *RegistryServer) SaveCircuit(ctx context.Context, req *SaveCircuitRequest) (*CircuitMetadata, error) {
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

	return &CircuitMetadata{
		Id:            id,
		Name:          req.Name,
		Description:   req.Description,
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
func (s *RegistryServer) LoadCircuit(ctx context.Context, req *LoadCircuitRequest) (*CircuitRequest, error) {
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

	var circuit CircuitRequest
	if err := json.Unmarshal([]byte(circuitJSON), &circuit); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to deserialize circuit: %v", err)
	}

	return &circuit, nil
}

// ListCircuits returns circuits matching the given filters
func (s *RegistryServer) ListCircuits(ctx context.Context, req *ListCircuitsRequest) (*CircuitList, error) {
	query := `SELECT id, name, description, author, domain, tags, num_qubits, num_operations, version, is_public, fork_count, run_count, created_at, updated_at FROM circuits WHERE 1=1`
	args := []interface{}{}
	argIdx := 1

	if req.Domain != "" {
		query += fmt.Sprintf(" AND domain = $%d", argIdx)
		args = append(args, req.Domain)
		argIdx++
	}
	if req.Author != "" {
		query += fmt.Sprintf(" AND author = $%d", argIdx)
		args = append(args, req.Author)
		argIdx++
	}
	if req.PublicOnly {
		query += " AND is_public = true"
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

	query += fmt.Sprintf(" ORDER BY created_at DESC LIMIT %d OFFSET %d", pageSize, offset)

	rows, err := s.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "query failed: %v", err)
	}
	defer rows.Close()

	var circuits []*CircuitMetadata
	for rows.Next() {
		var m CircuitMetadata
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

	return &CircuitList{
		Circuits: circuits,
		Page:     int32(page),
		PageSize: int32(pageSize),
	}, nil
}

// ForkCircuit creates a copy of an existing circuit
func (s *RegistryServer) ForkCircuit(ctx context.Context, req *ForkCircuitRequest) (*CircuitMetadata, error) {
	// Load original
	original, err := s.LoadCircuit(ctx, &LoadCircuitRequest{CircuitId: req.SourceCircuitId})
	if err != nil {
		return nil, err
	}

	// Save as new
	newMeta, err := s.SaveCircuit(ctx, &SaveCircuitRequest{
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
func (s *RegistryServer) DeleteCircuit(ctx context.Context, req *DeleteCircuitRequest) (*Empty, error) {
	result, err := s.db.ExecContext(ctx, `DELETE FROM circuits WHERE id = $1`, req.CircuitId)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "delete failed: %v", err)
	}

	rows, _ := result.RowsAffected()
	if rows == 0 {
		return nil, status.Errorf(codes.NotFound, "circuit not found")
	}

	return &Empty{}, nil
}

// Placeholder types - these would be generated from protobuf
type SaveCircuitRequest struct {
	Name        string
	Description string
	Circuit     *CircuitRequest
	Domain      string
	Tags        []string
	IsPublic    bool
}

type LoadCircuitRequest struct {
	CircuitId string
	Version   int32
}

type ListCircuitsRequest struct {
	Domain     string
	Tags       []string
	Author     string
	PublicOnly bool
	Page       int32
	PageSize   int32
}

type ForkCircuitRequest struct {
	SourceCircuitId string
	NewName         string
}

type DeleteCircuitRequest struct {
	CircuitId string
}

type CircuitMetadata struct {
	Id            string
	Name          string
	Description   string
	Author        string
	Domain        string
	Tags          []string
	NumQubits     int32
	NumOperations int32
	Version       int32
	CreatedAt     int64
	UpdatedAt     int64
	IsPublic      bool
	ForkCount     int32
	RunCount      int32
}

type CircuitList struct {
	Circuits   []*CircuitMetadata
	TotalCount int32
	Page       int32
	PageSize   int32
}

type CircuitRequest struct {
	NumQubits        int32           `json:"num_qubits"`
	Operations       []GateOperation `json:"operations"`
	NoiseProbability float64         `json:"noise_probability"`
}

type GateOperation struct {
	Type         int32   `json:"type"`
	TargetQubit  uint32  `json:"target_qubit"`
	ControlQubit uint32  `json:"control_qubit"`
	Angle        float64 `json:"angle"`
}

type Empty struct{}

func main() {
	dbHost := flag.String("db-host", "localhost", "PostgreSQL host")
	dbPort := flag.Int("db-port", 5432, "PostgreSQL port")
	dbUser := flag.String("db-user", "qubit", "PostgreSQL user")
	dbPass := flag.String("db-pass", "quantum", "PostgreSQL password")
	dbName := flag.String("db-name", "quantumcloud", "PostgreSQL database")
	grpcPort := flag.Int("port", 50052, "gRPC port")
	flag.Parse()

	// Connect to PostgreSQL
	connStr := fmt.Sprintf("host=%s port=%d user=%s password=%s dbname=%s sslmode=disable",
		*dbHost, *dbPort, *dbUser, *dbPass, *dbName)

	db, err := sql.Open("postgres", connStr)
	if err != nil {
		log.Fatalf("Failed to connect to database: %v", err)
	}
	defer db.Close()

	if err := db.Ping(); err != nil {
		log.Fatalf("Database ping failed: %v", err)
	}

	// Initialize schema
	if err := InitDB(db); err != nil {
		log.Fatalf("Failed to initialize database: %v", err)
	}
	log.Println("Database initialized successfully")

	// Start gRPC server
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *grpcPort))
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	server := grpc.NewServer()
	// RegisterCircuitRegistryServer(server, NewRegistryServer(db))

	log.Printf("🗄️ Circuit Registry starting on port %d", *grpcPort)
	if err := server.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}
}
