package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net"
	"sync"
	"time"

	"github.com/go-redis/redis/v8"
	"github.com/google/uuid"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
)

// ------------------------------------------------------------------
// Job Representation
// ------------------------------------------------------------------

type JobPriority int32

const (
	PriorityLow      JobPriority = 0
	PriorityNormal   JobPriority = 1
	PriorityHigh     JobPriority = 2
	PriorityRealtime JobPriority = 3
)

type JobState int32

const (
	StateUnknown   JobState = 0
	StateQueued    JobState = 1
	StateRunning   JobState = 2
	StateCompleted JobState = 3
	StateFailed    JobState = 4
	StateCancelled JobState = 5
)

type Job struct {
	ID           string            `json:"id"`
	UserID       string            `json:"user_id"`
	Priority     JobPriority       `json:"priority"`
	State        JobState          `json:"state"`
	NumQubits    int32             `json:"num_qubits"`
	NumOps       int32             `json:"num_ops"`
	Shots        int32             `json:"shots"`
	CallbackURL  string            `json:"callback_url"`
	Metadata     map[string]string `json:"metadata"`
	CircuitJSON  string            `json:"circuit_json"`
	WorkerID     string            `json:"worker_id"`
	SubmittedAt  int64             `json:"submitted_at"`
	StartedAt    int64             `json:"started_at"`
	CompletedAt  int64             `json:"completed_at"`
	ErrorMessage string            `json:"error_message"`
	Position     int32             `json:"position"`
}

// ------------------------------------------------------------------
// Scheduler Server
// ------------------------------------------------------------------

type SchedulerServer struct {
	rdb          *redis.Client
	engineAddr   string
	mu           sync.RWMutex
	jobResults   map[string]chan *JobResult
	workerCancel map[string]context.CancelFunc
}

type JobResult struct {
	JobID        string
	ShotNumber   int32
	StateVector  []ComplexNumber
	Measurements map[int32]bool
}

type ComplexNumber struct {
	Real float64 `json:"real"`
	Imag float64 `json:"imag"`
}

func NewSchedulerServer(rdb *redis.Client, engineAddr string) *SchedulerServer {
	return &SchedulerServer{
		rdb:          rdb,
		engineAddr:   engineAddr,
		jobResults:   make(map[string]chan *JobResult),
		workerCancel: make(map[string]context.CancelFunc),
	}
}

// ------------------------------------------------------------------
// SubmitJob - Add job to Redis queue
// ------------------------------------------------------------------

func (s *SchedulerServer) SubmitJob(ctx context.Context, req *JobRequest) (*JobHandle, error) {
	jobID := uuid.New().String()
	now := time.Now().Unix()

	job := &Job{
		ID:          jobID,
		UserID:      req.UserID,
		Priority:    JobPriority(req.Priority),
		State:       StateQueued,
		Shots:       req.Shots,
		CallbackURL: req.CallbackURL,
		Metadata:    req.Metadata,
		SubmittedAt: now,
	}

	// Serialize circuit
	if req.Circuit != nil {
		job.NumQubits = req.Circuit.NumQubits
		job.NumOps = int32(len(req.Circuit.Operations))
		circuitBytes, _ := json.Marshal(req.Circuit)
		job.CircuitJSON = string(circuitBytes)
	}

	// Store job metadata
	jobBytes, _ := json.Marshal(job)
	if err := s.rdb.Set(ctx, "job:"+jobID, jobBytes, 24*time.Hour).Err(); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to store job: %v", err)
	}

	// Add to priority queue (sorted set with score = priority * 1000000 + timestamp)
	score := float64(int64(job.Priority)*1000000 - now)
	if err := s.rdb.ZAdd(ctx, "queue:jobs", &redis.Z{
		Score:  score,
		Member: jobID,
	}).Err(); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to queue job: %v", err)
	}

	// Estimate wait time based on queue position
	queueLen, _ := s.rdb.ZCard(ctx, "queue:jobs").Result()
	estimatedWait := int32(queueLen) * 2 // 2 seconds per job estimate

	log.Printf("📥 Job submitted: %s (qubits=%d, ops=%d, priority=%d)",
		jobID, job.NumQubits, job.NumOps, job.Priority)

	// Start a background worker to process jobs
	go s.processNextJob()

	return &JobHandle{
		JobID:                jobID,
		SubmittedAt:          now,
		EstimatedWaitSeconds: estimatedWait,
	}, nil
}

// ------------------------------------------------------------------
// GetJobStatus - Retrieve job status from Redis
// ------------------------------------------------------------------

