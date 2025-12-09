#!/bin/bash
# A helper script to manually generate Protobufs if needed.

# Define paths
PROTO_DIR=./api/proto
CPP_OUT=./backend/src/generated
GO_OUT=./cli/internal/generated

echo "Generating C++ Protobufs..."
mkdir -p $CPP_OUT
protoc -I $PROTO_DIR --grpc_out=$CPP_OUT --cpp_out=$CPP_OUT \
    --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) $PROTO_DIR/quantum.proto

echo "Generating Go Protobufs..."
mkdir -p $GO_OUT
protoc -I $PROTO_DIR --go_out=$GO_OUT --go-grpc_out=$GO_OUT \
    $PROTO_DIR/quantum.proto

echo "Done."
