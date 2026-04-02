#include "OpenQASM.hpp"

#include <sstream>
#include <cctype>

// Centralized M_PI used

namespace qubit_engine {
namespace qasm {

// --- QASMParser ---

namespace {

// Helper to check if char is a valid identifier start
bool is_id_start(char c) {
  return std::isalpha(c) || c == '_';
}

// Helper for identifier body
bool is_id_char(char c) {
  return std::isalnum(c) || c == '_';
}

// Recursive descent math parser
class MathParser {
public:
  MathParser(const std::string& str) : text(str), pos(0) {
    skip_whitespace();
  }

  double parse_expression() {
    double val = parse_term();
    while (true) {
      if (match('+')) val += parse_term();
      else if (match('-')) val -= parse_term();
      else break;
    }
    return val;
  }

private:
  std::string text;
  size_t pos;

  void skip_whitespace() {
    while (pos < text.size() && std::isspace(text[pos])) pos++;
  }

  bool match(char c) {
    if (pos < text.size() && text[pos] == c) {
      pos++;
      skip_whitespace();
      return true;
    }
    return false;
  }

  double parse_term() {
    double val = parse_factor();
    while (true) {
      if (match('*')) val *= parse_factor();
      else if (match('/')) val /= parse_factor();
      else break;
    }
    return val;
  }

