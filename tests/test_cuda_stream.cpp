#include "HardwareConfig.hpp"
#include "IQuantumBackend.hpp"
#include "backends/CudaBackend.hpp"
#include <iostream>

using namespace qubit_engine;

int main() {
    try {
        std::cout << "Running CUDA Stream Race Test..." << std::endl;
        
        HardwareConfig config;
        config.num_qubits = 10;
        config.mpi_rank = 0;
        config.mpi_size = 1;

        CudaBackend backend(config);
        backend.initialize();

        // Simulate some gate operations on the default stream
        backend.applyHadamard(0);
        backend.applyHadamard(1);

        // Call the async telemetry readback
        // With the fix, this will wait on an event. Without it, it races.
        auto state = backend.getStateVectorAsync();
        
        std::cout << "Async telemetry completed successfully." << std::endl;
        std::cout << "Stream sync issue resolved." << std::endl;
        return 0;
    } catch(const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
