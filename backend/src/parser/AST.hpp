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
    CREG_DECL,
    BARRIER,
    RESET,
    IF_STMT,
    BLOCK,
    GATE_DEFINITION
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

struct ASTBarrier : public ASTNode {
    std::vector<std::string> qubits;
    ASTBarrier() { type = ASTNodeType::BARRIER; }
};

struct ASTReset : public ASTNode {
    std::string qubit;
    ASTReset() { type = ASTNodeType::RESET; }
};

struct ASTBlock : public ASTNode {
    std::vector<std::shared_ptr<ASTNode>> statements;
    ASTBlock() { type = ASTNodeType::BLOCK; }
};

struct ASTIfStmt : public ASTNode {
    std::string condition_var;
    int condition_value;
    std::shared_ptr<ASTBlock> then_block;
    std::shared_ptr<ASTBlock> else_block; // optional
    
    ASTIfStmt() { type = ASTNodeType::IF_STMT; }
};

struct ASTGateDefinition : public ASTNode {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> qubits;
    std::shared_ptr<ASTBlock> body;
    
    ASTGateDefinition() { type = ASTNodeType::GATE_DEFINITION; }
};

} // namespace parser
} // namespace qubit_engine
