#pragma once

#include "AST.hpp"
#include <string>
#include <vector>
#include "qubit_engine_export.h"

namespace qubit_engine {
namespace parser {

struct QUBIT_ENGINE_EXPORT Token {
    enum Type {
        IDENTIFIER,
        NUMBER,
        PUNCTUATION, // ;, ,, (, ), [, ]
        KEYWORD,
        STRING,
        END_OF_FILE
    };
    Type type;
    std::string value;
};

class QUBIT_ENGINE_EXPORT QASMParser {
public:
    explicit QASMParser(const std::string& source);

    std::shared_ptr<ASTProgram> parse();

private:
    std::vector<Token> tokens_;
    size_t current_ = 0;

    void tokenize(const std::string& source);
    Token advance();
    Token peek() const;
    Token peek(int offset) const;
    bool match(Token::Type type, const std::string& value);
    void consume(Token::Type type, const std::string& value, const std::string& err);
    bool isAtEnd() const;

    std::shared_ptr<ASTNode> parseStatement();
    std::shared_ptr<ASTQRegDecl> parseQReg();
    std::shared_ptr<ASTCRegDecl> parseCReg();
    std::shared_ptr<ASTGateCall> parseGateCall();
    std::shared_ptr<ASTMeasure> parseMeasure();
    std::shared_ptr<ASTBarrier> parseBarrier();
    std::shared_ptr<ASTReset> parseReset();
    std::shared_ptr<ASTBlock> parseBlock();
    std::shared_ptr<ASTIfStmt> parseIfStmt();
    std::shared_ptr<ASTGateDefinition> parseGateDefinition();
};

} // namespace parser
} // namespace qubit_engine
