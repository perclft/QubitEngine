#!/bin/bash
set -e

# Default Target
TARGET=${1:-all}

ROOT_DIR=$(pwd)
BUILD_DIR="${ROOT_DIR}/backend/build"
GENERATED_DIR="${ROOT_DIR}/cli/internal/generated"
VCPKG_ROOT="${HOME}/vcpkg"

# Colors
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

log_info() { echo -e "${CYAN}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

check_deps() {
    log_info "Checking dependencies..."
    
    if [[ "$OSTYPE" == "darwin"* ]]; then
        if ! command -v brew &> /dev/null; then
            log_error "Homebrew not found. Please install it first."
            exit 1
        fi
        
        # Install dependencies via Brew
        log_info "Installing dependencies via Homebrew..."
        brew install cmake protobuf grpc openssl@3 googletest prometheus-cpp
        
        # Add Homebrew to PATH if needed (Apple Silicon)
        if [[ -d "/opt/homebrew/bin" ]]; then
            export PATH="/opt/homebrew/bin:$PATH"
        fi
    else
        log_warn "Not on macOS. Skipping Homebrew checks. Ensure dependencies are installed!"
    fi
    
    if ! command -v go &> /dev/null; then
        log_error "Go not found. Please install Go."
        exit 1
    fi
}

build_proto() {
    log_info "Generating Protobufs..."
    
    mkdir -p "${GENERATED_DIR}"
    
    # Ensure Go plugins are installed
    go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
    go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest
    
    # Export GOBIN to PATH
    export PATH="$(go env GOPATH)/bin:$PATH"

    PROTO_DIR="${ROOT_DIR}/api/proto"
    
    protoc -I="${PROTO_DIR}" \
        --go_out="${GENERATED_DIR}" --go_opt=paths=source_relative \
        --go-grpc_out="${GENERATED_DIR}" --go-grpc_opt=paths=source_relative \
        "${PROTO_DIR}/quantum.proto"
        
    log_success "Protobufs generated."
}

build_cpp() {
    log_info "Building C++ Engine..."
    
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    CMAKE_ARGS=""
    if [[ "$OSTYPE" == "darwin"* ]]; then
       # Helper for OpenSSL on Mac (Brew location)
       OPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
       CMAKE_ARGS="-DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}"
    fi
    
    # Use Manifest Mode if vcpkg.json present and no brew? 
    # Actually, for Mac we prefer Brew as per plan.
    # We'll let CMake find packages naturally.
    
    cmake .. -DCMAKE_BUILD_TYPE=Release ${CMAKE_ARGS}
    cmake --build . --config Release -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)
    
    cd "${ROOT_DIR}"
    log_success "C++ Engine built."
}

build_go() {
    log_info "Building Go Services..."
    
    if [ -d "${ROOT_DIR}/modules/gaming" ]; then
        cd "${ROOT_DIR}/modules/gaming"
        go build -o "${ROOT_DIR}/bin/gaming_service" .
        cd "${ROOT_DIR}"
    fi
    
    if [ -d "${ROOT_DIR}/cli" ]; then
        cd "${ROOT_DIR}/cli"
        go build -o "${ROOT_DIR}/bin/qctl" ./cmd/qctl
        cd "${ROOT_DIR}"
    fi
    
    log_success "Go services built."
}

run_tests() {
    log_info "Running Tests..."
    
    if [ ! -d "${BUILD_DIR}" ]; then
        log_error "Build directory not found. Run ./build.sh cpp first."
        exit 1
    fi
    
    cd "${BUILD_DIR}"
    ctest --output-on-failure
    cd "${ROOT_DIR}"
}

clean() {
    log_info "Cleaning..."
    rm -rf "${BUILD_DIR}"
    rm -rf "${GENERATED_DIR}"
    rm -f "${ROOT_DIR}/bin/gaming_service" "${ROOT_DIR}/bin/qctl"
    log_success "Cleaned."
}

case "$TARGET" in
    deps)
        check_deps
        ;;
    proto)
        build_proto
        ;;
    cpp)
        check_deps
        build_cpp
        ;;
    go)
        check_deps
        build_go
        ;;
    test)
        run_tests
        ;;
    clean)
        clean
        ;;
    all)
        check_deps
        build_proto
        build_cpp
        build_go
        ;;
    *)
        echo "Usage: ./build.sh {deps|proto|cpp|go|test|clean|all}"
        exit 1
        ;;
esac
