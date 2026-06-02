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

struct QubitCalibration {
    double t1_us;
    double t2_us;
    double readout_error;
    double gate_error_1q;
};

struct CouplerCalibration {
    int qubit1;
    int qubit2;
    double cx_error;
    double cx_time_ns;
};

struct DeviceCalibration {
    std::string device_name;
    int num_qubits;
    double single_qubit_gate_time_ns;
    std::vector<QubitCalibration> qubit_calibrations;
    std::vector<CouplerCalibration> coupler_calibrations;
    std::vector<NodeDef> topology_nodes;
    std::vector<EdgeDef> topology_edges;
};

class HardwareConfig {
public:
    HardwareConfig() = default;
    
    // Load config from a JSON file path
    bool loadFromFile(const std::string& filepath);
    
    // Fallback/Default Mock Lattice (if no file is specified/found)
    void loadDefaultHeavyHex();

    // Preset Hardware Calibrations
    static DeviceCalibration ibmBrisbane();
    static DeviceCalibration googleSycamore();
    static DeviceCalibration genericDevice(int n);

    const std::vector<NodeDef>& getNodes() const { return nodes; }
    const std::vector<EdgeDef>& getEdges() const { return edges; }

    void setCalibration(const DeviceCalibration& cal);
    double getEdgeErrorRate(int node1, int node2) const;
    double getQubitReadoutError(int node) const;
    bool hasCalibration() const { return has_calibration; }

private:
    std::vector<NodeDef> nodes;
    std::vector<EdgeDef> edges;
    DeviceCalibration calibration;
    bool has_calibration = false;
};

} // namespace qubit_engine
