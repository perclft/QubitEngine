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

func TestRateLimiterTTLEviction(t *testing.T) {
	server := NewSchedulerServer(nil, "localhost:50051", "")

	userID := "user-stale"
	l := server.getOrCreateLimiter(userID)
	l.mu.Lock()
	l.last = time.Now().Add(-10 * time.Minute) // Mark as inactive > 5 minutes
	l.mu.Unlock()

	// Trigger cleanup via next limiter insertion
	server.getOrCreateLimiter("user-active")

	// Check if user-stale was evicted
	_, exists := server.limiters.Load(userID)
	if exists {
		t.Fatalf("expected inactive limiter %s to be evicted via TTL", userID)
	}
	t.Logf("PASS: inactive user limiter successfully evicted via TTL sweep")
}
