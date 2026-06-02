#include "../src/transpiler/Router.hpp"
#include "../src/HardwareConfig.hpp"
#include <gtest/gtest.h>

using namespace qubit_engine;
using namespace qubit_engine::transpiler;
using Gate = QuantumRegister::RecordedGate;

// Helper to build a gate
static Gate makeGate(Gate::Type t, std::vector<size_t> qubits,
                     std::vector<double> params = {}) {
  return {t, qubits, params};
}

TEST(NoiseAwareTranspilerTest, DijkstraPrefersHighFidelityPath) {
    // Construct a mock hardware topology:
    // We want to route a gate between Q0 and Q3.
    // Path 1 (Short but Noisy): Q0 -> Q1 -> Q3
    // Path 2 (Long but Pristine): Q0 -> Q2 -> Q4 -> Q3
    
    DeviceCalibration mock_cal;
    mock_cal.num_qubits = 5;
    
    // Add nodes
    for (int i = 0; i < 5; ++i) {
        mock_cal.topology_nodes.push_back({i, (double)i, 0.0});
    }
    
    // Add edges
    mock_cal.topology_edges.push_back({0, 1});
    mock_cal.topology_edges.push_back({1, 3});
    
    mock_cal.topology_edges.push_back({0, 2});
    mock_cal.topology_edges.push_back({2, 4});
    mock_cal.topology_edges.push_back({4, 3});
    
    // Add noisy edges
    mock_cal.coupler_calibrations.push_back({0, 1, 0.50, 100.0}); // 50% error
    mock_cal.coupler_calibrations.push_back({1, 3, 0.50, 100.0}); // 50% error
    
    // Add pristine edges
    mock_cal.coupler_calibrations.push_back({0, 2, 0.001, 100.0}); // 0.1% error
    mock_cal.coupler_calibrations.push_back({2, 4, 0.001, 100.0}); // 0.1% error
    mock_cal.coupler_calibrations.push_back({4, 3, 0.001, 100.0}); // 0.1% error
    
    HardwareConfig config;
    config.setCalibration(mock_cal);
    
    Router router(config);
    
    std::vector<Gate> tape = {
        makeGate(Gate::CNOT, {0, 3})
    };
    
    auto routed_tape = router.route(tape);
    
    // The routed tape should contain SWAPs along the pristine path (0->2->4->3).
    // Let's count SWAPs or trace where it went.
    // CNOT(0, 3) needs to bring 0 and 3 adjacent.
    // If it used Path 1, it would do 1 SWAP (e.g., SWAP(0,1) then CNOT(1,3) or SWAP(1,3) then CNOT(0,1)).
    // If it used Path 2, it would do 2 SWAPs (e.g., SWAP(0,2), SWAP(2,4) then CNOT(4,3)).
    
    int swap_count = 0;
    for (const auto& g : routed_tape) {
        if (g.type == Gate::SWAP) {
            swap_count++;
        }
    }
    
    // Since Path 2 requires 2 SWAPs, and Path 1 requires 1 SWAP,
    // if the transpiler is noise-aware, it will choose Path 2, meaning 2 SWAPs instead of 1.
    EXPECT_EQ(swap_count, 2) << "Router should have chosen the longer pristine path (2 SWAPs)";
}
