#!/bin/bash
set -e

# Configuration
BUILD_DIR="backend/build"
OUTPUT_BIN="bin"
VCPKG_ROOT="$HOME/vcpkg"

# Ensure VCPKG is available
if [ ! -d "$VCPKG_ROOT" ]; then
    echo "vcpkg not found at $VCPKG_ROOT. Please install it."
    exit 1
fi

# Clean previous build if requested
if [ "$1" == "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf $BUILD_DIR $OUTPUT_BIN
fi

# Create output directories
mkdir -p $OUTPUT_BIN

# Configure CMake
echo "Configuring CMake..."
cmake -S backend -B $BUILD_DIR \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DMPI_ENABLED=ON \
    -DENABLE_CUDA=OFF

# Build
echo "Building..."
cmake --build $BUILD_DIR -- -j$(nproc)

echo "Build complete. Executables are in $OUTPUT_BIN/"
