// Unit Tests for OpenQASM Parser and Exporter — Roundtrip and Individual Tests
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "../src/OpenQASM.hpp"
#include <gtest/gtest.h>

using namespace qubit_engine::qasm;

// ===== Parser Tests =====

TEST(QASMParserTest, ParseVersion) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    qubit[2] q;
    h q[0];
  )";

  auto circuit = parser.parse(code);
  EXPECT_EQ(circuit.version, "3.0");
}

TEST(QASMParserTest, ParseQubitDeclaration) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    qubit[4] q;
  )";

  auto circuit = parser.parse(code);
  EXPECT_EQ(circuit.num_qubits, 4);
  EXPECT_EQ(circuit.qubit_map.size(), 4);
  EXPECT_EQ(circuit.qubit_map["q[0]"], 0);
  EXPECT_EQ(circuit.qubit_map["q[3]"], 3);
}

TEST(QASMParserTest, ParseBasicGates) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    qubit[2] q;
    h q[0];
    x q[1];
    cx q[0], q[1];
  )";

  auto circuit = parser.parse(code);

  ASSERT_EQ(circuit.gates.size(), 3);
  EXPECT_EQ(circuit.gates[0].name, "h");
  EXPECT_EQ(circuit.gates[0].qubits[0], 0);

  EXPECT_EQ(circuit.gates[1].name, "x");
  EXPECT_EQ(circuit.gates[1].qubits[0], 1);

  EXPECT_EQ(circuit.gates[2].name, "cx");
  ASSERT_EQ(circuit.gates[2].qubits.size(), 2);
  EXPECT_EQ(circuit.gates[2].qubits[0], 0);
  EXPECT_EQ(circuit.gates[2].qubits[1], 1);
}

TEST(QASMParserTest, ParseParameterizedGate) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    qubit[1] q;
    rz(1.5708) q[0];
  )";

  auto circuit = parser.parse(code);

  ASSERT_EQ(circuit.gates.size(), 1);
  EXPECT_EQ(circuit.gates[0].name, "rz");
  ASSERT_EQ(circuit.gates[0].params.size(), 1);
  EXPECT_NEAR(circuit.gates[0].params[0], 1.5708, 1e-4);
}

TEST(QASMParserTest, ParsePiParameter) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    qubit[1] q;
    rz(pi) q[0];
  )";

  auto circuit = parser.parse(code);

  ASSERT_EQ(circuit.gates.size(), 1);
  ASSERT_EQ(circuit.gates[0].params.size(), 1);
  EXPECT_NEAR(circuit.gates[0].params[0], M_PI, 1e-10);
}

TEST(QASMParserTest, ParseMathExpression) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    qubit[1] q;
    rz(pi/2 + 0.1 * 2) q[0];
  )";

  auto circuit = parser.parse(code);

  ASSERT_EQ(circuit.gates.size(), 1);
  ASSERT_EQ(circuit.gates[0].params.size(), 1);
  EXPECT_NEAR(circuit.gates[0].params[0], M_PI/2 + 0.2, 1e-10);
}

TEST(QASMParserTest, SkipsComments) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    // This is a comment
    qubit[1] q;
    h q[0];
  )";

  auto circuit = parser.parse(code);
  EXPECT_EQ(circuit.gates.size(), 1);
}

TEST(QASMParserTest, ParseClassicalBits) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    qubit[2] q;
    bit[2] c;
  )";

  auto circuit = parser.parse(code);
  EXPECT_EQ(circuit.num_classical, 2);
}

TEST(QASMParserTest, ParseBarrier) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    qubit[2] q;
    barrier q[0], q[1];
  )";
  auto circuit = parser.parse(code);
  ASSERT_EQ(circuit.gates.size(), 1);
  EXPECT_EQ(circuit.gates[0].name, "barrier");
  EXPECT_EQ(circuit.gates[0].qubits.size(), 2);
}

TEST(QASMParserTest, ParseReset) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    qubit[1] q;
    reset q[0];
  )";
  auto circuit = parser.parse(code);
  ASSERT_EQ(circuit.gates.size(), 1);
  EXPECT_EQ(circuit.gates[0].name, "reset");
}

TEST(QASMParserTest, ParseGateDefinition) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    gate bell q0, q1 {
        h q0;
        cx q0, q1;
    }
    qubit[2] q;
    bell q[0], q[1];
  )";
  auto circuit = parser.parse(code);
  EXPECT_EQ(circuit.gate_definitions.size(), 1);
  EXPECT_TRUE(circuit.gate_definitions.count("bell"));
  EXPECT_EQ(circuit.gates.size(), 1);
  EXPECT_EQ(circuit.gates[0].name, "bell");
}

TEST(QASMParserTest, ParseIfElse) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    qubit[1] q;
    bit[1] c;
    if (c[0] == 1) {
        x q[0];
    } else {
        h q[0];
    }
  )";
  auto circuit = parser.parse(code);
  ASSERT_EQ(circuit.gates.size(), 2);
  EXPECT_EQ(circuit.gates[0].name, "x");
  EXPECT_EQ(circuit.gates[0].condition_var, "c[0]");
  EXPECT_EQ(circuit.gates[0].condition_val, 1);
  
  EXPECT_EQ(circuit.gates[1].name, "h");
  EXPECT_EQ(circuit.gates[1].condition_var, "c[0]");
  EXPECT_EQ(circuit.gates[1].condition_val, 0);
}

// ===== Exporter Tests =====

