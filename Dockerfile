# --- Stage 1: Builder (Debian) ---
FROM debian:bookworm-slim AS builder

# Install Toolchain
RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# 1. Create Output Directory (Matches your Makefile structure)
RUN mkdir -p backend/src/generated

# 2. GENERATE PROTOBUFS
# Use repo root as include path so cross-proto imports resolve (e.g. cache.proto -> api/proto/quantum.proto)
RUN protoc -I . -I api/proto \
    --grpc_out=backend/src/generated --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` \
    --cpp_out=backend/src/generated \
    api/proto/*.proto

# 3. Build Binary
RUN cmake -B build -S backend \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3" && \
    cmake --build build --config Release

# --- Stage 2: Runtime (Debian Slim) ---
FROM debian:bookworm-slim

RUN apt-get update && apt-get upgrade -y && apt-get install -y \
    libgrpc++1.51 libprotobuf32 libssl3 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/build/qubit_engine /usr/local/bin/qubit_engine

EXPOSE 50051
ENTRYPOINT ["qubit_engine"]
