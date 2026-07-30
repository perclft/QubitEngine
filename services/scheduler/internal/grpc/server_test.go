package grpc

import (
	"context"
	"encoding/json"
	"fmt"
	"sync"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/go-redis/redis/v8"
	"github.com/perclft/QubitEngine/services/scheduler/pkg/models"
)

func TestAtomicJobStateUpdate(t *testing.T) {
	ctx := context.Background()
	
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("failed to start miniredis: %v", err)
	}
	defer mr.Close()

	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()

	// Clear out any old state
	rdb.FlushDB(ctx)

	server := NewSchedulerServer(rdb, "localhost:50051", "token")

	jobID := "test-race-job"
	job := &models.Job{
		ID:          jobID,
		UserID:      "user1",
		State:       models.StateQueued,
		SubmittedAt: time.Now().Unix(),
	}

	jobBytes, _ := json.Marshal(job)
	rdb.Set(ctx, "job:"+jobID, jobBytes, 0)

	var wg sync.WaitGroup
	workers := 10
	iterations := 100

	for i := 0; i < workers; i++ {
		wg.Add(1)
		go func(w int) {
			defer wg.Done()
			for j := 0; j < iterations; j++ {
				state := models.StateRunning
				if j%2 == 0 {
					state = models.StateCompleted
				}
				server.updateJobState(ctx, jobID, state, "test msg")
			}
		}(i)
	}

	wg.Wait()

	jobStr, err := rdb.Get(ctx, "job:"+jobID).Result()
	if err != nil {
		t.Fatalf("failed to get job: %v", err)
	}

	var finalJob models.Job
	if err := json.Unmarshal([]byte(jobStr), &finalJob); err != nil {
		t.Fatalf("failed to unmarshal job: %v", err)
	}

	if finalJob.State != models.StateRunning && finalJob.State != models.StateCompleted {
		t.Errorf("expected final state to be Running (%d) or Completed (%d), got %d", models.StateRunning, models.StateCompleted, finalJob.State)
	}

	// Verify that the final state index has the job ID, and the alternative state does not
	finalStateInt := int(finalJob.State)
	otherStateInt := int(models.StateRunning)
	if finalJob.State == models.StateRunning {
		otherStateInt = int(models.StateCompleted)
	}

	// Global State Index
	_, err = rdb.ZScore(ctx, fmt.Sprintf("index:jobs:state:%d", finalStateInt), jobID).Result()
	if err != nil {
		t.Errorf("expected job ID %s to be in index:jobs:state:%d, but got error: %v", jobID, finalStateInt, err)
	}

	_, err = rdb.ZScore(ctx, fmt.Sprintf("index:jobs:state:%d", otherStateInt), jobID).Result()
	if err != redis.Nil {
		t.Errorf("expected job ID %s to be absent from index:jobs:state:%d, but it was found", jobID, otherStateInt)
	}

	// User State Index
	userID := "user1"
	_, err = rdb.ZScore(ctx, fmt.Sprintf("index:jobs:user:%s:state:%d", userID, finalStateInt), jobID).Result()
	if err != nil {
		t.Errorf("expected job ID %s to be in index:jobs:user:%s:state:%d, but got error: %v", jobID, userID, finalStateInt, err)
	}

	_, err = rdb.ZScore(ctx, fmt.Sprintf("index:jobs:user:%s:state:%d", userID, otherStateInt), jobID).Result()
	if err != redis.Nil {
		t.Errorf("expected job ID %s to be absent from index:jobs:user:%s:state:%d, but it was found", jobID, userID, otherStateInt)
	}
}
