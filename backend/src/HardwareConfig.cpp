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
