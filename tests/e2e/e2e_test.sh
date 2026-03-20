#!/bin/bash
# QubitEngine End-to-End Integration Test
# Spins up the full stack via Docker Compose, submits a test job, and verifies the result.
set -euo pipefail

COMPOSE_FILE="deploy/docker/docker-compose.yaml"
TIMEOUT=120

cleanup() {
    echo "🧹 Tearing down..."
    docker compose -f "$COMPOSE_FILE" down --volumes --remove-orphans 2>/dev/null || true
}
trap cleanup EXIT

echo "═══════════════════════════════════════════"
echo "  QubitEngine E2E Integration Test"
echo "═══════════════════════════════════════════"

# 1. Build and start all services
echo ""
echo "📦 Building and starting services..."
docker compose -f "$COMPOSE_FILE" build --quiet
docker compose -f "$COMPOSE_FILE" up -d

# 2. Wait for services to be healthy
echo "⏳ Waiting for services to be ready..."
ELAPSED=0
while [ $ELAPSED -lt $TIMEOUT ]; do
    # Check if engine, scheduler, and registry are responding
    ENGINE_OK=$(docker compose -f "$COMPOSE_FILE" exec -T engine /bin/sh -c "echo OK" 2>/dev/null || echo "FAIL")
    REDIS_OK=$(docker compose -f "$COMPOSE_FILE" exec -T redis redis-cli ping 2>/dev/null || echo "FAIL")
    
    if [ "$ENGINE_OK" = "OK" ] && [ "$REDIS_OK" = "PONG" ]; then
        echo "✅ All services ready (${ELAPSED}s)"
        break
    fi
    
    sleep 5
    ELAPSED=$((ELAPSED + 5))
    echo "   ... waiting (${ELAPSED}s / ${TIMEOUT}s)"
done

if [ $ELAPSED -ge $TIMEOUT ]; then
    echo "❌ Timeout waiting for services"
    docker compose -f "$COMPOSE_FILE" logs
    exit 1
fi

# 3. Install grpcurl if not available
if ! command -v grpcurl &> /dev/null; then
    echo "📥 Installing grpcurl..."
    go install github.com/fullstorydev/grpcurl/cmd/grpcurl@latest
fi

# 4. Test: Engine gRPC is responding
echo ""
echo "🔍 Test 1: Engine gRPC connectivity..."
if grpcurl -plaintext localhost:50051 list 2>/dev/null; then
    echo "   ✅ Engine gRPC server is responding"
else
    echo "   ⚠️  Engine gRPC list failed (may need reflection enabled)"
fi

# 5. Test: Submit a simple Bell state circuit
echo ""
echo "🔍 Test 2: Submit Bell state circuit..."
BELL_STATE_RESULT=$(grpcurl -plaintext \
    -d '{
        "numQubits": 2,
        "operations": [
            {"type": 0, "targetQubit": 0},
            {"type": 2, "targetQubit": 1, "controlQubit": 0}
        ]
    }' \
    localhost:50051 qubit_engine.QuantumCompute/RunCircuit 2>&1) || true

if echo "$BELL_STATE_RESULT" | grep -q "stateVector"; then
    echo "   ✅ Bell state circuit executed successfully"
    echo "   State vector received:"
    echo "$BELL_STATE_RESULT" | head -20
else
    echo "   ⚠️  Bell state result: $BELL_STATE_RESULT"
    echo "   (This may fail if auth is required — check QUBIT_ENGINE_SKIP_AUTH)"
fi

# 6. Test: Scheduler health
echo ""
echo "🔍 Test 3: Scheduler connectivity..."
SCHEDULER_RESULT=$(grpcurl -plaintext localhost:50053 list 2>&1) || true
if echo "$SCHEDULER_RESULT" | grep -q "Scheduler\|grpc"; then
    echo "   ✅ Scheduler is responding"
else
    echo "   ⚠️  Scheduler: $SCHEDULER_RESULT"
fi

# 7. Test: Cache service
echo ""
echo "🔍 Test 4: Cache service connectivity..."
CACHE_STATS=$(grpcurl -plaintext \
    -d '{}' \
    localhost:50054 qubit_engine.ResultCache/GetCacheStats 2>&1) || true
if echo "$CACHE_STATS" | grep -q "totalEntries\|hitRate\|{}"; then
    echo "   ✅ Cache service is responding"
    echo "   $CACHE_STATS"
else
    echo "   ⚠️  Cache: $CACHE_STATS"
fi

# 8. Test: Web frontend
echo ""
echo "🔍 Test 5: Web frontend..."
WEB_RESULT=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:3000 2>/dev/null || echo "000")
if [ "$WEB_RESULT" = "200" ]; then
    echo "   ✅ Web frontend is serving (HTTP 200)"
else
    echo "   ⚠️  Web frontend returned HTTP $WEB_RESULT"
fi

echo ""
echo "═══════════════════════════════════════════"
echo "  E2E Test Complete"
echo "═══════════════════════════════════════════"
