package retry

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"
	"time"

	"github.com/go-redis/redis/v8"
)

// DLQRetryer handles moving failed jobs from a Dead Letter Queue back to the main queue with exponential backoff.
type DLQRetryer struct {
	rdb       *redis.Client
	mainQueue string
	dlqName   string
}

func NewDLQRetryer(rdb *redis.Client, mainQueue, dlqName string) *DLQRetryer {
	return &DLQRetryer{
		rdb:       rdb,
		mainQueue: mainQueue,
		dlqName:   dlqName,
	}
}

type FailedJob struct {
	JobID      string    `json:"id"`
	Error      string    `json:"error"`
	RetryCount int       `json:"retry_count"`
	FirstSeen  time.Time `json:"first_seen"`
}

func (r *DLQRetryer) Start(ctx context.Context) {
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			r.processDLQ(ctx)
		}
	}
}

func (r *DLQRetryer) processDLQ(ctx context.Context) {
	// Simple scaffold: pop from DLQ, check retry count, push back to main if < 3
	jobs, err := r.rdb.LRange(ctx, r.dlqName, 0, -1).Result()
	if err != nil {
		slog.Error("Failed to read DLQ", "error", err)
		return
	}

	for _, jobJSON := range jobs {
		var job FailedJob
		if err := json.Unmarshal([]byte(jobJSON), &job); err != nil {
			continue
		}

		if job.RetryCount < 3 {
			// Exponential backoff wait (simplified)
			wait := time.Duration(1<<job.RetryCount) * time.Minute
			if time.Since(job.FirstSeen) < wait {
				continue
			}

			slog.Info("Retrying job from DLQ", "job_id", job.JobID, "retry", job.RetryCount+1)
			job.RetryCount++
			newJSON, _ := json.Marshal(job)

			// Atomic move: remove from DLQ and push to main queue
			pipe := r.rdb.Pipeline()
			pipe.LRem(ctx, r.dlqName, 1, jobJSON)
			pipe.LPush(ctx, r.mainQueue, job.JobID) // Pushing just the ID as per scheduler logic
			_, err := pipe.Exec(ctx)
			if err != nil {
				slog.Error("Failed to retry job", "job_id", job.JobID, "error", err)
			}
		} else {
			slog.Warn("Job exceeded max retries, leaving in DLQ", "job_id", job.JobID)
		}
	}
}
