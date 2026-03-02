#!/bin/bash
# A helper script to manually generate Protobufs if needed.

# Define paths
PROTO_DIR=./api/proto
CPP_OUT=./backend/src/generated
GO_OUT=./api/generated
PYTHON_OUT=./python/qubit_engine/proto

echo "Cleaning up old generated folders..."
rm -rf ./modules/*/generated

echo "Generating C++ Protobufs..."
mkdir -p $CPP_OUT
protoc -I $PROTO_DIR --grpc_out=$CPP_OUT --cpp_out=$CPP_OUT \
    --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) $PROTO_DIR/quantum.proto

echo "Generating Go Protobufs..."
mkdir -p $GO_OUT
protoc -I . --go_out=. --go_opt=module=github.com/perclft/QubitEngine \
    --go-grpc_out=. --go-grpc_opt=module=github.com/perclft/QubitEngine \
    api/proto/*.proto api/proto/*/*.proto

echo "Protobuf generation complete!"
