#!/bin/bash
# script to automate QubitEngine benchmarks

set -e

# Change to project root
cd "$(dirname "$0")/.."
PROJECT_ROOT=$(pwd)

OUTPUT_FILE="$PROJECT_ROOT/benchmarks/benchmark_results.md"
DATE=$(date +"%Y-%m-%d")

# Detect OS and hardware
OS=$(uname -s)
CPU_INFO=""
GPU_INFO=""

if [ "$OS" = "Darwin" ]; then
    OS_NAME="macOS"
    CPU_INFO=$(sysctl -n machdep.cpu.brand_string)
    GPU_INFO=$(system_profiler SPDisplaysDataType | grep "Chipset Model" | awk -F': ' '{print $2}' | head -n 1)
    if [ -z "$GPU_INFO" ]; then
        GPU_INFO="Apple Silicon (Integrated)"
    fi
    BACKEND="Metal Backend (GPU)"
elif [ "$OS" = "Linux" ]; then
    OS_NAME="Linux"
    if [ -f /proc/cpuinfo ]; then
        CPU_INFO=$(grep "model name" /proc/cpuinfo | head -n 1 | awk -F': ' '{print $2}')
    fi
    if command -v nvidia-smi &> /dev/null; then
        GPU_INFO=$(nvidia-smi --query-gpu=name --format=csv,noheader | head -n 1)
        BACKEND="Cuda Backend (GPU)"
    else
        GPU_INFO="None / CPU Only"
        BACKEND="Cpu Backend"
    fi
else
    OS_NAME="Windows"
    CPU_INFO="Unknown CPU"
    GPU_INFO="Unknown GPU"
    BACKEND="AVX2"
fi

echo "Starting QubitEngine Benchmark Suite..."
echo "Detected Environment: $OS_NAME | $CPU_INFO | $GPU_INFO"

# Setup the output file
echo "Appending to $OUTPUT_FILE..."
echo "" >> "$OUTPUT_FILE"
echo "## $OS_NAME Performance ($BACKEND)" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"
echo "**Date:** $DATE" >> "$OUTPUT_FILE"
echo "**Environment:** $OS_NAME ($CPU_INFO) - $BACKEND" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# 1. Run Python Benchmarks
echo "Running Python Benchmarks (Scaling, Throughput, Entanglement)..."

# Ensure Python can find qubit_engine
cp "$PROJECT_ROOT/bin/core."*".so" "$PROJECT_ROOT/python/qubit_engine/core.so" 2>/dev/null || true
export PYTHONPATH="$PROJECT_ROOT/python:$PYTHONPATH"
PYTHON_CMD="python3 python/tests/benchmark_suite.py"

# Capture the output, strip the ASCII headers/footers the python script adds
$PYTHON_CMD | awk '
BEGIN { capture=0; }
/^# SCALING TEST/ { capture=1; print "### 1. Scaling Test: State Vector Size vs Performance\n"; next }
/^# GATE THROUGHPUT TEST/ { capture=1; print "\n### 2. Gate Throughput Test\n"; next }
/^# ENTANGLEMENT BENCHMARK/ { capture=1; print "\n### 3. Entanglement Benchmarks\n"; next }
/^# RANDOM CIRCUIT BENCHMARK/ { capture=1; print "\n### 4. Random Circuit Benchmark\n"; next }
/^# STRESS TEST/ { capture=1; print "\n### 5. Stress Test: Maximum Qubit Count\n"; next }
/^MetalBackend::/ { next }
/^QuantumRegister:/ { next }
/^= QubitEngine Performance/ { capture=0; next }
/^=+/ { next }
/Benchmark Complete/ { capture=0; next }
{ if(capture) print $0 }
' >> "$OUTPUT_FILE"

# 2. Run C++ Memory Wall Benchmark
if [ ! -d "$PROJECT_ROOT/benchmarks/build" ]; then
    echo "Building C++ Memory Wall Benchmark..."
    mkdir -p "$PROJECT_ROOT/benchmarks/build"
    cd "$PROJECT_ROOT/benchmarks/build"
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="/Users/sahil/vcpkg/scripts/buildsystems/vcpkg.cmake" > /dev/null
    make memory_benchmark > /dev/null
    cd "$PROJECT_ROOT"
fi

CPP_BENCH="$PROJECT_ROOT/benchmarks/build/memory_benchmark"

if [ -f "$CPP_BENCH" ]; then
    echo "Running C++ Memory Wall Benchmark..."
    echo "" >> "$OUTPUT_FILE"
    echo "### 6. C++ Memory Wall Benchmark (Bandwidth)" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
    echo "\`\`\`" >> "$OUTPUT_FILE"
    $CPP_BENCH --benchmark_format=console 2>&1 | grep -v -E "MetalBackend::|QuantumRegister:" >> "$OUTPUT_FILE"
    echo "\`\`\`" >> "$OUTPUT_FILE"
else
    echo "Warning: C++ benchmark executable not found at $CPP_BENCH"
    echo "Skipping C++ Memory Wall Benchmark."
fi

echo ""
echo "Benchmarks completed successfully! Results appended to $OUTPUT_FILE."
