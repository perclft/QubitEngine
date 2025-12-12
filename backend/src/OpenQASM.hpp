// OpenQASM 3.0 Parser and Exporter
// Import and export quantum circuits in OpenQASM format

#ifndef OPENQASM_HPP
#define OPENQASM_HPP

#include <map>
#include <regex>
#include <sstream>
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
  QASMCircuit parse(const std::string &qasm_code) {
    QASMCircuit circuit;
    circuit.version = "3.0";
    circuit.num_qubits = 0;
    circuit.num_classical = 0;

    std::istringstream stream(qasm_code);
    std::string line;

    while (std::getline(stream, line)) {
      line = trim(line);
      if (line.empty() || line[0] == '/')
        continue;

      if (line.find("OPENQASM") != std::string::npos) {
        // Version declaration
        std::regex version_re("OPENQASM\\s+([\\d.]+)");
        std::smatch match;
        if (std::regex_search(line, match, version_re)) {
          circuit.version = match[1];
        }
      } else if (line.find("qubit") != std::string::npos) {
        // Qubit declaration: qubit[4] q;
        std::regex qubit_re("qubit\\[(\\d+)\\]\\s+(\\w+)");
        std::smatch match;
        if (std::regex_search(line, match, qubit_re)) {
          int size = std::stoi(match[1]);
          std::string name = match[2];
          for (int i = 0; i < size; i++) {
            circuit.qubit_map[name + "[" + std::to_string(i) + "]"] =
                circuit.num_qubits++;
          }
        }
      } else if (line.find("bit") != std::string::npos) {
        // Classical bit declaration
        std::regex bit_re("bit\\[(\\d+)\\]");
        std::smatch match;
        if (std::regex_search(line, match, bit_re)) {
          circuit.num_classical = std::stoi(match[1]);
        }
      } else {
        // Gate instruction
        QASMGate gate = parse_gate(line, circuit.qubit_map);
        if (!gate.name.empty()) {
          circuit.gates.push_back(gate);
        }
      }
    }

    return circuit;
  }

private:
  std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r;");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
  }

  QASMGate parse_gate(const std::string &line,
                      const std::map<std::string, int> &qubit_map) {
    QASMGate gate;

    // Match gate with optional parameters: gate_name(params) qubit1, qubit2
    std::regex gate_re("(\\w+)(?:\\(([^)]+)\\))?\\s+(.+)");
    std::smatch match;

    if (!std::regex_match(line, match, gate_re)) {
      return gate;
    }

    gate.name = match[1];

    // Parse parameters
    if (match[2].matched) {
      std::string params_str = match[2];
      std::regex param_re("([\\d.e+-]+|pi)");
      std::sregex_iterator it(params_str.begin(), params_str.end(), param_re);
      std::sregex_iterator end;
      for (; it != end; ++it) {
        std::string p = (*it)[1];
        if (p == "pi") {
          gate.params.push_back(M_PI);
        } else {
          gate.params.push_back(std::stod(p));
        }
      }
    }

    // Parse qubits
    std::string qubits_str = match[3];
    std::regex qubit_ref_re("(\\w+\\[\\d+\\])");
    std::sregex_iterator qit(qubits_str.begin(), qubits_str.end(),
                             qubit_ref_re);
    std::sregex_iterator qend;
    for (; qit != qend; ++qit) {
      std::string qubit_name = (*qit)[1];
      auto it = qubit_map.find(qubit_name);
      if (it != qubit_map.end()) {
        gate.qubits.push_back(it->second);
      }
    }

    return gate;
  }
};

// OpenQASM Exporter
class QASMExporter {
public:
  std::string
  to_qasm3(int num_qubits,
           const std::vector<std::pair<std::string, std::vector<int>>> &gates,
           const std::vector<double> &params = {}) {
    std::ostringstream out;

    out << "OPENQASM 3.0;\n";
    out << "include \"stdgates.inc\";\n\n";
    out << "qubit[" << num_qubits << "] q;\n";
    out << "bit[" << num_qubits << "] c;\n\n";

    size_t param_idx = 0;
    for (const auto &[name, qubits] : gates) {
      out << gate_name_to_qasm(name);

      // Add parameters if it's a rotation gate
      if (is_rotation_gate(name) && param_idx < params.size()) {
        out << "(" << params[param_idx++] << ")";
      }

      out << " ";
      for (size_t i = 0; i < qubits.size(); i++) {
        if (i > 0)
          out << ", ";
        out << "q[" << qubits[i] << "]";
      }
      out << ";\n";
    }

    out << "\n// Measure all qubits\n";
    for (int i = 0; i < num_qubits; i++) {
      out << "c[" << i << "] = measure q[" << i << "];\n";
    }

    return out.str();
  }

  std::string
  to_qasm2(int num_qubits,
           const std::vector<std::pair<std::string, std::vector<int>>> &gates,
           const std::vector<double> &params = {}) {
    std::ostringstream out;

    out << "OPENQASM 2.0;\n";
    out << "include \"qelib1.inc\";\n\n";
    out << "qreg q[" << num_qubits << "];\n";
    out << "creg c[" << num_qubits << "];\n\n";

    size_t param_idx = 0;
    for (const auto &[name, qubits] : gates) {
      out << gate_name_to_qasm(name);

      if (is_rotation_gate(name) && param_idx < params.size()) {
        out << "(" << params[param_idx++] << ")";
      }

      out << " ";
      for (size_t i = 0; i < qubits.size(); i++) {
        if (i > 0)
          out << ",";
        out << "q[" << qubits[i] << "]";
      }
      out << ";\n";
    }

    out << "\nmeasure q -> c;\n";

    return out.str();
  }

private:
  std::string gate_name_to_qasm(const std::string &name) {
    static std::map<std::string, std::string> mapping = {
        {"HADAMARD", "h"}, {"H", "h"},      {"PAULI_X", "x"}, {"X", "x"},
        {"PAULI_Y", "y"},  {"Y", "y"},      {"PAULI_Z", "z"}, {"Z", "z"},
        {"CNOT", "cx"},    {"CX", "cx"},    {"CZ", "cz"},     {"SWAP", "swap"},
        {"S", "s"},        {"S_GATE", "s"}, {"T", "t"},       {"T_GATE", "t"},
        {"RZ", "rz"},      {"RX", "rx"},    {"RY", "ry"},     {"PHASE", "p"}};

    auto it = mapping.find(name);
    return (it != mapping.end()) ? it->second : name;
  }

  bool is_rotation_gate(const std::string &name) {
    return name == "RZ" || name == "RX" || name == "RY" || name == "PHASE" ||
           name == "rz" || name == "rx" || name == "ry" || name == "p";
  }
};

} // namespace qasm
} // namespace qubit_engine

#endif // OPENQASM_HPP
