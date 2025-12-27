#!/bin/bash
set -e

# 1. Cleanup old processes
echo "Cleaning up old processes..."
pkill -9 -f qubit_engine || true
sleep 2

# 2. Dependencies Check (Arch Linux)
echo "Checking for MPI..."
if ! command -v mpirun &> /dev/null; then
    echo "mpirun could not be found."
    echo "Please install OpenMPI: sudo pacman -S openmpi"
    exit 1
fi

# 4. Rebuild with MPI
echo "Rebuilding Backend with MPI support..."
cd backend
rm -rf build_mpi
mkdir build_mpi && cd build_mpi

# Use vcpkg toolchain
VCPKG_ROOT=""
if [ -d "$HOME/vcpkg" ]; then
    VCPKG_ROOT="$HOME/vcpkg"
elif [ -d "../../vcpkg" ]; then
    VCPKG_ROOT="../../vcpkg"
elif [ -d "../vcpkg" ]; then
    VCPKG_ROOT="../vcpkg"
fi

CMAKE_FLAGS="-DENABLE_CUDA=ON -DMPI_ENABLED=ON"

if [ -n "$VCPKG_ROOT" ]; then
    echo "Found vcpkg at $VCPKG_ROOT"
    CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
else
    echo "Warning: vcpkg not found. Relying on system libraries."
fi

cmake .. $CMAKE_FLAGS

make -j$(nproc)

# 5. Run Verification (C++ Binary)
echo "Running C++ MPI Verification..."
mpirun -n 2 ./mpi_test && echo "MPI Verification SUCCESS" || echo "MPI Verification FAILED"

# 3. Run Distributed Server
echo "Starting Distributed QubitEngine (Rank 0 = Server)..."
# Launch 2 processes. Rank 0 will bind to port 50051. Rank 1 will wait.
# We run this in the background.
mpirun -n 2 ./qubit_engine &
SERVER_PID=$!

echo "Waiting for server to start..."
sleep 5

# 4. Run Verification Client
echo "Running Python Verification Client..."
cd ../.. # Back to root
python3 verify_mpi.py

# 5. Cleanup
echo "Stopping Server (PID: $SERVER_PID)..."
kill $SERVER_PID || true
wait $SERVER_PID || true
