package grpc

import (
	"fmt"
	"testing"
	"time"
)

func TestRateLimiterBoundedMemory(t *testing.T) {
	server := NewSchedulerServer(nil, "localhost:50051", "")

	// Insert 2500 unique user limiters
	for i := 0; i < 2500; i++ {
		userID := fmt.Sprintf("user-stress-%d", i)
		l := server.getOrCreateLimiter(userID)
		if l == nil {
			t.Fatalf("failed to create limiter for %s", userID)
		}
	}

	// Trigger periodic cleanup scan
	server.cleanupLimiters()

	// Count active entries in limiters map
	count := 0
	server.limiters.Range(func(key, value interface{}) bool {
		count++
		return true
	})

	if count > MaxLimiterEntries {
		t.Fatalf("limiters map count %d exceeded maximum allowed bound %d", count, MaxLimiterEntries)
	}
	t.Logf("PASS: limiters count %d is strictly bounded <= %d under 2500 unique user load", count, MaxLimiterEntries)
}

func TestRateLimiterLRUTimestampEviction(t *testing.T) {
	server := NewSchedulerServer(nil, "localhost:50051", "")

	// 1. Add oldest user
	oldestID := "user-oldest-access"
	lOld := server.getOrCreateLimiter(oldestID)
	lOld.mu.Lock()
	lOld.last = time.Now().Add(-1 * time.Hour)
	lOld.mu.Unlock()

	// 2. Add 1200 newer users
	for i := 0; i < 1200; i++ {
		userID := fmt.Sprintf("user-recent-%d", i)
		l := server.getOrCreateLimiter(userID)
		l.mu.Lock()
		l.last = time.Now().Add(time.Duration(i) * time.Second)
		l.mu.Unlock()
	}

	// Run LRU eviction
	server.cleanupLimiters()

	// Verify user-oldest-access was evicted first due to oldest timestamp
	_, exists := server.limiters.Load(oldestID)
	if exists {
		t.Fatalf("expected oldest user %s to be evicted via timestamp-sorted LRU", oldestID)
	}
	t.Logf("PASS: oldest user %s successfully evicted first by LRU timestamp ordering", oldestID)
}
