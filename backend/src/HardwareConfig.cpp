#include "HardwareConfig.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace qubit_engine {

bool HardwareConfig::loadFromFile(const std::string& filepath) {
    try {
        std::ifstream f(filepath);
        if (!f.is_open()) {
            spdlog::warn("Failed to open hardware topology file: {}", filepath);
            return false;
        }

        json data = json::parse(f);
        
        nodes.clear();
        edges.clear();

        for (const auto& n : data["nodes"]) {
            nodes.push_back({
                n["id"].get<int>(),
                n["x"].get<double>(),
                n["y"].get<double>()
            });
        }

        for (const auto& e : data["edges"]) {
            edges.push_back({
                e["node1"].get<int>(),
                e["node2"].get<int>()
            });
        }
        
        spdlog::info("Successfully loaded topology from {} ({} nodes, {} edges)", filepath, nodes.size(), edges.size());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error parsing topology JSON {}: {}", filepath, e.what());
        return false;
    }
}

void HardwareConfig::loadDefaultHeavyHex() {
    spdlog::info("Falling back to default Heavy-Hex Lattice");
    nodes = {
        {0, 20.0, 0.0}, {1, 40.0, 0.0}, {2, 60.0, 0.0},
        {3, 40.0, 20.0},
        {4, 0.0, 40.0}, {5, 20.0, 40.0}, {6, 40.0, 40.0}, {7, 60.0, 40.0}, {8, 80.0, 40.0},
        {9, 20.0, 60.0}, {10, 60.0, 60.0},
        {11, 0.0, 80.0}, {12, 20.0, 80.0}, {13, 40.0, 80.0}, {14, 60.0, 80.0}, {15, 80.0, 80.0}
    };
    
    // IBM Guadalupe Heavy-Hex Layout Edges
    edges = {
        {0, 1}, {1, 2}, {1, 3}, {3, 6}, {4, 5}, {5, 6},
        {6, 7}, {7, 8}, {5, 9}, {9, 12}, {7, 10}, {10, 14},
        {11, 12}, {12, 13}, {13, 14}, {14, 15}
    };
}

} // namespace qubit_engine

namespace qubit_engine {

DeviceCalibration HardwareConfig::ibmBrisbane() {
    DeviceCalibration cal;
    cal.device_name = "IBM Brisbane (Eagle r3)";
    cal.num_qubits = 127;
    cal.single_qubit_gate_time_ns = 35.0; // Typical SX gate time
    
    // Median values
    double median_t1 = 260.0;
    double median_t2 = 175.0;
    double median_readout = 0.014; // 1.4%
    double median_1q_error = 0.00024; // 0.024%
    
    for (int i = 0; i < cal.num_qubits; ++i) {
        cal.qubit_calibrations.push_back({
            median_t1,
            median_t2,
            median_readout,
            median_1q_error
        });
        cal.topology_nodes.push_back({i, (double)(i % 10), (double)(i / 10)}); // Dummy generic grid
    }
    
    // Simple line connectivity for mock (full HeavyHex would be large)
    for (int i = 0; i < cal.num_qubits - 1; ++i) {
        cal.coupler_calibrations.push_back({
            i, i + 1,
            0.0076, // 0.76% median 2Q error
            500.0   // 500ns ECR time
        });
        cal.topology_edges.push_back({i, i + 1});
    }
    return cal;
}

DeviceCalibration HardwareConfig::googleSycamore() {
    DeviceCalibration cal;
    cal.device_name = "Google Sycamore";
    cal.num_qubits = 53;
    cal.single_qubit_gate_time_ns = 25.0;
    
    for (int i = 0; i < cal.num_qubits; ++i) {
        cal.qubit_calibrations.push_back({
            15.0,   // 15us
            12.0,   // 12us
            0.038,  // 3.8%
            0.0015  // 0.15%
        });
        cal.topology_nodes.push_back({i, (double)(i % 10), (double)(i / 10)});
    }
    
    for (int i = 0; i < cal.num_qubits - 1; ++i) {
        cal.coupler_calibrations.push_back({
            i, i + 1,
            0.006,  // 0.6%
            32.0    // 32ns
        });
        cal.topology_edges.push_back({i, i + 1});
    }
    return cal;
}

DeviceCalibration HardwareConfig::genericDevice(int n) {
    DeviceCalibration cal;
    cal.device_name = "Generic " + std::to_string(n) + "Q";
    cal.num_qubits = n;
    cal.single_qubit_gate_time_ns = 50.0;
    
    for (int i = 0; i < n; ++i) {
        cal.qubit_calibrations.push_back({50.0, 30.0, 0.01, 0.001});
        cal.topology_nodes.push_back({i, (double)(i % 5), (double)(i / 5)});
    }
    
    for (int i = 0; i < n - 1; ++i) {
        cal.coupler_calibrations.push_back({i, i + 1, 0.01, 100.0});
        cal.topology_edges.push_back({i, i + 1});
    }
    return cal;
}

} // namespace qubit_engine