func (s *SchedulerServer) GetJobStatus(ctx context.Context, handle *JobHandle) (*JobStatus, error) {
	jobBytes, err := s.rdb.Get(ctx, "job:"+handle.JobID).Bytes()
	if err == redis.Nil {
		return nil, status.Errorf(codes.NotFound, "job not found: %s", handle.JobID)
	}
	if err != nil {
		return nil, status.Errorf(codes.Internal, "redis error: %v", err)
	}

	var job Job
	if err := json.Unmarshal(jobBytes, &job); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to parse job: %v", err)
	}

	// Get queue position if still queued
	position := int32(0)
	if job.State == StateQueued {
		rank, err := s.rdb.ZRank(ctx, "queue:jobs", handle.JobID).Result()
		if err == nil {
			position = int32(rank) + 1
		}
	}

	return &JobStatus{
		JobID:           job.ID,
		State:           int32(job.State),
		PositionInQueue: position,
		WorkerID:        job.WorkerID,
		StartedAt:       job.StartedAt,
		CompletedAt:     job.CompletedAt,
		ErrorMessage:    job.ErrorMessage,
	}, nil
}

// ------------------------------------------------------------------
// CancelJob - Remove from queue or stop running job
// ------------------------------------------------------------------

func (s *SchedulerServer) CancelJob(ctx context.Context, handle *JobHandle) (*CancelResponse, error) {
	// Try to remove from queue
	removed, _ := s.rdb.ZRem(ctx, "queue:jobs", handle.JobID).Result()
	if removed > 0 {
		s.updateJobState(ctx, handle.JobID, StateCancelled, "")
		return &CancelResponse{Success: true, Message: "Job cancelled from queue"}, nil
	}

	// Try to cancel running job
	s.mu.RLock()
	cancel, exists := s.workerCancel[handle.JobID]
	s.mu.RUnlock()

	if exists {
		cancel()
		s.updateJobState(ctx, handle.JobID, StateCancelled, "")
		return &CancelResponse{Success: true, Message: "Running job cancelled"}, nil
	}

	return &CancelResponse{Success: false, Message: "Job not found or already completed"}, nil
}

// ------------------------------------------------------------------
// ListJobs - List jobs for a user
// ------------------------------------------------------------------

func (s *SchedulerServer) ListJobs(ctx context.Context, req *ListJobsRequest) (*JobList, error) {
	// Get all job IDs for user (we'd normally have a user index, simplified here)
	pattern := "job:*"
	keys, err := s.rdb.Keys(ctx, pattern).Result()
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to list jobs: %v", err)
	}

	var jobs []*JobStatus
	for _, key := range keys {
		jobBytes, err := s.rdb.Get(ctx, key).Bytes()
		if err != nil {
			continue
		}
		var job Job
		if err := json.Unmarshal(jobBytes, &job); err != nil {
			continue
		}

		// Filter by user if specified
		if req.UserID != "" && job.UserID != req.UserID {
			continue
		}

		// Filter by state if specified
		if req.StateFilter != 0 && int32(job.State) != req.StateFilter {
			continue
		}

		jobs = append(jobs, &JobStatus{
			JobID:        job.ID,
			State:        int32(job.State),
			WorkerID:     job.WorkerID,
			StartedAt:    job.StartedAt,
			CompletedAt:  job.CompletedAt,
			ErrorMessage: job.ErrorMessage,
		})
	}

	// Apply pagination
	start := int(req.Offset)
	end := start + int(req.Limit)
	if end > len(jobs) {
		end = len(jobs)
	}
	if start > len(jobs) {
		start = len(jobs)
	}

	return &JobList{
		Jobs:       jobs[start:end],
		TotalCount: int32(len(jobs)),
	}, nil
}

// ------------------------------------------------------------------
// Background Job Processor
// ------------------------------------------------------------------

