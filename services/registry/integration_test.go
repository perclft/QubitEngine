package main

import (
	"context"
	"database/sql"
	"encoding/json"
	"regexp"
	"testing"
	"time"

	"github.com/DATA-DOG/go-sqlmock"
	api "github.com/perclft/QubitEngine/api/generated"
	pb "github.com/perclft/QubitEngine/services/registry/generated"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

func TestSaveCircuit(t *testing.T) {
	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("an error '%s' was not expected when opening a stub database connection", err)
	}
	defer db.Close()

	server := NewRegistryServer(db)
	ctx := context.WithValue(context.Background(), ownerIDKey, "test-user-id")

	// Prepare mock
	mock.ExpectExec("INSERT INTO circuits").
		WithArgs(sqlmock.AnyArg(), "Test Circuit", "A test circuit", "test-user-id", "general", "null", 2, 1, sqlmock.AnyArg(), true, sqlmock.AnyArg(), sqlmock.AnyArg()).
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
	ctx := context.WithValue(context.Background(), ownerIDKey, "test-user-id")

	circuitId := "test-uuid-1234"
	mockCircuit := api.CircuitRequest{
		NumQubits: 2,
		Operations: []*api.GateOperation{
			{Type: api.GateOperation_PAULI_X, TargetQubit: 0},
		},
	}
	circuitJSON, _ := json.Marshal(mockCircuit)

	// Mock SELECT
	rows := sqlmock.NewRows([]string{"circuit_json", "is_public", "owner_id"}).
		AddRow(string(circuitJSON), true, "")

	mock.ExpectQuery("SELECT circuit_json, is_public, owner_id FROM circuits WHERE id = \\$1").
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
	ctx := context.WithValue(context.Background(), ownerIDKey, "test-user-id")

	// Mock COUNT(*) query for total records
	mock.ExpectQuery(regexp.QuoteMeta("SELECT COUNT(*) FROM circuits WHERE (is_public = true OR owner_id = $1)")).
		WithArgs("test-user-id").
		WillReturnRows(sqlmock.NewRows([]string{"count"}).AddRow(10))

	// Mock SELECT query for a single page of results
	rows := sqlmock.NewRows([]string{"id", "name", "description", "author", "domain", "tags", "num_qubits", "num_operations", "version", "is_public", "fork_count", "run_count", "created_at", "updated_at"}).
		AddRow("id1", "Circuit 1", "Desc 1", "author1", "domain1", "[]", 2, 1, 1, true, 0, 0, time.Now(), time.Now()).
		AddRow("id2", "Circuit 2", "Desc 2", "author2", "domain2", "[]", 4, 2, 1, true, 0, 0, time.Now(), time.Now())

	mock.ExpectQuery(regexp.QuoteMeta("SELECT id, name, description, author, domain, tags, num_qubits, num_operations, version, is_public, fork_count, run_count, created_at, updated_at FROM circuits WHERE (is_public = true OR owner_id = $1)") + "(.+)").
		WithArgs("test-user-id").
		WillReturnRows(rows)

	req := &pb.ListCircuitsRequest{
		PageSize: 2,
		Page:     0,
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

func TestDeleteCircuit(t *testing.T) {
	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("an error '%s' was not expected when opening a stub database connection", err)
	}
	defer db.Close()

	server := NewRegistryServer(db)
	circuitId := "test-uuid-1234"

	// 1. Unauthenticated request
	{
		req := &pb.DeleteCircuitRequest{CircuitId: circuitId}
		_, err := server.DeleteCircuit(context.Background(), req)
		if err == nil {
			t.Error("expected error for unauthenticated delete, got nil")
		} else {
			st, ok := status.FromError(err)
			if !ok || st.Code() != codes.Unauthenticated {
				t.Errorf("expected Unauthenticated error, got %v", err)
			}
		}
	}

	// 2. Owner ID matches (successful delete)
	{
		ctx := context.WithValue(context.Background(), ownerIDKey, "owner-user")
		mock.ExpectQuery("SELECT owner_id FROM circuits WHERE id = \\$1").
			WithArgs(circuitId).
			WillReturnRows(sqlmock.NewRows([]string{"owner_id"}).AddRow("owner-user"))

		mock.ExpectExec("DELETE FROM circuits WHERE id = \\$1").
			WithArgs(circuitId).
			WillReturnResult(sqlmock.NewResult(1, 1))

		req := &pb.DeleteCircuitRequest{CircuitId: circuitId}
		_, err := server.DeleteCircuit(ctx, req)
		if err != nil {
			t.Errorf("expected no error for owner delete, got %v", err)
		}
	}

	// 3. Owner ID mismatch (permission denied)
	{
		ctx := context.WithValue(context.Background(), ownerIDKey, "other-user")
		mock.ExpectQuery("SELECT owner_id FROM circuits WHERE id = \\$1").
			WithArgs(circuitId).
			WillReturnRows(sqlmock.NewRows([]string{"owner_id"}).AddRow("owner-user"))

		req := &pb.DeleteCircuitRequest{CircuitId: circuitId}
		_, err := server.DeleteCircuit(ctx, req)
		if err == nil {
			t.Error("expected error for unauthorized delete, got nil")
		} else {
			st, ok := status.FromError(err)
			if !ok || st.Code() != codes.PermissionDenied {
				t.Errorf("expected PermissionDenied error, got %v", err)
			}
		}
	}

	// 4. Circuit not found
	{
		ctx := context.WithValue(context.Background(), ownerIDKey, "owner-user")
		mock.ExpectQuery("SELECT owner_id FROM circuits WHERE id = \\$1").
			WithArgs(circuitId).
			WillReturnError(sql.ErrNoRows)

		req := &pb.DeleteCircuitRequest{CircuitId: circuitId}
		_, err := server.DeleteCircuit(ctx, req)
		if err == nil {
			t.Error("expected error for non-existent circuit delete, got nil")
		} else {
			st, ok := status.FromError(err)
			if !ok || st.Code() != codes.NotFound {
				t.Errorf("expected NotFound error, got %v", err)
			}
		}
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("there were unfulfilled expectations: %s", err)
	}
}

func TestForkCircuit_RollbackOnSecondStepFailure(t *testing.T) {
	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("an error '%s' was not expected when opening a stub database connection", err)
	}
	defer db.Close()

	server := NewRegistryServer(db)
	ctx := context.WithValue(context.Background(), ownerIDKey, "test-user-id")
	sourceID := "source-circuit-123"

	mockCircuit := api.CircuitRequest{
		NumQubits: 2,
		Operations: []*api.GateOperation{
			{Type: api.GateOperation_PAULI_X, TargetQubit: 0},
		},
	}
	circuitJSON, _ := json.Marshal(mockCircuit)

	// 1. Mock LoadCircuit (SELECT + UPDATE run_count)
	mock.ExpectQuery("SELECT circuit_json, is_public, owner_id FROM circuits WHERE id = \\$1").
		WithArgs(sourceID).
		WillReturnRows(sqlmock.NewRows([]string{"circuit_json", "is_public", "owner_id"}).AddRow(string(circuitJSON), true, "test-user-id"))

	mock.ExpectExec("UPDATE circuits SET run_count = run_count \\+ 1 WHERE id = \\$1").
		WithArgs(sourceID).
		WillReturnResult(sqlmock.NewResult(1, 1))

	// 2. Mock Begin Transaction
	mock.ExpectBegin()

	// 3. Mock Step 1: INSERT new circuit (Succeeds)
	mock.ExpectExec("INSERT INTO circuits").
		WithArgs(sqlmock.AnyArg(), "Forked Name", regexp.QuoteMeta("Forked from "+sourceID), "test-user-id", "general", "null", 2, 1, sqlmock.AnyArg(), true, sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnResult(sqlmock.NewResult(1, 1))

	// 4. Mock Step 2: UPDATE fork_count (FAILS deliberately!)
	mock.ExpectExec("UPDATE circuits SET fork_count = fork_count \\+ 1 WHERE id = \\$1").
		WithArgs(sourceID).
		WillReturnError(sql.ErrConnDone) // Simulating DB connection / query failure

	// 5. Mock Rollback (Transaction MUST roll back!)
	mock.ExpectRollback()

	req := &pb.ForkCircuitRequest{
		SourceCircuitId: sourceID,
		NewName:         "Forked Name",
	}

	meta, err := server.ForkCircuit(ctx, req)

	// Assertions
	if err == nil {
		t.Fatalf("expected error from failed second step, got success meta: %v", meta)
	}
	if meta != nil {
		t.Errorf("expected nil metadata on transaction failure, got: %v", meta)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Errorf("unfulfilled expectations (rollback was not executed as expected): %s", err)
	}
}

