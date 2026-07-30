package grpc

import (
	"context"
	"crypto/tls"
	"encoding/json"
	"fmt"
	"log/slog"
	"os"
	"runtime"
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/go-redis/redis/v8"
	"github.com/google/uuid"
	pb "github.com/perclft/QubitEngine/api/generated"
	"github.com/perclft/QubitEngine/services/scheduler/pkg/models"
	"github.com/sony/gobreaker"
	google_grpc "google.golang.org/grpc"
	_ "google.golang.org/grpc/balancer/roundrobin"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/emptypb"
)

type SchedulerServer struct {
	pb.UnimplementedQuantumSchedulerServer
	rdb          *redis.Client
	engineAddr   string
	mu           sync.RWMutex
	workerCancel map[string]context.CancelFunc
	engineConn   *google_grpc.ClientConn
	engineClient pb.QuantumComputeClient
	workerCount  int
	limiters     sync.Map // map[string]*userLimiter
	cb           *gobreaker.CircuitBreaker
	engineToken  string
}

type tokenAuth struct {
	token string
}

func (t tokenAuth) GetRequestMetadata(ctx context.Context, uri ...string) (map[string]string, error) {
	return map[string]string{
		"authorization": "Bearer " + t.token,
	}, nil
}

func (t tokenAuth) RequireTransportSecurity() bool {
	return false
}

type userLimiter struct {
	tokens float64
	last   time.Time
	mu     sync.Mutex
}

type limiterEntry struct {
	key  string
	last time.Time
}

const (
	maxQubits          = 32
	maxOperations      = 10000
	refillRate         = 0.1 // tokens per second (1 every 10s)
	burstSize          = 5.0
	MaxLimiterEntries  = 1000
	limiterInactivity  = 5 * time.Minute
)

func (s *SchedulerServer) getOrCreateLimiter(userID string) *userLimiter {
	now := time.Now()
	lim, _ := s.limiters.LoadOrStore(userID, &userLimiter{tokens: burstSize, last: now})
	return lim.(*userLimiter)
}

func (s *SchedulerServer) StartLimiterCleaner(ctx context.Context) {
	ticker := time.NewTicker(1 * time.Minute)
	go func() {
		for {
			select {
			case <-ctx.Done():
				ticker.Stop()
				return
			case <-ticker.C:
				s.cleanupLimiters()
			}
		}
	}()
}

func (s *SchedulerServer) cleanupLimiters() {
	now := time.Now()
	var entries []limiterEntry

	s.limiters.Range(func(key, value interface{}) bool {
		k := key.(string)
		l := value.(*userLimiter)
		l.mu.Lock()
		lastAccess := l.last
		l.mu.Unlock()

		if now.Sub(lastAccess) > limiterInactivity {
			s.limiters.Delete(k)
		} else {
			entries = append(entries, limiterEntry{key: k, last: lastAccess})
		}
		return true
	})

	if len(entries) > MaxLimiterEntries {
		sort.Slice(entries, func(i, j int) bool {
			return entries[i].last.Before(entries[j].last)
		})
		toEvict := len(entries) - MaxLimiterEntries
		for i := 0; i < toEvict; i++ {
			s.limiters.Delete(entries[i].key)
		}
	}
}

func NewSchedulerServer(rdb *redis.Client, engineAddr string, engineToken string) *SchedulerServer {
	cb := gobreaker.NewCircuitBreaker(gobreaker.Settings{
		Name:        "Redis",
		MaxRequests: 5,
		Interval:    10 * time.Second,
		Timeout:     5 * time.Second,
	})
	return &SchedulerServer{
		rdb:          rdb,
		engineAddr:   engineAddr,
		workerCancel: make(map[string]context.CancelFunc),
		workerCount:  4,
		cb:           cb,
		engineToken:  engineToken,
	}
}

