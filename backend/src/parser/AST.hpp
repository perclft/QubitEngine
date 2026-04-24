#pragma once

#include <string>
#include <vector>
#include <memory>

namespace qubit_engine {
namespace parser {

enum class ASTNodeType {
    PROGRAM,
    GATE_DECLARATION,
    GATE_CALL,
    MEASURE,
    INCLUDE,
    QREG_DECL,
    CREG_DECL
};

struct ASTNode {
    ASTNodeType type;
    virtual ~ASTNode() = default;
};

struct ASTProgram : public ASTNode {
    std::vector<std::shared_ptr<ASTNode>> statements;
    ASTProgram() { type = ASTNodeType::PROGRAM; }
};

struct ASTGateCall : public ASTNode {
    std::string gate_name;
    std::vector<std::string> target_qubits; // using string IDs like q[0]
    std::vector<double> params;
    
    ASTGateCall() { type = ASTNodeType::GATE_CALL; }
};

struct ASTMeasure : public ASTNode {
    std::string source_qubit;
    std::string target_bit;
    
    ASTMeasure() { type = ASTNodeType::MEASURE; }
};

struct ASTQRegDecl : public ASTNode {
    std::string name;
    int size;
    
    ASTQRegDecl() { type = ASTNodeType::QREG_DECL; }
};

struct ASTCRegDecl : public ASTNode {
    std::string name;
    int size;
    
    ASTCRegDecl() { type = ASTNodeType::CREG_DECL; }
};

} // namespace parser
} // namespace qubit_engine
