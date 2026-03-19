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
        {0, 40.0, 20.0}, {1, 60.0, 20.0}, {2, 80.0, 20.0},
        {3, 90.0, 30.0}, {4, 100.0, 40.0}, {5, 90.0, 50.0},
        {6, 80.0, 60.0}, {7, 60.0, 60.0}, {8, 40.0, 60.0},
        {9, 30.0, 50.0}, {10, 20.0, 40.0}, {11, 30.0, 30.0},
        {12, 30.0, 10.0}, {13, 90.0, 10.0}, {14, 120.0, 40.0}, {15, 0.0, 40.0}
    };
    
    edges = {
        {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6},
        {6, 7}, {7, 8}, {8, 9}, {9, 10}, {10, 11}, {11, 0},
        {0, 12}, {2, 13}, {4, 14}, {10, 15}
    };
}

} // namespace qubit_engine
