#ifndef OPENQASM3_PARSER_HPP
#define OPENQASM3_PARSER_HPP

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include "OpenQASM.hpp"

namespace qubit_engine {
namespace qasm3 {

// --- AST Nodes ---

struct Expression;
using ExprPtr = std::shared_ptr<Expression>;

struct Expression {
    virtual ~Expression() = default;
};

struct BinaryExpr : Expression {
    std::string op;
    ExprPtr left, right;
};

struct LiteralExpr : Expression {
    double value;
};

struct IdentifierExpr : Expression {
    std::string name;
};

// --- Statement Types ---

struct Statement {
    virtual ~Statement() = default;
};

struct GateStmt : Statement {
    std::string name;
    std::vector<ExprPtr> params;
    std::vector<std::string> qubits;
};

struct DeclarationStmt : Statement {
    std::string type; // "qubit", "bit", "int", "float"
    std::string name;
    int size;
};

struct IfStmt : Statement {
    ExprPtr condition;
    std::vector<std::shared_ptr<Statement>> body;
    std::vector<std::shared_ptr<Statement>> else_body;
};

struct ForStmt : Statement {
    std::string index_var;
    int start, end;
    std::vector<std::shared_ptr<Statement>> body;
};

struct GateDefinition : Statement {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> qubits;
    std::vector<std::shared_ptr<Statement>> body;
};

// --- Parser ---

class OpenQASM3Parser {
public:
    explicit OpenQASM3Parser(const std::string& source) : source_(source), pos_(0) {}

    // Main entry point
    std::vector<std::shared_ptr<Statement>> parse();

private:
    std::string source_;
    size_t pos_;

    // Helper: Basic tokenization (scaffolded)
    std::string next_token();
    bool match(const std::string& expected);
    
    // Recursive descent parser methods (scaffolded)
    std::shared_ptr<Statement> parse_statement();
    std::shared_ptr<Statement> parse_gate_stmt();
    std::shared_ptr<Statement> parse_declaration();
    std::shared_ptr<Statement> parse_if();
    std::shared_ptr<Statement> parse_for();
    std::shared_ptr<Statement> parse_gate_def();

    ExprPtr parse_expression();
};

} // namespace qasm3
} // namespace qubit_engine

#endif // OPENQASM3_PARSER_HPP
