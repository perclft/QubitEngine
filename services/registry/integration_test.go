package main

import (
	"context"
	"encoding/json"
	"testing"

	"github.com/DATA-DOG/go-sqlmock"
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

	req := &SaveCircuitRequest{
		Name:        "Test Circuit",
		Description: "A test circuit",
		Domain:      "general",
		IsPublic:    true,
		Circuit: &CircuitRequest{
			NumQubits: 2,
			Operations: []GateOperation{
				{Type: 1, TargetQubit: 0},
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
	mockCircuit := CircuitRequest{
		NumQubits: 2,
		Operations: []GateOperation{
			{Type: 1, TargetQubit: 0},
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

	req := &LoadCircuitRequest{
		CircuitId: circuitId,
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
