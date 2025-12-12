FROM golang:1.23-alpine AS builder

WORKDIR /build

# Install dependencies
RUN apk add --no-cache git

# Copy go.mod first for caching
COPY go.mod go.sum* ./
RUN go mod download || true

# Copy source
COPY . .

# Build
RUN CGO_ENABLED=0 GOOS=linux go build -o registry ./main.go

# Runtime stage
FROM alpine:latest

RUN apk --no-cache add ca-certificates

WORKDIR /app

COPY --from=builder /build/registry .

# Non-root user
RUN adduser -D -u 1000 registry
USER registry

EXPOSE 50052

ENTRYPOINT ["./registry"]
