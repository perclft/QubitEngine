package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"log/slog"
	"net"
	"net/http"
	"os"
	"sync"
	"time"

	"github.com/go-redis/redis/v8"
	"github.com/google/uuid"
	pb "github.com/perclft/QubitEngine/api/generated"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
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
	pb.UnimplementedQuantumSchedulerServer
	rdb          *redis.Client
	engineAddr   string
	mu           sync.RWMutex
	workerCancel map[string]context.CancelFunc
	engineConn   *grpc.ClientConn
	engineClient pb.QuantumComputeClient
	workerCount  int
}

type ComplexNumber struct {
	Real float64 `json:"real"`
	Imag float64 `json:"imag"`
}

func NewSchedulerServer(rdb *redis.Client, engineAddr string) *SchedulerServer {
	return &SchedulerServer{
		rdb:          rdb,
		engineAddr:   engineAddr,
		workerCancel: make(map[string]context.CancelFunc),
		workerCount:  1000, // Bounded execution
	}
}

func (s *SchedulerServer) ConnectEngine(ctx context.Context) error {
	conn, err := grpc.Dial(s.engineAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return fmt.Errorf("failed to connect to engine: %w", err)
	}
	s.engineConn = conn
	s.engineClient = pb.NewQuantumComputeClient(conn)
	return nil
}

func (s *SchedulerServer) StartWorkers(ctx context.Context) {
	// Engine decoupling: C++ workers will pull directly from Redis "queue:jobs"
	// The Go scheduler only handles incoming streams and job requests.
	log.Println("Go Scheduler workers disabled to prevent bounded starvation. Awaiting C++ nodes to pull jobs.")
}

// ------------------------------------------------------------------
// SubmitJob - Add job to Redis queue
// ------------------------------------------------------------------

