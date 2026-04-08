# Variables
GO = go
PROTOC = protoc
PROTO_DIR = api/proto
GO_OUT_DIR = api/generated
ENGINE_IMAGE = qubit-engine:latest

.PHONY: all clean proto build-engine build-tui build-services build-web all-fullstack docker-build deploy test docs

# Generate API documentation
docs:
	@bash scripts/generate_docs.sh


all: proto build-engine

all-fullstack: all build-tui build-services build-web

# Generate Go protobuf stubs
proto:
	@echo "Generating Protobufs..."
	@mkdir -p $(GO_OUT_DIR)
	$(PROTOC) -I . \
		--go_out=$(GO_OUT_DIR) --go_opt=paths=source_relative \
		--go-grpc_out=$(GO_OUT_DIR) --go-grpc_opt=paths=source_relative \
		$(PROTO_DIR)/*.proto

# Build C++ engine (requires vcpkg toolchain)
build-engine:
	@echo "Building C++ Engine..."
	@mkdir -p backend/build
	cd backend/build && cmake .. -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake -DMPI_ENABLED=ON && cmake --build . --config Release --parallel

# Build Rust TUI
build-tui:
	@echo "Building Rust TUI..."
	cd cli-rs && cargo build --release

# Build Go Microservices
build-services:
	@echo "Building Go Microservices..."
	cd services/scheduler && $(GO) build -o scheduler .
	cd services/registry && $(GO) build -o registry .
	cd services/cache && $(GO) build -o cache .

# Build Web Frontend
build-web:
	@echo "Building Next.js Web Frontend..."
	cd web && npm install && npm run build

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
	rm -f services/scheduler/scheduler
	rm -f services/registry/registry
	rm -f services/cache/cache
	rm -rf web/.next web/node_modules