func (s *SchedulerServer) ConnectEngine(ctx context.Context) error {
	var creds credentials.TransportCredentials
	if os.Getenv("QUBIT_ENGINE_CERT_PATH") != "" || os.Getenv("QUBIT_ENGINE_SKIP_AUTH") == "" {
		// Use TLS in production if cert path is available or auth is not explicitly skipped.
		// For simplicity we use default client TLS certs which rely on system CA pool.
		creds = credentials.NewTLS(&tls.Config{})
	} else {
		creds = insecure.NewCredentials()
	}

	targetAddr := s.engineAddr
	if !strings.HasPrefix(targetAddr, "dns:///") {
		targetAddr = "dns:///" + targetAddr
	}

	conn, err := google_grpc.Dial(targetAddr,
		google_grpc.WithTransportCredentials(creds),
		google_grpc.WithPerRPCCredentials(tokenAuth{token: s.engineToken}),
		google_grpc.WithDefaultServiceConfig(`{"loadBalancingPolicy":"round_robin"}`),
	)
	if err != nil {
		return fmt.Errorf("failed to connect to engine: %w", err)
	}
	s.engineConn = conn
	s.engineClient = pb.NewQuantumComputeClient(conn)
	return nil
}

func (s *SchedulerServer) StartWorkers(ctx context.Context) {
	slog.Info("Go Scheduler workers disabled to prevent bounded starvation. Awaiting C++ nodes to pull jobs.")
}

func (s *SchedulerServer) SubmitJob(ctx context.Context, req *pb.JobRequest) (*pb.JobHandle, error) {
	// 1. Rate Limiting with bounded eviction
	if req.UserId != "" {
		l := s.getOrCreateLimiter(req.UserId)
		
		l.mu.Lock()
		now_lim := time.Now()
		dt := now_lim.Sub(l.last).Seconds()
		l.tokens += dt * refillRate
		if l.tokens > burstSize {
			l.tokens = burstSize
		}
		l.last = now_lim
		
		if l.tokens < 1.0 {
			l.mu.Unlock()
			return nil, status.Errorf(codes.ResourceExhausted, "rate limit exceeded for user %s", req.UserId)
		}
		l.tokens -= 1.0
		l.mu.Unlock()
	}

	if req.Shots <= 0 {
		return nil, status.Error(codes.InvalidArgument, "shots must be greater than 0")
	}

	if req.Circuit == nil {
		return nil, status.Error(codes.InvalidArgument, "circuit is required")
	}

	if req.Circuit.NumQubits > maxQubits {
		return nil, status.Errorf(codes.InvalidArgument, "numQubits %d exceeds maximum allowed (%d)", req.Circuit.NumQubits, maxQubits)
	}

	if len(req.Circuit.Operations) > maxOperations {
		return nil, status.Errorf(codes.InvalidArgument, "too many operations: %d (max %d)", len(req.Circuit.Operations), maxOperations)
	}

	for _, op := range req.Circuit.Operations {
		if op.TargetQubit >= uint32(req.Circuit.NumQubits) {
			return nil, status.Errorf(codes.InvalidArgument, "target qubit index %d out of range [0, %d)", op.TargetQubit, req.Circuit.NumQubits)
		}
		if op.ControlQubit != 0 && op.ControlQubit >= uint32(req.Circuit.NumQubits) {
			return nil, status.Errorf(codes.InvalidArgument, "control qubit index %d out of range [0, %d)", op.ControlQubit, req.Circuit.NumQubits)
		}
		if op.SecondControlQubit != 0 && op.SecondControlQubit >= uint32(req.Circuit.NumQubits) {
			return nil, status.Errorf(codes.InvalidArgument, "second control qubit index %d out of range [0, %d)", op.SecondControlQubit, req.Circuit.NumQubits)
		}
	}

	jobID := uuid.New().String()
	now := time.Now().Unix()

	job := &models.Job{
		ID:          jobID,
		UserID:      req.UserId,
		Priority:    models.JobPriority(req.Priority),
		State:       models.StateQueued,
		Shots:       req.Shots,
		CallbackURL: req.CallbackUrl,
		Metadata:    req.Metadata,
		SubmittedAt: now,
	}

	if req.Circuit != nil {
		job.NumQubits = req.Circuit.NumQubits
		job.NumOps = int32(len(req.Circuit.Operations))
		circuitBytes, err := json.Marshal(req.Circuit)
		if err != nil {
			return nil, status.Errorf(codes.Internal, "failed to marshal circuit: %v", err)
		}
		job.CircuitJSON = string(circuitBytes)
	}

	_, err := s.cb.Execute(func() (interface{}, error) {
		jobBytes, err := json.Marshal(job)
		if err != nil {
			return nil, err
		}
		if err := s.rdb.Set(ctx, "job:"+jobID, jobBytes, 24*time.Hour).Err(); err != nil {
			return nil, err
		}

		if req.Circuit != nil {
			b, err := proto.Marshal(req.Circuit)
			if err != nil {
				return nil, err
			}
			if err := s.rdb.Set(ctx, "job:circuitpb:"+jobID, b, 24*time.Hour).Err(); err != nil {
				return nil, err
			}
		}

		if err := s.rdb.Set(ctx, "job:shots:"+jobID, fmt.Sprintf("%d", req.Shots), 24*time.Hour).Err(); err != nil {
			return nil, err
		}

		score := float64(int64(job.Priority)*1000000 - now)
		if err := s.rdb.ZAdd(ctx, "queue:jobs", &redis.Z{
			Score:  score,
			Member: jobID,
		}).Err(); err != nil {
			return nil, err
		}

		// Secondary index for O(log n) ListJobs lookups
		s.rdb.ZAdd(ctx, "index:jobs:all", &redis.Z{
			Score:  float64(now),
			Member: jobID,
		})
		s.rdb.ZAdd(ctx, fmt.Sprintf("index:jobs:state:%d", job.State), &redis.Z{
			Score:  float64(now),
			Member: jobID,
		})
		
		if job.UserID != "" {
			s.rdb.ZAdd(ctx, "index:jobs:user:"+job.UserID, &redis.Z{
				Score:  float64(now),
				Member: jobID,
			})
			s.rdb.ZAdd(ctx, fmt.Sprintf("index:jobs:user:%s:state:%d", job.UserID, job.State), &redis.Z{
				Score:  float64(now),
				Member: jobID,
			})
		}
		
		return nil, nil
	})

	if err != nil {
		return nil, status.Errorf(codes.Unavailable, "redis temporarily unavailable: %v", err)
	}

	queueLen, _ := s.rdb.ZCard(ctx, "queue:jobs").Result()
	estimatedWait := int32(queueLen) * 2

	slog.Info("Job submitted", "job_id", jobID, "qubits", job.NumQubits, "ops", job.NumOps, "priority", job.Priority)

	return &pb.JobHandle{
		JobId:                jobID,
		SubmittedAt:          now,
		EstimatedWaitSeconds: estimatedWait,
	}, nil
}