  double parse_factor() {
    if (match('+')) return parse_factor();
    if (match('-')) return -parse_factor();
    if (match('(')) {
      double val = parse_expression();
      match(')'); // Expect closing paren
      return val;
    }

    // Try parsing 'pi'
    if (pos + 1 < text.size() && text[pos] == 'p' && text[pos+1] == 'i') {
      pos += 2;
      skip_whitespace();
      return M_PI;
    }

    // Try parsing number
    size_t end;
    double val = 0.0;
    try {
      val = std::stod(text.substr(pos), &end);
      pos += end;
      skip_whitespace();
    } catch (...) {
      // Return 0 on parsing failure and advance character to avoid infinite loop
      pos++;
      skip_whitespace();
      val = 0.0;
    }
    return val;
  }
};

} // namespace

QASMCircuit QASMParser::parse(const std::string &qasm_code) {
  QASMCircuit circuit;
  circuit.version = "3.0";
  circuit.num_qubits = 0;
  circuit.num_classical = 0;

  std::istringstream stream(qasm_code);
  std::string line;

  while (std::getline(stream, line)) {
    line = trim(line);
    if (line.empty() || (line[0] == '/' && line[1] == '/'))
      continue;

    // manual tokenizer for the line
    size_t i = 0;
    auto skip_ws = [&]() { while(i < line.size() && std::isspace(line[i])) i++; };
    auto read_id = [&]() {
      size_t start = i;
      while(i < line.size() && is_id_char(line[i])) i++;
      return line.substr(start, i - start);
    };

    skip_ws();
    if (i >= line.size()) continue;

    std::string kw = read_id();
    
    if (kw == "OPENQASM") {
      skip_ws();
      size_t start = i;
      while(i < line.size() && (std::isdigit(line[i]) || line[i] == '.')) i++;
      if (start < i) {
        circuit.version = line.substr(start, i - start);
      }
    } else if (kw == "qubit" || kw == "qreg") {
      skip_ws();
      if (i < line.size() && line[i] == '[') {
        i++;
        skip_ws();
        size_t start = i;
        while(i < line.size() && std::isdigit(line[i])) i++;
        int size = std::stoi(line.substr(start, i - start));
        skip_ws();
        if (i < line.size() && line[i] == ']') i++;
        skip_ws();
        std::string name = read_id();
        for (int q = 0; q < size; q++) {
          circuit.qubit_map[name + "[" + std::to_string(q) + "]"] = circuit.num_qubits++;
        }
      }
    } else if (kw == "bit" || kw == "creg") {
      skip_ws();
      if (i < line.size() && line[i] == '[') {
        i++;
        skip_ws();
        size_t start = i;
        while(i < line.size() && std::isdigit(line[i])) i++;
        int size = std::stoi(line.substr(start, i - start));
        circuit.num_classical = size;
      }
    } else if (kw == "include") {
      // ignore
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

std::string QASMParser::trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  size_t end = s.find_last_not_of(" \t\n\r;");
  return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

QASMGate QASMParser::parse_gate(const std::string &line,
                                const std::map<std::string, int> &qubit_map) {
  QASMGate gate;
  size_t i = 0;
  auto skip_ws = [&]() { while(i < line.size() && std::isspace(line[i])) i++; };
  
  skip_ws();
  if (i >= line.size()) return gate;

  // Read gate name
  size_t start = i;
  while(i < line.size() && is_id_char(line[i])) i++;
  gate.name = line.substr(start, i - start);

  skip_ws();
  
  // Parse parameters if '('
  if (i < line.size() && line[i] == '(') {
    i++; // Skip '('
    size_t end_paren = line.find(')', i);
    if (end_paren != std::string::npos) {
      std::string params_str = line.substr(i, end_paren - i);
      i = end_paren + 1;
      
      // Tokenize by comma and parse each math expression
      size_t p_start = 0;
      while (p_start < params_str.size()) {
        size_t comma = params_str.find(',', p_start);
        std::string expr;
        if (comma == std::string::npos) {
          expr = params_str.substr(p_start);
          p_start = params_str.size();
        } else {
          expr = params_str.substr(p_start, comma - p_start);
          p_start = comma + 1;
        }
        MathParser p(expr);
        gate.params.push_back(p.parse_expression());
      }
    }
  }

  // Parse qubits (e.g. q[0], q[1])
  while (i < line.size() && line[i] != ';') {
    skip_ws();
    if (i >= line.size() || line[i] == ';') break;

    if (line[i] == ',') {
      i++;
      continue;
    }

    // Read qubit register name
    start = i;
    while(i < line.size() && is_id_char(line[i])) i++;
    std::string q_name = line.substr(start, i - start);
    
    // If no ID char was found and we haven't advanced, we must advance
    // to avoid an infinite loop on malformed gates
    if (start == i && i < line.size() && line[i] != '[' && line[i] != ',' && line[i] != ';') {
        i++;
        continue;
    }

    skip_ws();
    if (i < line.size() && line[i] == '[') {
      i++;
      skip_ws();
      start = i;
      while(i < line.size() && std::isdigit(line[i])) i++;
      std::string idx = line.substr(start, i - start);
      skip_ws();
      if (i < line.size() && line[i] == ']') i++;
      
      std::string full_q_name = q_name + "[" + idx + "]";
      auto it = qubit_map.find(full_q_name);
      if (it != qubit_map.end()) {
        gate.qubits.push_back(it->second);
      }
    }
    
    skip_ws();
    if (i < line.size() && line[i] == ',') i++; // Skip comma
  }

  return gate;
}

// --- QASMExporter ---

std::string QASMExporter::to_qasm3(
    int num_qubits,
    const std::vector<std::pair<std::string, std::vector<int>>> &gates,
    const std::vector<double> &params) {
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

std::string QASMExporter::to_qasm2(
    int num_qubits,
    const std::vector<std::pair<std::string, std::vector<int>>> &gates,
    const std::vector<double> &params) {
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

std::string QASMExporter::gate_name_to_qasm(const std::string &name) {
  static std::map<std::string, std::string> mapping = {
      {"HADAMARD", "h"}, {"H", "h"},      {"PAULI_X", "x"}, {"X", "x"},
      {"PAULI_Y", "y"},  {"Y", "y"},      {"PAULI_Z", "z"}, {"Z", "z"},
      {"CNOT", "cx"},    {"CX", "cx"},    {"CZ", "cz"},     {"SWAP", "swap"},
      {"S", "s"},        {"S_GATE", "s"}, {"T", "t"},       {"T_GATE", "t"},
      {"RZ", "rz"},      {"RX", "rx"},    {"RY", "ry"},     {"PHASE", "p"}};

  auto it = mapping.find(name);
  return (it != mapping.end()) ? it->second : name;
}

bool QASMExporter::is_rotation_gate(const std::string &name) {
  return name == "RZ" || name == "RX" || name == "RY" || name == "PHASE" ||
         name == "rz" || name == "rx" || name == "ry" || name == "p";
}

} // namespace qasm
} // namespace qubit_engine
