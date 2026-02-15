FROM golang:1.24-alpine AS builder


WORKDIR /build

# Copy shared dependencies
COPY api/ api/
COPY pkg/ pkg/
COPY go.mod go.sum ./

# Copy module
COPY modules/music/ modules/music/

WORKDIR /build/modules/music
RUN go mod download 2>/dev/null || go mod tidy
RUN CGO_ENABLED=0 GOOS=linux go build -o music-server .

FROM alpine:3.19
RUN apk --no-cache add ca-certificates
WORKDIR /app
COPY --from=builder /build/modules/music/music-server .
EXPOSE 50062
ENTRYPOINT ["./music-server"]