func (s *SchedulerServer) GetJobStatus(ctx context.Context, handle *pb.JobHandle) (*pb.JobStatus, error) {
	jobBytes, err := s.rdb.Get(ctx, "job:"+handle.JobId).Bytes()
	if err == redis.Nil {
		return nil, status.Errorf(codes.NotFound, "job not found: %s", handle.JobId)
	}
	if err != nil {
		return nil, status.Errorf(codes.Internal, "redis error: %v", err)
	}

	var job models.Job
	if err := json.Unmarshal(jobBytes, &job); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to parse job: %v", err)
	}

	if sVal, err := s.rdb.Get(ctx, "job:state:"+handle.JobId).Int64(); err == nil {
		job.State = models.JobState(sVal)
	}
	if v, err := s.rdb.Get(ctx, "job:started_at:"+handle.JobId).Int64(); err == nil {
		job.StartedAt = v
	}
	if v, err := s.rdb.Get(ctx, "job:completed_at:"+handle.JobId).Int64(); err == nil {
		job.CompletedAt = v
	}
	if v, err := s.rdb.Get(ctx, "job:worker_id:"+handle.JobId).Result(); err == nil {
		job.WorkerID = v
	}
	if v, err := s.rdb.Get(ctx, "job:error:"+handle.JobId).Result(); err == nil {
		job.ErrorMessage = v
	}

	position := int32(0)
	if job.State == models.StateQueued {
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

func (s *SchedulerServer) CancelJob(ctx context.Context, handle *pb.JobHandle) (*pb.CancelResponse, error) {
	removed, _ := s.rdb.ZRem(ctx, "queue:jobs", handle.JobId).Result()
	if removed > 0 {
		s.updateJobState(ctx, handle.JobId, models.StateCancelled, "")
		return &pb.CancelResponse{Success: true, Message: "Job cancelled from queue"}, nil
	}

	s.mu.Lock()
	cancel, exists := s.workerCancel[handle.JobId]
	if exists {
		delete(s.workerCancel, handle.JobId)
	}
	s.mu.Unlock()

	if exists {
		cancel()
		s.updateJobState(ctx, handle.JobId, models.StateCancelled, "")
		return &pb.CancelResponse{Success: true, Message: "Running job cancelled"}, nil
	}

	return &pb.CancelResponse{Success: false, Message: "Job not found or already completed"}, nil
}

func (s *SchedulerServer) updateJobState(ctx context.Context, jobID string, state models.JobState, errMsg string) {
	jobBytes, err := s.rdb.Get(ctx, "job:"+jobID).Bytes()
	if err != nil {
		return
	}
	var job models.Job
	if err := json.Unmarshal(jobBytes, &job); err != nil {
		return
	}
	oldState := job.State
	job.State = state
	job.ErrorMessage = errMsg
	if state == models.StateCompleted || state == models.StateFailed || state == models.StateCancelled {
		job.CompletedAt = time.Now().Unix()
	}
	s.saveJob(ctx, &job)

	// Update state indexes
	if oldState != state {
		s.rdb.ZRem(ctx, fmt.Sprintf("index:jobs:state:%d", oldState), jobID)
		s.rdb.ZAdd(ctx, fmt.Sprintf("index:jobs:state:%d", state), &redis.Z{Score: float64(job.SubmittedAt), Member: jobID})
		if job.UserID != "" {
			s.rdb.ZRem(ctx, fmt.Sprintf("index:jobs:user:%s:state:%d", job.UserID, oldState), jobID)
			s.rdb.ZAdd(ctx, fmt.Sprintf("index:jobs:user:%s:state:%d", job.UserID, state), &redis.Z{Score: float64(job.SubmittedAt), Member: jobID})
		}
	}
}

func (s *SchedulerServer) saveJob(ctx context.Context, job *models.Job) {
	jobBytes, err := json.Marshal(job)
	if err != nil {
		slog.Error("Failed to marshal job for saving", "job_id", job.ID, "error", err)
		return
	}
	s.rdb.Set(ctx, "job:"+job.ID, jobBytes, 24*time.Hour)
}

func (s *SchedulerServer) ListJobs(ctx context.Context, req *pb.ListJobsRequest) (*pb.JobList, error) {
	// 1. Determine pagination bounds
	pageSize := int(req.Limit)
	if pageSize <= 0 || pageSize > 100 {
		pageSize = 50
	}
	offset := int(req.Offset)
	if offset < 0 {
		offset = 0
	}

	var allIDs []string
	var err error

	// If a specific state filter is requested, we must still filter, but we can do MGET 
	// or scan to be efficient. However, if no filter or a typical list is requested:
	// ZRevRange uses 0-based start/stop indices
	start := int64(offset)
	stop := int64(offset + pageSize - 1)

	var indexKey string
	if req.UserId != "" {
		if req.StateFilter != pb.JobState_STATE_UNKNOWN {
			indexKey = fmt.Sprintf("index:jobs:user:%s:state:%d", req.UserId, req.StateFilter)
		} else {
			indexKey = "index:jobs:user:" + req.UserId
		}
	} else {
		if req.StateFilter != pb.JobState_STATE_UNKNOWN {
			indexKey = fmt.Sprintf("index:jobs:state:%d", req.StateFilter)
		} else {
			indexKey = "index:jobs:all"
		}
	}

	allIDs, err = s.rdb.ZRevRange(ctx, indexKey, start, stop).Result()
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to list jobs: %v", err)
	}

	totalCount, _ := s.rdb.ZCard(ctx, indexKey).Result()

	var jobs []*pb.JobStatus
	if len(allIDs) > 0 {
		// Use MGET to fetch all job JSON payloads in a single round-trip
		keys := make([]string, len(allIDs))
		for i, id := range allIDs {
			keys[i] = "job:" + id
		}
		
		results, err := s.rdb.MGet(ctx, keys...).Result()
		if err != nil {
			return nil, status.Errorf(codes.Internal, "failed to fetch jobs metadata: %v", err)
		}

		for _, item := range results {
			if item == nil {
				continue
			}
			jobStr, ok := item.(string)
			if !ok {
				continue
			}
			var job models.Job
			if err := json.Unmarshal([]byte(jobStr), &job); err != nil {
				continue
			}

			// Retrieve runtime overrides
			if sVal, err := s.rdb.Get(ctx, "job:state:"+job.ID).Int64(); err == nil {
				job.State = models.JobState(sVal)
			}
			if v, err := s.rdb.Get(ctx, "job:started_at:"+job.ID).Int64(); err == nil {
				job.StartedAt = v
			}
			if v, err := s.rdb.Get(ctx, "job:completed_at:"+job.ID).Int64(); err == nil {
				job.CompletedAt = v
			}
			if v, err := s.rdb.Get(ctx, "job:worker_id:"+job.ID).Result(); err == nil {
				job.WorkerID = v
			}
			if v, err := s.rdb.Get(ctx, "job:error:"+job.ID).Result(); err == nil {
				job.ErrorMessage = v
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
	}

	return &pb.JobList{
		Jobs:       jobs,
		TotalCount: int32(totalCount),
	}, nil
}

func (s *SchedulerServer) StreamJobResults(handle *pb.JobHandle, stream pb.QuantumScheduler_StreamJobResultsServer) error {
	jobID := handle.JobId

	exists, err := s.rdb.Exists(stream.Context(), "job:"+jobID).Result()
	if err != nil || exists == 0 {
		return status.Errorf(codes.NotFound, "job not found: %s", jobID)
	}

	slog.Info("Client streaming results for job", "job_id", jobID)
	redisKey := "stream:results:" + jobID
	lastID := "0"

	for {
		select {
		case <-stream.Context().Done():
			slog.Info("Client disconnected from stream", "job_id", jobID)
			return stream.Context().Err()
		default:
		}

		streams, err := s.rdb.XRead(stream.Context(), &redis.XReadArgs{
			Streams: []string{redisKey, lastID},
			Block:   2 * time.Second,
			Count:   1,
		}).Result()
		if err == redis.Nil || len(streams) == 0 {
			continue
		}
		if err != nil {
			if stream.Context().Err() != nil {
				return stream.Context().Err()
			}
			return status.Errorf(codes.Internal, "redis stream error: %v", err)
		}

		msg := streams[0].Messages[0]
		lastID = msg.ID

		data, ok := msg.Values["data"].(string)
		if !ok {
			continue
		}

		if data == "EOF" {
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

// collectMetrics gathers real system and queue metrics from the Go runtime and Redis.
func (s *SchedulerServer) collectMetrics(ctx context.Context) *pb.ClusterMetricsResponse {
	var queueLen, runningCount int64

	v, err := s.cb.Execute(func() (interface{}, error) {
		qLen, err := s.rdb.ZCard(ctx, "queue:jobs").Result()
		if err != nil {
			return nil, err
		}
		rCount, err := s.rdb.HLen(ctx, "jobs:processing").Result()
		if err != nil {
			return nil, err
		}
		return []int64{qLen, rCount}, nil
	})
	
	if err == nil {
		counts := v.([]int64)
		queueLen = counts[0]
		runningCount = counts[1]
	}

	// Real memory usage from Go runtime
	var memStats runtime.MemStats
	runtime.ReadMemStats(&memStats)
	// HeapInuse / HeapSys gives proportion of heap actually in use
	memPercent := 0.0
	if memStats.HeapSys > 0 {
		memPercent = float64(memStats.HeapInuse) / float64(memStats.HeapSys) * 100.0
	}

	return &pb.ClusterMetricsResponse{
		ActiveWorkers:      int32(s.workerCount),
		QueueDepth:         int32(queueLen),
		MemoryUsagePercent: memPercent,
		JobsByState: map[int32]int32{
			int32(models.StateQueued):  int32(queueLen),
			int32(models.StateRunning): int32(runningCount),
		},
	}
}

func (s *SchedulerServer) GetClusterMetrics(ctx context.Context, req *emptypb.Empty) (*pb.ClusterMetricsResponse, error) {
	return s.collectMetrics(ctx), nil
}

func (s *SchedulerServer) StreamClusterMetrics(req *emptypb.Empty, stream pb.QuantumScheduler_StreamClusterMetricsServer) error {
	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-stream.Context().Done():
			return nil
		case <-ticker.C:
			if err := stream.Send(s.collectMetrics(stream.Context())); err != nil {
				return err
			}
		}
	}
}
