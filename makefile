# Variables
CXX = g++
GO = go
PROTOC = protoc
PROTO_DIR = api/proto
CPP_OUT_DIR = backend/src/generated
GO_OUT_DIR = cli/internal/generated
ENGINE_IMAGE = qubit-engine:latest
CLI_IMAGE = qctl:latest

.PHONY: all clean proto build-cpp build-go docker-build deploy test

all: proto build-cpp build-go

# Note: Proto generation is now handled by CMake for C++, 
# but we keep this target for Go and manual checks.
proto:
	@echo "Generating Protobufs..."
	@mkdir -p $(GO_OUT_DIR)
	$(PROTOC) -I $(PROTO_DIR) \
    --go_out=$(GO_OUT_DIR) --go_opt=paths=source_relative \
    --go-grpc_out=$(GO_OUT_DIR) --go-grpc_opt=paths=source_relative \
    $(PROTO_DIR)/quantum.proto

build-cpp:
	@echo "Building C++ Engine..."
	@mkdir -p backend/build
	# Assumes vcpkg toolchain is set in environment or handled by user config
	cd backend/build && cmake .. && make

build-go: proto
	@echo "Building Go CLI..."
	cd cli && $(GO) build -o ../bin/qctl ./cmd/qctl

test:
	@echo "Running C++ Unit Tests..."
	cd backend/build && ctest --output-on-failure

# Build Docker Images
docker-build:
	@echo "Building Docker Images..."
	docker build -t $(ENGINE_IMAGE) -f deploy/docker/Dockerfile.engine .
	docker build -t $(CLI_IMAGE) -f deploy/docker/Dockerfile.cli .

# Generate Web Clients (TypeScript)
web-proto:
	@echo "Generating Web Clients..."
	@mkdir -p web/src/generated
	@rm -rf web/src/generated/*
	$(PROTOC) -I $(PROTO_DIR) \
		--ts_out=web/src/generated \
		--plugin=protoc-gen-ts=./web/node_modules/.bin/protoc-gen-ts \
		$(PROTO_DIR)/quantum.proto

# Run the full stack (Engine + Envoy + Web)
run-web:
	@echo "Starting Full Stack..."
	docker compose -f deploy/docker/docker-compose.yaml up --build

deploy:
	@echo "Deploying to Kubernetes..."
	kubectl apply -f deploy/k8s/namespace.yaml
	kubectl apply -f deploy/k8s/
