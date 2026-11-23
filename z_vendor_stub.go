//go:build ignore
// +build ignore

package main

import (
    _ "google.golang.org/grpc/health"
    _ "google.golang.org/grpc/health/grpc_health_v1"
    _ "google.golang.org/grpc/peer"
    _ "google.golang.org/grpc/stats"
    _ "google.golang.org/grpc/encoding/proto"
    _ "go.opentelemetry.io/otel"
    _ "go.opentelemetry.io/otel/attribute"
    _ "go.opentelemetry.io/otel/semconv/v1.4.0"
    _ "go.opentelemetry.io/otel/semconv/v1.17.0"
    _ "go.opentelemetry.io/otel/sdk"
    _ "go.opentelemetry.io/otel/sdk/resource"
    _ "go.opentelemetry.io/otel/sdk/trace"
)
