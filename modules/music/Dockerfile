FROM golang:1.21-alpine AS builder

WORKDIR /app
COPY go.mod go.sum* ./
RUN go mod download 2>/dev/null || go mod tidy
COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -o music-server .

FROM alpine:3.19
RUN apk --no-cache add ca-certificates
WORKDIR /app
COPY --from=builder /app/music-server .
EXPOSE 50062
ENTRYPOINT ["./music-server"]
