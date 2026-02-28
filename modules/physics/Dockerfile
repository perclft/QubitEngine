FROM golang:1.24-alpine AS builder

WORKDIR /app

# Install build dependencies
RUN apk add --no-cache git gcc musl-dev

# Copy API module first for replace directive
COPY api/ /app/api/

# Copy and download module dependencies
# Copy local replace dependencies
COPY api/ /app/api/
COPY modules/physics/go.mod modules/physics/go.sum* ./modules/physics/

WORKDIR /app/modules/physics
# Rewrite the local path replace directive to match the Docker container layout /app/api
RUN go mod edit -replace github.com/perclft/QubitEngine/api=../../api
RUN go mod download
RUN go mod tidy

# Copy source
COPY modules/physics/ .

# Build
RUN CGO_ENABLED=0 GOOS=linux go build -o /physics main.go

# Runtime Stage
FROM alpine:3.19

# Security best practices
RUN adduser -D -g '' qubit
USER qubit

COPY --from=builder /physics /usr/local/bin/physics

EXPOSE 50060

ENTRYPOINT ["/usr/local/bin/physics"]
