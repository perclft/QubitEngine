#!/bin/bash
set -e

# Configuration
BUILD_DIR="backend/build"
OUTPUT_BIN="bin"
VCPKG_ROOT="$HOME/vcpkg"

# Ensure VCPKG is available
# Optional VCPKG
CMAKE_ARGS=""
if [ -d "$VCPKG_ROOT" ]; then
    echo "Using vcpkg at $VCPKG_ROOT"
    CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
else
    echo "vcpkg not found at $VCPKG_ROOT. Attempting to use system dependencies..."
fi

# Clean previous build if requested
if [ "$1" == "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf $BUILD_DIR $OUTPUT_BIN
fi

# Create output directories
mkdir -p $OUTPUT_BIN

# Check for Metal toolchain on macOS
if [[ "$OSTYPE" == "darwin"* ]]; then
    if ! xcrun -sdk macosx metal --version &> /dev/null; then
        echo "Metal toolchain not found. Disabling Metal shaders."
        CMAKE_ARGS="$CMAKE_ARGS -DSKIP_METAL_SHADERS=ON"
    fi
fi

# Configure CMake
echo "Configuring CMake..."
cmake -S backend -B $BUILD_DIR \
    $CMAKE_ARGS \
    -DCMAKE_BUILD_TYPE=Release \
    -DMPI_ENABLED=OFF \
    -DENABLE_CUDA=OFF

# Build
echo "Building..."
cmake --build $BUILD_DIR -- -j$(nproc)

echo "Build complete. Executables are in $OUTPUT_BIN/"
