FROM golang:1.23-alpine AS builder

WORKDIR /app

# Copy shared code (CLI generated bindings)
COPY cli cli

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
