// OpenQASM 3.0 Parser and Exporter
// Import and export quantum circuits in OpenQASM format

#ifndef OPENQASM_HPP
#define OPENQASM_HPP

#define _USE_MATH_DEFINES
#include <cmath>

#include <map>
#include <string>
#include <vector>
#include "qubit_engine_export.h"

namespace qubit_engine {
namespace qasm {

// Parsed gate instruction
struct QUBIT_ENGINE_EXPORT QASMGate {
  std::string name;
  std::vector<int> qubits;
  std::vector<double> params;
  std::string condition_var = "";
  int condition_val = 0;
};

// Parsed gate definition
struct QUBIT_ENGINE_EXPORT QASMGateDef {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> qubits;
    std::vector<QASMGate> body;
};

// Parsed circuit
struct QUBIT_ENGINE_EXPORT QASMCircuit {
  int num_qubits;
  int num_classical;
  std::vector<QASMGate> gates;
  std::string version;
  std::map<std::string, int> qubit_map;
  std::map<std::string, QASMGateDef> gate_definitions;
};

// OpenQASM Parser
class QUBIT_ENGINE_EXPORT QASMParser {
public:
  QASMCircuit parse(const std::string &qasm_code);
};

// OpenQASM Exporter
class QUBIT_ENGINE_EXPORT QASMExporter {
public:
  std::string
  to_qasm3(int num_qubits,
           const std::vector<std::pair<std::string, std::vector<int>>> &gates,
           const std::vector<double> &params = {});
           
  std::string to_qasm3(const QASMCircuit &circuit);

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
