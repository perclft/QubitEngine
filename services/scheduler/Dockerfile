FROM golang:1.25-alpine AS builder
WORKDIR /build
RUN apk add --no-cache git

# Copy shared dependencies
COPY api/ api/
COPY go.mod go.sum ./

# Copy service
COPY services/scheduler/ services/scheduler/

WORKDIR /build/services/scheduler
ENV GOTOOLCHAIN=auto
RUN go mod download 2>/dev/null || go mod tidy
RUN CGO_ENABLED=0 GOOS=linux go build -o scheduler ./cmd/scheduler/main.go

# Runtime stage
FROM alpine:latest

RUN apk --no-cache add ca-certificates

WORKDIR /app

COPY --from=builder /build/services/scheduler/scheduler .

RUN adduser -D -u 1000 scheduler
USER scheduler

EXPOSE 50053

ENTRYPOINT ["./scheduler"]
