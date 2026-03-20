package main

import (
	"context"
	"encoding/json"
	"testing"

	"github.com/DATA-DOG/go-sqlmock"
	api "github.com/perclft/QubitEngine/api/generated"
	pb "github.com/perclft/QubitEngine/services/registry/generated"
)

func TestSaveCircuit(t *testing.T) {
	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("an error '%s' was not expected when opening a stub database connection", err)
	}
	defer db.Close()

	server := NewRegistryServer(db)
	ctx := context.Background()

	// Prepare mock
	mock.ExpectExec("INSERT INTO circuits").
		WithArgs(sqlmock.AnyArg(), "Test Circuit", "A test circuit", "general", "null", 2, 1, sqlmock.AnyArg(), true, sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnResult(sqlmock.NewResult(1, 1))

	req := &pb.SaveCircuitRequest{
		Name:        "Test Circuit",
		Description: "A test circuit",
		Domain:      "general",
		IsPublic:    true,
		Circuit: &api.CircuitRequest{
			NumQubits: 2,
			Operations: []*api.GateOperation{
				{Type: api.GateOperation_PAULI_X, TargetQubit: 0},
			},
		},
	}

	meta, err := server.SaveCircuit(ctx, req)
	if err != nil {
		t.Errorf("expected no error, got %v", err)
	}
	if meta == nil {
		t.Fatal("expected metadata, got nil")
	}
	if meta.Name != "Test Circuit" {
		t.Errorf("expected name 'Test Circuit', got '%s'", meta.Name)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("there were unfulfilled expectations: %s", err)
	}
}

func TestLoadCircuit(t *testing.T) {
	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("an error '%s' was not expected when opening a stub database connection", err)
	}
	defer db.Close()

	server := NewRegistryServer(db)
	ctx := context.Background()

	circuitId := "test-uuid-1234"
	mockCircuit := api.CircuitRequest{
		NumQubits: 2,
		Operations: []*api.GateOperation{
			{Type: api.GateOperation_PAULI_X, TargetQubit: 0},
		},
	}
	circuitJSON, _ := json.Marshal(mockCircuit)

	// Mock SELECT
	rows := sqlmock.NewRows([]string{"circuit_json"}).
		AddRow(string(circuitJSON))

	mock.ExpectQuery("SELECT circuit_json FROM circuits WHERE id = \\$1").
		WithArgs(circuitId).
		WillReturnRows(rows)

	// Mock UPDATE run_count
	mock.ExpectExec("UPDATE circuits SET run_count = run_count \\+ 1 WHERE id = \\$1").
		WithArgs(circuitId).
		WillReturnResult(sqlmock.NewResult(1, 1))

	req := &pb.LoadCircuitRequest{
		CircuitId: circuitId,
		Version:   0,
	}

	loaded, err := server.LoadCircuit(ctx, req)
	if err != nil {
		t.Errorf("expected no error, got %v", err)
	}
	if loaded == nil {
		t.Fatal("expected loaded circuit, got nil")
	}
	if loaded.NumQubits != 2 {
		t.Errorf("expected 2 qubits, got %d", loaded.NumQubits)
	}
	if len(loaded.Operations) != 1 {
		t.Errorf("expected 1 operation, got %d", len(loaded.Operations))
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("there were unfulfilled expectations: %s", err)
	}
}

func TestListCircuits(t *testing.T) {
	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("an error '%s' was not expected when opening a stub database connection", err)
	}
	defer db.Close()

	server := NewRegistryServer(db)
	ctx := context.Background()

	// Mock COUNT(*) query for total records
	mock.ExpectQuery("SELECT COUNT\\(\\*\\) FROM circuits").
		WillReturnRows(sqlmock.NewRows([]string{"count"}).AddRow(10))

	// Mock SELECT query for a single page of results
	rows := sqlmock.NewRows([]string{"id", "name", "description", "author", "domain", "tags", "num_qubits", "num_operations", "version", "is_public", "fork_count", "run_count", "created_at", "updated_at"}).
		AddRow("id1", "Circuit 1", "Desc 1", "author1", "domain1", "[]", 2, 1, 1, true, 0, 0, time.Now(), time.Now()).
		AddRow("id2", "Circuit 2", "Desc 2", "author2", "domain2", "[]", 4, 2, 1, true, 0, 0, time.Now(), time.Now())

	mock.ExpectQuery("SELECT (.+) FROM circuits ORDER BY created_at DESC LIMIT 2 OFFSET 0").
		WillReturnRows(rows)

	req := &pb.ListJobsRequest{
		Limit:  2,
		Offset: 0,
	}

	res, err := server.ListCircuits(ctx, req)
	if err != nil {
		t.Errorf("expected no error, got %v", err)
	}
	if res == nil {
		t.Fatal("expected result list, got nil")
	}
	if res.TotalCount != 10 {
		t.Errorf("expected TotalCount 10, got %d", res.TotalCount)
	}
	if len(res.Circuits) != 2 {
		t.Errorf("expected 2 circuits in page, got %d", len(res.Circuits))
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("there were unfulfilled expectations: %s", err)
	}
}
