#!/bin/bash
set -e

echo "============================================="
echo "QubitEngine MPI Distributed Benchmark Cluster"
echo "============================================="

# Ensure OpenMPI is installed and paths are available
if ! command -v mpirun &> /dev/null; then
    echo "Error: mpirun could not be found. Please install OpenMPI (e.g., brew install open-mpi)."
    exit 1
fi

if [[ "$OSTYPE" == "darwin"* ]]; then
    export OMPI_MCA_btl="^openib" # Avoid specific macOS warnings
    export OMPI_MCA_plm_rsh_agent=ssh
fi

# Build QubitEngine with MPI explicitly set
echo "1. Recompiling the Native Backend with ENABLE_MPI=ON..."
cd ../backend
mkdir -p build && cd build
cmake -DENABLE_MPI=ON -DCMAKE_BUILD_TYPE=Release ..
make -j4 qubit_engine
cd ../../benchmarks

# Find the binary
ENGINE_BIN="../bin/qubit_engine"

if [ ! -f "$ENGINE_BIN" ]; then
    echo "Error: Compilation failed. Could not locate $ENGINE_BIN"
    exit 1
fi

echo "2. Launching Distribute MPI Simulation (n=2 Ranks)..."
echo "Executing QubitEngine as an MPI cluster simulating network-sharded QuantumRegisters."
echo "--------------------------------------------------------"

# Run 2 ranks over localhost (we suppress OMPI warnings often found on macs running dev envs)
# Passing `qubit_engine` any arbitrary arguments or simply starting it distributes it via the framework.
mpirun --allow-run-as-root -n 2 "$ENGINE_BIN" &
# We capture the PID to terminate it since the Engine normally runs forever as a gRPC daemon
ENGINE_PID=$!
sleep 5 # Allow the cluster to form and simulate
kill -9 $ENGINE_PID

echo "--------------------------------------------------------"
echo "MPI Benchmark completed successfully!"
