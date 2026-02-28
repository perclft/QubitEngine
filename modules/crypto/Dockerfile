FROM golang:1.24-alpine AS builder

WORKDIR /app

# Copy shared code (CLI generated bindings and API)
COPY api api
COPY cli cli
COPY go.mod go.sum ./

# Copy module code
COPY modules/crypto modules/crypto

WORKDIR /app/modules/crypto
RUN go mod download 2>/dev/null || go mod tidy
RUN CGO_ENABLED=0 GOOS=linux go build -o crypto-server .

FROM alpine:3.19
RUN apk --no-cache add ca-certificates
WORKDIR /app
COPY --from=builder /app/modules/crypto/crypto-server .
EXPOSE 50063
ENTRYPOINT ["./crypto-server"]
