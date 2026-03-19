#pragma once

#include <string>
#include <vector>

namespace qubit_engine {

struct NodeDef {
    int id;
    double x;
    double y;
};

struct EdgeDef {
    int node1;
    int node2;
};

class HardwareConfig {
public:
    HardwareConfig() = default;
    
    // Load config from a JSON file path
    bool loadFromFile(const std::string& filepath);
    
    // Fallback/Default Mock Lattice (if no file is specified/found)
    void loadDefaultHeavyHex();

    const std::vector<NodeDef>& getNodes() const { return nodes; }
    const std::vector<EdgeDef>& getEdges() const { return edges; }

private:
    std::vector<NodeDef> nodes;
    std::vector<EdgeDef> edges;
};

} // namespace qubit_engine
