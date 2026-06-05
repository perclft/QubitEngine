#pragma once

#include <string>
#include <vector>
#include "qubit_engine_export.h"

namespace qubit_engine {

struct QUBIT_ENGINE_EXPORT NodeDef {
    int id;
    double x;
    double y;
};

struct QUBIT_ENGINE_EXPORT EdgeDef {
    int node1;
    int node2;
};

struct QUBIT_ENGINE_EXPORT QubitCalibration {
    double t1_us;
    double t2_us;
    double readout_error;
    double gate_error_1q;
};

struct QUBIT_ENGINE_EXPORT CouplerCalibration {
    int qubit1;
    int qubit2;
    double cx_error;
    double cx_time_ns;
};

struct QUBIT_ENGINE_EXPORT DeviceCalibration {
    std::string device_name;
    int num_qubits;
    double single_qubit_gate_time_ns;
    std::vector<QubitCalibration> qubit_calibrations;
    std::vector<CouplerCalibration> coupler_calibrations;
    std::vector<NodeDef> topology_nodes;
    std::vector<EdgeDef> topology_edges;
};

class QUBIT_ENGINE_EXPORT HardwareConfig {
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
