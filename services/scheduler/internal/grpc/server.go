package grpc

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"log/slog"
	"strings"
	"sync"
	"time"

	"github.com/go-redis/redis/v8"
	"github.com/google/uuid"
	pb "github.com/perclft/QubitEngine/api/generated"
	"github.com/perclft/QubitEngine/services/scheduler/pkg/models"
	google_grpc "google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
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
}

func NewSchedulerServer(rdb *redis.Client, engineAddr string) *SchedulerServer {
	return &SchedulerServer{
		rdb:          rdb,
		engineAddr:   engineAddr,
		workerCancel: make(map[string]context.CancelFunc),
		workerCount:  1000,
	}
}

func (s *SchedulerServer) ConnectEngine(ctx context.Context) error {
	conn, err := google_grpc.Dial(s.engineAddr, google_grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return fmt.Errorf("failed to connect to engine: %w", err)
	}
	s.engineConn = conn
	s.engineClient = pb.NewQuantumComputeClient(conn)
	return nil
}

func (s *SchedulerServer) StartWorkers(ctx context.Context) {
	log.Println("Go Scheduler workers disabled to prevent bounded starvation. Awaiting C++ nodes to pull jobs.")
}

func (s *SchedulerServer) SubmitJob(ctx context.Context, req *pb.JobRequest) (*pb.JobHandle, error) {
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
		circuitBytes, _ := json.Marshal(req.Circuit)
		job.CircuitJSON = string(circuitBytes)
	}

	jobBytes, _ := json.Marshal(job)
	if err := s.rdb.Set(ctx, "job:"+jobID, jobBytes, 24*time.Hour).Err(); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to store job: %v", err)
	}

	if req.Circuit != nil {
		b, err := proto.Marshal(req.Circuit)
		if err != nil {
			return nil, status.Errorf(codes.Internal, "failed to marshal circuit proto: %v", err)
		}
		if err := s.rdb.Set(ctx, "job:circuitpb:"+jobID, b, 24*time.Hour).Err(); err != nil {
			return nil, status.Errorf(codes.Internal, "failed to store circuit proto: %v", err)
		}
	}

	if err := s.rdb.Set(ctx, "job:shots:"+jobID, fmt.Sprintf("%d", req.Shots), 24*time.Hour).Err(); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to store shots: %v", err)
	}

	score := float64(int64(job.Priority)*1000000 - now)
	if err := s.rdb.ZAdd(ctx, "queue:jobs", &redis.Z{
		Score:  score,
		Member: jobID,
	}).Err(); err != nil {
		return nil, status.Errorf(codes.Internal, "failed to queue job: %v", err)
	}

	queueLen, _ := s.rdb.ZCard(ctx, "queue:jobs").Result()
	estimatedWait := int32(queueLen) * 2

	log.Printf("📥 Job submitted: %s (qubits=%d, ops=%d, priority=%d)",
		jobID, job.NumQubits, job.NumOps, job.Priority)

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

	s.mu.RLock()
	cancel, exists := s.workerCancel[handle.JobId]
	s.mu.RUnlock()

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
	job.State = state
	job.ErrorMessage = errMsg
	if state == models.StateCompleted || state == models.StateFailed || state == models.StateCancelled {
		job.CompletedAt = time.Now().Unix()
	}
	s.saveJob(ctx, &job)
}

func (s *SchedulerServer) saveJob(ctx context.Context, job *models.Job) {
	jobBytes, _ := json.Marshal(job)
	s.rdb.Set(ctx, "job:"+job.ID, jobBytes, 24*time.Hour)
}

func (s *SchedulerServer) ListJobs(ctx context.Context, req *pb.ListJobsRequest) (*pb.JobList, error) {
	var allKeys []string
	var cursor uint64
	for {
		keys, nextCursor, err := s.rdb.Scan(ctx, cursor, "job:*", 100).Result()
		if err != nil {
			return nil, status.Errorf(codes.Internal, "failed to scan jobs: %v", err)
		}
		allKeys = append(allKeys, keys...)
		cursor = nextCursor
		if cursor == 0 {
			break
		}
	}

	var jobs []*pb.JobStatus
	for _, key := range allKeys {
		if strings.Count(key, ":") > 1 {
			continue
		}

		jobBytes, err := s.rdb.Get(ctx, key).Bytes()
		if err != nil {
			continue
		}
		var job models.Job
		if err := json.Unmarshal(jobBytes, &job); err != nil {
			continue
		}

		if req.UserId != "" && job.UserID != req.UserId {
			continue
		}

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

	exists, err := s.rdb.Exists(stream.Context(), "job:"+jobID).Result()
	if err != nil || exists == 0 {
		return status.Errorf(codes.NotFound, "job not found: %s", jobID)
	}

	log.Printf("📡 Client streaming results for job: %s", jobID)
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
