// OpenQASM 3.0 Parser and Exporter
// Import and export quantum circuits in OpenQASM format

#ifndef OPENQASM_HPP
#define OPENQASM_HPP

#define _USE_MATH_DEFINES
#include <cmath>

#include <map>
#include <string>
#include <vector>

namespace qubit_engine {
namespace qasm {

// Parsed gate instruction
struct QASMGate {
  std::string name;
  std::vector<int> qubits;
  std::vector<double> params;
};

// Parsed circuit
struct QASMCircuit {
  int num_qubits;
  int num_classical;
  std::vector<QASMGate> gates;
  std::string version;
  std::map<std::string, int> qubit_map;
};

// OpenQASM Parser
class QASMParser {
public:
  QASMCircuit parse(const std::string &qasm_code);

private:
  std::string trim(const std::string &s);

  QASMGate parse_gate(const std::string &line,
                      const std::map<std::string, int> &qubit_map);
};

// OpenQASM Exporter
class QASMExporter {
public:
  std::string
  to_qasm3(int num_qubits,
           const std::vector<std::pair<std::string, std::vector<int>>> &gates,
           const std::vector<double> &params = {});

  std::string
  to_qasm2(int num_qubits,
           const std::vector<std::pair<std::string, std::vector<int>>> &gates,
           const std::vector<double> &params = {});

private:
  std::string gate_name_to_qasm(const std::string &name);
  bool is_rotation_gate(const std::string &name);
};

} // namespace qasm
} // namespace qubit_engine

#endif // OPENQASM_HPP
