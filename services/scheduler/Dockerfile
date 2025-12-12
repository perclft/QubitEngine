FROM golang:1.23-alpine AS builder

WORKDIR /build

RUN apk add --no-cache git

COPY go.mod go.sum* ./
RUN go mod download || true

COPY . .

RUN CGO_ENABLED=0 GOOS=linux go build -o scheduler ./main.go

# Runtime stage
FROM alpine:latest

RUN apk --no-cache add ca-certificates

WORKDIR /app

COPY --from=builder /build/scheduler .

RUN adduser -D -u 1000 scheduler
USER scheduler

EXPOSE 50053

ENTRYPOINT ["./scheduler"]
