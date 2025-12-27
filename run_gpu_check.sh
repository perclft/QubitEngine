#!/bin/bash
set -e

echo "=== QubitEngine GPU Verification Script ==="
echo "Checking for NVIDIA GPU..."

if ! command -v nvcc &> /dev/null
then
    echo "Warning: 'nvcc' not found in PATH. Checking common locations..."
    
    # Check Arch Linux default
    if [ -d "/opt/cuda/bin" ]; then
        echo "Found CUDA at /opt/cuda"
        export PATH=$PATH:/opt/cuda/bin
        export CUDACXX=/opt/cuda/bin/nvcc
    # Check standard Linux location
    elif [ -d "/usr/local/cuda/bin" ]; then
        echo "Found CUDA at /usr/local/cuda"
        export PATH=$PATH:/usr/local/cuda/bin
        export CUDACXX=/usr/local/cuda/bin/nvcc
    else
        echo "Error: Could not locate CUDA installation. Please set PATH manually."
        echo "Example: export PATH=\$PATH:/opt/cuda/bin"
        exit 1
    fi
fi

echo "Using nvcc: $(which nvcc)"
nvcc --version | grep release

echo ""
echo "=== Building Backend with CUDA Support ==="
mkdir -p backend/build
cd backend
rm -rf build/*

# Check for vcpkg in common locations
VCPKG_ROOT=""
if [ -d "$HOME/vcpkg" ]; then
    VCPKG_ROOT="$HOME/vcpkg"
elif [ -d "../vcpkg" ]; then
    VCPKG_ROOT="../vcpkg"
fi

CMAKE_FLAGS="-DENABLE_CUDA=ON"

if [ -n "$VCPKG_ROOT" ]; then
    echo "Found vcpkg at $VCPKG_ROOT"
    CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
else
    echo "Warning: vcpkg not found. Relying on system libraries."
    echo "If build fails, ensure: grpc, protobuf, openssl, gflags, glog, gtest are installed."
    echo "Arch tip: sudo pacman -S grpc protobuf openssl gflags google-glog gtest"
    # Attempt to prompt cmake to verify gRPC explicitly if needed, but standard find_package should work if installed.
fi

echo "Running CMake with flags: $CMAKE_FLAGS"
cmake -B build -S . $CMAKE_FLAGS
make -C build -j$(nproc)

echo ""
echo "=== Running GPU Binding Verification ==="
cd ..
export PYTHONPATH=$PYTHONPATH:$(pwd)/backend/build
python3 verify_gpu_binding.py

echo ""
echo "=== Success! GPU Backend is operational. ==="