func (s *SchedulerServer) processNextJob() {
	ctx := context.Background()

	// Pop highest priority job from queue
	result, err := s.rdb.ZPopMax(ctx, "queue:jobs", 1).Result()
	if err != nil || len(result) == 0 {
		return
	}

	jobID := result[0].Member.(string)

	// Get job details
	jobBytes, err := s.rdb.Get(ctx, "job:"+jobID).Bytes()
	if err != nil {
		log.Printf("❌ Failed to get job %s: %v", jobID, err)
		return
	}

	var job Job
	if err := json.Unmarshal(jobBytes, &job); err != nil {
		log.Printf("❌ Failed to parse job %s: %v", jobID, err)
		return
	}

	// Update state to running
	job.State = StateRunning
	job.StartedAt = time.Now().Unix()
	s.saveJob(ctx, &job)

	log.Printf("🚀 Processing job: %s (%d qubits, %d ops, %d shots)",
		jobID, job.NumQubits, job.NumOps, job.Shots)

	// Create cancellable context
	jobCtx, cancel := context.WithCancel(ctx)
	s.mu.Lock()
	s.workerCancel[jobID] = cancel
	s.mu.Unlock()

	defer func() {
		s.mu.Lock()
		delete(s.workerCancel, jobID)
		s.mu.Unlock()
	}()

	// Execute on engine (simplified - just marking complete)
	// In production, this would call the engine gRPC service
	err = s.executeOnEngine(jobCtx, &job)
	if err != nil {
		job.State = StateFailed
		job.ErrorMessage = err.Error()
	} else {
		job.State = StateCompleted
	}

	job.CompletedAt = time.Now().Unix()
	s.saveJob(ctx, &job)

	log.Printf("✅ Job completed: %s (state=%d)", jobID, job.State)

	// TODO: Call callback URL if specified
}

func (s *SchedulerServer) executeOnEngine(ctx context.Context, job *Job) error {
	// Connect to engine
	conn, err := grpc.Dial(s.engineAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return fmt.Errorf("failed to connect to engine: %w", err)
	}
	defer conn.Close()

	// For now, just simulate execution
	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-time.After(time.Duration(job.NumOps) * 100 * time.Millisecond):
		return nil
	}
}

func (s *SchedulerServer) updateJobState(ctx context.Context, jobID string, state JobState, errMsg string) {
	jobBytes, err := s.rdb.Get(ctx, "job:"+jobID).Bytes()
	if err != nil {
		return
	}
	var job Job
	if err := json.Unmarshal(jobBytes, &job); err != nil {
		return
	}
	job.State = state
	job.ErrorMessage = errMsg
	if state == StateCompleted || state == StateFailed || state == StateCancelled {
		job.CompletedAt = time.Now().Unix()
	}
	s.saveJob(ctx, &job)
}

func (s *SchedulerServer) saveJob(ctx context.Context, job *Job) {
	jobBytes, _ := json.Marshal(job)
	s.rdb.Set(ctx, "job:"+job.ID, jobBytes, 24*time.Hour)
}

// ------------------------------------------------------------------
// Placeholder types (would be generated from protobuf)
// ------------------------------------------------------------------

type JobRequest struct {
	Circuit     *CircuitRequest
	Priority    int32
	Shots       int32
	CallbackURL string
	UserID      string
	Metadata    map[string]string
}

type CircuitRequest struct {
	NumQubits  int32           `json:"num_qubits"`
	Operations []GateOperation `json:"operations"`
}

type GateOperation struct {
	Type        int32 `json:"type"`
	TargetQubit int32 `json:"target_qubit"`
}

type JobHandle struct {
	JobID                string
	SubmittedAt          int64
	EstimatedWaitSeconds int32
}

type JobStatus struct {
	JobID           string
	State           int32
	PositionInQueue int32
	ProgressPercent int32
	WorkerID        string
	StartedAt       int64
	CompletedAt     int64
	ErrorMessage    string
}

type CancelResponse struct {
	Success bool
	Message string
}

type ListJobsRequest struct {
	UserID      string
	StateFilter int32
	Limit       int32
	Offset      int32
}

type JobList struct {
	Jobs       []*JobStatus
	TotalCount int32
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------

func main() {
	redisAddr := flag.String("redis-addr", "localhost:6379", "Redis address")
	engineAddr := flag.String("engine-addr", "engine:50051", "Engine gRPC address")
	port := flag.Int("port", 50053, "gRPC port")
	flag.Parse()

	// Connect to Redis
	rdb := redis.NewClient(&redis.Options{
		Addr:     *redisAddr,
		Password: "",
		DB:       0,
	})

	ctx := context.Background()
	if err := rdb.Ping(ctx).Err(); err != nil {
		log.Fatalf("Failed to connect to Redis: %v", err)
	}
	log.Println("Connected to Redis")

	// Create server
	server := NewSchedulerServer(rdb, *engineAddr)

	// Start gRPC server
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	// RegisterQuantumSchedulerServer(grpcServer, server)

	log.Printf("📋 Quantum Scheduler starting on port %d", *port)
	log.Printf("   Redis: %s", *redisAddr)
	log.Printf("   Engine: %s", *engineAddr)

	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}

	_ = server // Silence unused variable warning
}