TEST(QASMExporterTest, ExportQASM3_Header) {
  QASMExporter exporter;
  std::vector<std::pair<std::string, std::vector<int>>> gates = {};

  auto output = exporter.to_qasm3(2, gates);

  EXPECT_NE(output.find("OPENQASM 3.0"), std::string::npos);
  EXPECT_NE(output.find("qubit[2] q"), std::string::npos);
  EXPECT_NE(output.find("bit[2] c"), std::string::npos);
}

TEST(QASMExporterTest, ExportQASM3_BasicGates) {
  QASMExporter exporter;
  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"H", {0}}, {"X", {1}}, {"CNOT", {0, 1}}};

  auto output = exporter.to_qasm3(2, gates);

  EXPECT_NE(output.find("h q[0]"), std::string::npos);
  EXPECT_NE(output.find("x q[1]"), std::string::npos);
  EXPECT_NE(output.find("cx q[0], q[1]"), std::string::npos);
}

TEST(QASMExporterTest, ExportQASM3_ParameterizedGate) {
  QASMExporter exporter;
  std::vector<std::pair<std::string, std::vector<int>>> gates = {{"RZ", {0}}};
  std::vector<double> params = {1.5708};

  auto output = exporter.to_qasm3(1, gates, params);

  // Should contain rz with parameter
  EXPECT_NE(output.find("rz("), std::string::npos);
}

TEST(QASMExporterTest, ExportQASM2_Header) {
  QASMExporter exporter;
  std::vector<std::pair<std::string, std::vector<int>>> gates = {};

  auto output = exporter.to_qasm2(2, gates);

  EXPECT_NE(output.find("OPENQASM 2.0"), std::string::npos);
  EXPECT_NE(output.find("qreg q[2]"), std::string::npos);
  EXPECT_NE(output.find("creg c[2]"), std::string::npos);
}

TEST(QASMExporterTest, ExportQASM3_MeasureAll) {
  QASMExporter exporter;
  std::vector<std::pair<std::string, std::vector<int>>> gates = {};

  auto output = exporter.to_qasm3(2, gates);

  EXPECT_NE(output.find("c[0] = measure q[0]"), std::string::npos);
  EXPECT_NE(output.find("c[1] = measure q[1]"), std::string::npos);
}

TEST(QASMExporterTest, ExportQASM3_ObjectApi) {
  QASMParser parser;
  std::string code = R"(
    OPENQASM 3.0;
    qubit[1] q;
    bit[1] c;
    if (c[0] == 1) x q[0];
    barrier q[0];
    reset q[0];
  )";
  auto circuit = parser.parse(code);
  
  QASMExporter exporter;
  auto output = exporter.to_qasm3(circuit);
  
  EXPECT_NE(output.find("if (c[0] == 1) x q[0];"), std::string::npos);
  EXPECT_NE(output.find("barrier q[0];"), std::string::npos);
  EXPECT_NE(output.find("reset q[0];"), std::string::npos);
}

// ===== Roundtrip Test: Export -> Parse -> Verify =====

// Helper: find first gate by name, skipping parser artifacts like 'include'
static const QASMGate *find_gate(const QASMCircuit &c, const std::string &name,
                                 int skip = 0) {
  int found = 0;
  for (auto &g : c.gates) {
    if (g.name == name) {
      if (found == skip)
        return &g;
      found++;
    }
  }
  return nullptr;
}

TEST(QASMRoundtripTest, ExportAndParse_BasicCircuit) {
  // Export a circuit to QASM 3.0
  QASMExporter exporter;
  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"H", {0}}, {"CNOT", {0, 1}}};

  auto qasm_str = exporter.to_qasm3(2, gates);

  // Parse it back
  QASMParser parser;
  auto circuit = parser.parse(qasm_str);

  EXPECT_EQ(circuit.num_qubits, 2);

  // Find our gates by name (parser may also pick up 'include' directive)
  auto *h_gate = find_gate(circuit, "h");
  auto *cx_gate = find_gate(circuit, "cx");

  ASSERT_NE(h_gate, nullptr) << "Expected 'h' gate in parsed roundtrip";
  ASSERT_NE(cx_gate, nullptr) << "Expected 'cx' gate in parsed roundtrip";
  EXPECT_EQ(h_gate->qubits[0], 0);
  EXPECT_EQ(cx_gate->qubits[0], 0);
  EXPECT_EQ(cx_gate->qubits[1], 1);
}

TEST(QASMRoundtripTest, ExportAndParse_WithParameters) {
  QASMExporter exporter;
  std::vector<std::pair<std::string, std::vector<int>>> gates = {{"RZ", {0}},
                                                                 {"H", {0}}};
  std::vector<double> params = {1.5708};

  auto qasm_str = exporter.to_qasm3(1, gates, params);

  QASMParser parser;
  auto circuit = parser.parse(qasm_str);

  auto *rz_gate = find_gate(circuit, "rz");
  ASSERT_NE(rz_gate, nullptr) << "Expected 'rz' gate in parsed roundtrip";
  ASSERT_GE(rz_gate->params.size(), 1);
  EXPECT_NEAR(rz_gate->params[0], 1.5708, 0.01);
}

// ===== Gate Name Mapping =====

TEST(QASMExporterTest, GateNameMapping) {
  QASMExporter exporter;

  // SWAP and CZ should map correctly
  std::vector<std::pair<std::string, std::vector<int>>> gates = {
      {"SWAP", {0, 1}}, {"CZ", {0, 1}}, {"S", {0}}, {"T", {0}}};

  auto output = exporter.to_qasm3(2, gates);

  EXPECT_NE(output.find("swap"), std::string::npos);
  EXPECT_NE(output.find("cz"), std::string::npos);
  EXPECT_NE(output.find("s q[0]"), std::string::npos);
  EXPECT_NE(output.find("t q[0]"), std::string::npos);
}
