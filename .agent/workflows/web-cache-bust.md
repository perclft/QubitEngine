---
description: How to rebuild and test web changes with cache busting
---

# Web Cache-Busting Build & Test Workflow

When making changes to the web frontend, the browser may cache old JavaScript bundles. Follow these steps to ensure you see the latest changes:

## 1. Stop and Rebuild Web Container (No Cache)

```bash
docker compose -f deploy/docker/docker-compose.yaml down web && \
docker compose -f deploy/docker/docker-compose.yaml build --no-cache web && \
docker compose -f deploy/docker/docker-compose.yaml up -d web
```

## 2. Force Browser Refresh

After the container is rebuilt:

- **Chrome/Edge**: `Ctrl + Shift + R` (Windows/Linux) or `Cmd + Shift + R` (Mac)
- **Firefox**: `Ctrl + Shift + R` or `Cmd + Shift + R`
- **Alternative**: Close browser tab completely and reopen `http://localhost:5173`
- **Best**: Use Incognito/Private Window for testing

## 3. Full Platform Rebuild (All Services)

If you need to rebuild everything with no cache:

```bash
docker compose -f deploy/docker/docker-compose.yaml down && \
docker compose -f deploy/docker/docker-compose.yaml build --no-cache && \
docker compose -f deploy/docker/docker-compose.yaml up -d
```

## 4. Verify Services Are Running

```bash
docker compose -f deploy/docker/docker-compose.yaml ps
```

Expected output should show all services as `Up`:
- postgres (healthy)
- redis
- engine
- registry
- envoy
- web

## Quick Reference Ports

| Service | Port |
|---------|------|
| Web Dashboard | http://localhost:5173 |
| Envoy Proxy | http://localhost:8080 |
| Engine (gRPC) | localhost:50051 |
| Registry (gRPC) | localhost:50052 |
| PostgreSQL | localhost:5432 |
| Redis | localhost:6379 |
