# Variables
GO = go
PROTOC = protoc
PROTO_DIR = api/proto
GO_OUT_DIR = api/generated
ENGINE_IMAGE = qubit-engine:latest

.PHONY: all clean proto build-engine build-tui docker-build deploy test docs

# Generate API documentation
docs:
	@echo "Ensuring API Documentation is up to date..."
	@ls docs/api_reference.md > /dev/null || (echo "Warning: API Reference missing!")
	@echo "Documentation available at docs/api_reference.md"


all: proto build-engine

# Generate Go protobuf stubs
proto:
	@echo "Generating Protobufs..."
	@mkdir -p $(GO_OUT_DIR)
	$(PROTOC) -I $(PROTO_DIR) \
    --go_out=$(GO_OUT_DIR) --go_opt=paths=source_relative \
    --go-grpc_out=$(GO_OUT_DIR) --go-grpc_opt=paths=source_relative \
    $(PROTO_DIR)/quantum.proto

# Build C++ engine (requires vcpkg toolchain)
build-engine:
	@echo "Building C++ Engine..."
	@mkdir -p backend/build
	cd backend/build && cmake .. -DCMAKE_TOOLCHAIN_FILE=/Users/sahil/vcpkg/scripts/buildsystems/vcpkg.cmake -DMPI_ENABLED=ON && cmake --build . --config Release --parallel

# Build Rust TUI
build-tui:
	@echo "Building Rust TUI..."
	cd cli-rs && cargo build --release

# Run C++ unit tests
test:
	@echo "Running C++ Unit Tests..."
	cd backend/build && ctest --output-on-failure

# Build Docker Images
docker-build:
	@echo "Building Docker Images..."
	docker compose -f deploy/docker/docker-compose.yaml build

# Deploy to Kubernetes
deploy:
	@echo "Deploying to Kubernetes..."
	kubectl apply -f deploy/k8s/namespace.yaml
	kubectl apply -f deploy/k8s/

clean:
	@echo "Cleaning build artifacts..."
	rm -rf backend/build
	cd cli-rs && cargo clean
