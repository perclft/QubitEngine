FROM golang:1.24-alpine AS builder

WORKDIR /build

# Install dependencies
RUN apk add --no-cache git

# Copy shared dependencies
COPY api/ api/
COPY go.mod go.sum ./

# Copy service
COPY services/registry/ services/registry/

# Build
WORKDIR /build/services/registry
ENV GOTOOLCHAIN=auto
RUN go mod download 2>/dev/null || go mod tidy
RUN CGO_ENABLED=0 GOOS=linux go build -o registry ./main.go

# Runtime stage
FROM alpine:latest

RUN apk --no-cache add ca-certificates

WORKDIR /app

COPY --from=builder /build/services/registry/registry .

# Non-root user
RUN adduser -D -u 1000 registry
USER registry

EXPOSE 50052

ENTRYPOINT ["./registry"]
