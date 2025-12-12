FROM golang:1.21-alpine AS builder

WORKDIR /app
COPY go.mod go.sum* ./
RUN go mod download 2>/dev/null || go mod tidy
COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -o gaming-server .

FROM alpine:3.19
RUN apk --no-cache add ca-certificates
WORKDIR /app
COPY --from=builder /app/gaming-server .
EXPOSE 50061
ENTRYPOINT ["./gaming-server"]
