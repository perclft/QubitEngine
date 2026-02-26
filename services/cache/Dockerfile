FROM golang:1.24-alpine AS builder

WORKDIR /build

RUN apk add --no-cache git

# Copy shared dependencies
COPY api/ api/
COPY go.mod go.sum ./

# Copy service
COPY services/cache/ services/cache/

WORKDIR /build/services/cache
RUN go mod download 2>/dev/null || go mod tidy
RUN CGO_ENABLED=0 GOOS=linux go build -o cache ./main.go

# Runtime stage
FROM alpine:latest

RUN apk --no-cache add ca-certificates

WORKDIR /app

COPY --from=builder /build/services/cache/cache .

RUN adduser -D -u 1000 cache
USER cache

EXPOSE 50054

ENTRYPOINT ["./cache"]
