#include "parser/QASMParser.hpp"
#include "OpenQASM.hpp"

#include <sstream>
#include <cctype>

// Centralized M_PI used

namespace qubit_engine {
namespace qasm {

QASMCircuit QASMParser::parse(const std::string &qasm_code) {
  qubit_engine::parser::QASMParser ast_parser(qasm_code);
  auto ast = ast_parser.parse();

  QASMCircuit circuit;
  circuit.version = "3.0";
  circuit.num_qubits = 0;
  circuit.num_classical = 0;

  auto process_stmt = [&](const std::shared_ptr<qubit_engine::parser::ASTNode>& stmt, const std::string& cond_var, int cond_val, auto& self) -> void {
      if (auto qreg = std::dynamic_pointer_cast<qubit_engine::parser::ASTQRegDecl>(stmt)) {
          for (int i = 0; i < qreg->size; ++i) {
              circuit.qubit_map[qreg->name + "[" + std::to_string(i) + "]"] = circuit.num_qubits++;
          }
      } else if (auto creg = std::dynamic_pointer_cast<qubit_engine::parser::ASTCRegDecl>(stmt)) {
          circuit.num_classical += creg->size;
      } else if (auto call = std::dynamic_pointer_cast<qubit_engine::parser::ASTGateCall>(stmt)) {
          QASMGate gate;
          gate.name = call->gate_name;
          gate.params = call->params;
          gate.condition_var = cond_var;
          gate.condition_val = cond_val;
          for (const auto& q : call->target_qubits) {
              if (circuit.qubit_map.count(q)) {
                  gate.qubits.push_back(circuit.qubit_map[q]);
              }
          }
          circuit.gates.push_back(gate);
      } else if (auto measure = std::dynamic_pointer_cast<qubit_engine::parser::ASTMeasure>(stmt)) {
          QASMGate gate;
          gate.name = "measure";
          gate.condition_var = cond_var;
          gate.condition_val = cond_val;
          if (circuit.qubit_map.count(measure->source_qubit)) {
              gate.qubits.push_back(circuit.qubit_map[measure->source_qubit]);
          }
          circuit.gates.push_back(gate);
      } else if (auto barrier = std::dynamic_pointer_cast<qubit_engine::parser::ASTBarrier>(stmt)) {
          QASMGate gate;
          gate.name = "barrier";
          for (const auto& q : barrier->qubits) {
              if (circuit.qubit_map.count(q)) {
                  gate.qubits.push_back(circuit.qubit_map[q]);
              }
          }
          circuit.gates.push_back(gate);
      } else if (auto reset = std::dynamic_pointer_cast<qubit_engine::parser::ASTReset>(stmt)) {
          QASMGate gate;
          gate.name = "reset";
          gate.condition_var = cond_var;
          gate.condition_val = cond_val;
          if (circuit.qubit_map.count(reset->qubit)) {
              gate.qubits.push_back(circuit.qubit_map[reset->qubit]);
          }
          circuit.gates.push_back(gate);
      } else if (auto if_stmt = std::dynamic_pointer_cast<qubit_engine::parser::ASTIfStmt>(stmt)) {
          if (if_stmt->then_block) {
              for (auto s : if_stmt->then_block->statements) self(s, if_stmt->condition_var, if_stmt->condition_value, self);
          }
          if (if_stmt->else_block) {
              // Inverse logic for else (c == 0 if condition was c == 1)
              int else_val = (if_stmt->condition_value == 0) ? 1 : 0;
              for (auto s : if_stmt->else_block->statements) self(s, if_stmt->condition_var, else_val, self);
          }
      } else if (auto gdef = std::dynamic_pointer_cast<qubit_engine::parser::ASTGateDefinition>(stmt)) {
          QASMGateDef d;
          d.name = gdef->name;
          d.params = gdef->params;
          d.qubits = gdef->qubits;
          circuit.gate_definitions[gdef->name] = d;
      }
  };

  for (const auto& stmt : ast->statements) {
      process_stmt(stmt, "", 0, process_stmt);
  }

  return circuit;
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

std::string QASMExporter::to_qasm3(const QASMCircuit &circuit) {
    std::ostringstream out;
    out << "OPENQASM 3.0;\n";
    out << "include \"stdgates.inc\";\n\n";
    out << "qubit[" << circuit.num_qubits << "] q;\n";
    if (circuit.num_classical > 0) {
        out << "bit[" << circuit.num_classical << "] c;\n";
    }
    out << "\n";
    
    // Gate definitions
    for (const auto& [name, def] : circuit.gate_definitions) {
        out << "gate " << name;
        if (!def.params.empty()) {
            out << "(";
            for (size_t i = 0; i < def.params.size(); ++i) {
                if (i > 0) out << ", ";
                out << def.params[i];
            }
            out << ")";
        }
        out << " ";
        for (size_t i = 0; i < def.qubits.size(); ++i) {
            if (i > 0) out << ", ";
            out << def.qubits[i];
        }
        out << " {\n  // body skipped in exporter for now\n}\n\n";
    }

    for (const auto& gate : circuit.gates) {
        if (!gate.condition_var.empty()) {
            out << "if (" << gate.condition_var << " == " << gate.condition_val << ") ";
        }
        if (gate.name == "measure") {
            // Assume simple measurement mapping
            out << "c[" << gate.qubits[0] << "] = measure q[" << gate.qubits[0] << "];\n";
            continue;
        }
        out << gate_name_to_qasm(gate.name);
        if (!gate.params.empty()) {
            out << "(";
            for (size_t i = 0; i < gate.params.size(); ++i) {
                if (i > 0) out << ", ";
                out << gate.params[i];
            }
            out << ")";
        }
        if (!gate.qubits.empty()) {
            out << " ";
            for (size_t i = 0; i < gate.qubits.size(); ++i) {
                if (i > 0) out << ", ";
                out << "q[" << gate.qubits[i] << "]";
            }
        }
        out << ";\n";
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