func (s *SchedulerServer) SubmitJob(ctx context.Context, req *pb.JobRequest) (*pb.JobHandle, error) {
	jobID := uuid.New().String()
	now := time.Now().Unix()

	job := &Job{
		ID:          jobID,
		UserID:      req.UserId,
		Priority:    JobPriority(req.Priority),
		State:       StateQueued,
		Shots:       req.Shots,
		CallbackURL: req.CallbackUrl,
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

	// Signal handled by background workers instead of spawning eagerly

	return &pb.JobHandle{
		JobId:                jobID,
		SubmittedAt:          now,
		EstimatedWaitSeconds: estimatedWait,
	}, nil
}

// ------------------------------------------------------------------
// GetJobStatus - Retrieve job status from Redis
// ------------------------------------------------------------------

func (s *SchedulerServer) GetJobStatus(ctx context.Context, handle *pb.JobHandle) (*pb.JobStatus, error) {
	jobBytes, err := s.rdb.Get(ctx, "job:"+handle.JobId).Bytes()
	if err == redis.Nil {
		return nil, status.Errorf(codes.NotFound, "job not found: %s", handle.JobId)
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
		rank, err := s.rdb.ZRank(ctx, "queue:jobs", handle.JobId).Result()
		if err == nil {
			position = int32(rank) + 1
		}
	}

	return &pb.JobStatus{
		JobId:           job.ID,
		State:           pb.JobState(job.State),
		PositionInQueue: position,
		WorkerId:        job.WorkerID,
		StartedAt:       job.StartedAt,
		CompletedAt:     job.CompletedAt,
		ErrorMessage:    job.ErrorMessage,
	}, nil
}

// ------------------------------------------------------------------
// CancelJob - Remove from queue or stop running job
// ------------------------------------------------------------------

func (s *SchedulerServer) CancelJob(ctx context.Context, handle *pb.JobHandle) (*pb.CancelResponse, error) {
	// Try to remove from queue
	removed, _ := s.rdb.ZRem(ctx, "queue:jobs", handle.JobId).Result()
	if removed > 0 {
		s.updateJobState(ctx, handle.JobId, StateCancelled, "")
		return &pb.CancelResponse{Success: true, Message: "Job cancelled from queue"}, nil
	}

	// Try to cancel running job
	s.mu.RLock()
	cancel, exists := s.workerCancel[handle.JobId]
	s.mu.RUnlock()

	if exists {
		cancel()
		s.updateJobState(ctx, handle.JobId, StateCancelled, "")
		return &pb.CancelResponse{Success: true, Message: "Running job cancelled"}, nil
	}

	return &pb.CancelResponse{Success: false, Message: "Job not found or already completed"}, nil
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
// ListJobs - List jobs for a user
// ------------------------------------------------------------------

func (s *SchedulerServer) ListJobs(ctx context.Context, req *pb.ListJobsRequest) (*pb.JobList, error) {
	// Get all job IDs for user (we'd normally have a user index, simplified here)
	pattern := "job:*"
	keys, err := s.rdb.Keys(ctx, pattern).Result()
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to list jobs: %v", err)
	}

	var jobs []*pb.JobStatus
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
		if req.UserId != "" && job.UserID != req.UserId {
			continue
		}

		// Filter by state if specified
		if req.StateFilter != pb.JobState_STATE_UNKNOWN && pb.JobState(job.State) != req.StateFilter {
			continue
		}

		jobs = append(jobs, &pb.JobStatus{
			JobId:        job.ID,
			State:        pb.JobState(job.State),
			WorkerId:     job.WorkerID,
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

	return &pb.JobList{
		Jobs:       jobs[start:end],
		TotalCount: int32(len(jobs)),
	}, nil
}

func (s *SchedulerServer) StreamJobResults(handle *pb.JobHandle, stream pb.QuantumScheduler_StreamJobResultsServer) error {
	jobID := handle.JobId

	// Verify job exists
	exists, err := s.rdb.Exists(stream.Context(), "job:"+jobID).Result()
	if err != nil || exists == 0 {
		return status.Errorf(codes.NotFound, "job not found: %s", jobID)
	}

	log.Printf("📡 Client streaming results for job: %s", jobID)
	redisKey := "stream:results:" + jobID
	lastID := "0" // Track stream offset

	for {
		select {
		case <-stream.Context().Done():
			slog.Info("Client disconnected from stream", "job_id", jobID)
			return stream.Context().Err()
		default:
		}

		// XRead Block offers true zero-latency backpressure without sleep-loops
		streams, err := s.rdb.XRead(stream.Context(), &redis.XReadArgs{
			Streams: []string{redisKey, lastID},
			Block:   2 * time.Second,
			Count:   1,
		}).Result()
		if err == redis.Nil || len(streams) == 0 {
			continue // try again
		}
		if err != nil {
			return status.Errorf(codes.Internal, "redis stream error: %v", err)
		}

		msg := streams[0].Messages[0]
		lastID = msg.ID

		data, ok := msg.Values["data"].(string)
		if !ok {
			continue
		}

		if data == "EOF" {
			// Job finished
			return nil
		}

		var resp pb.JobResult
		if err := json.Unmarshal([]byte(data), &resp); err != nil {
			slog.Warn("Failed to parse result from redis", "error", err)
			continue
		}

		if err := stream.Send(&resp); err != nil {
			slog.Warn("Stream send error", "job_id", jobID, "error", err)
			return err
		}
	}
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------

func main() {
	redisAddr := flag.String("redis-addr", "localhost:6379", "Redis address")
	engineAddr := flag.String("engine-addr", "engine:50051", "Engine gRPC address")
	port := flag.Int("port", 50053, "gRPC port")
	flag.Parse()

	// Initialize structured logger
	logger := slog.New(slog.NewJSONHandler(os.Stdout, nil))
	slog.SetDefault(logger)

	// Connect to Redis
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

	// Create server
	server := NewSchedulerServer(rdb, *engineAddr)
	if err := server.ConnectEngine(ctx); err != nil {
		slog.Error("Failed to connect to engine", "error", err)
		os.Exit(1)
	}
	server.StartWorkers(ctx)

	// ---------------------------------------------
	// Prometheus Metrics Scraper Endpoint
	// ---------------------------------------------
	http.Handle("/metrics", promhttp.Handler())
	go func() {
		slog.Info("Starting Prometheus metrics server on :2112/metrics")
		if err := http.ListenAndServe(":2112", nil); err != nil {
			slog.Error("Metrics server failed", "error", err)
		}
	}()

	// Background ticker to update Redis Gauges
	go func() {
		for {
			depth, err := rdb.ZCard(ctx, "queue:jobs").Result()
			if err == nil {
				queueDepthMetric.Set(float64(depth))
			}
			time.Sleep(5 * time.Second)
		}
	}()

	// Start gRPC server
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		slog.Error("Failed to listen", "error", err)
		os.Exit(1)
	}

	grpcServer := grpc.NewServer()
	pb.RegisterQuantumSchedulerServer(grpcServer, server)

	slog.Info("Quantum Scheduler starting",
		"port", *port,
		"redis", *redisAddr,
		"engine", *engineAddr,
	)

	if err := grpcServer.Serve(lis); err != nil {
		slog.Error("Failed to serve", "error", err)
		os.Exit(1)
	}
}
