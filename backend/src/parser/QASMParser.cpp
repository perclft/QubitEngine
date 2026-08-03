#include "QASMParser.hpp"
#include <stdexcept>
#include <cctype>

namespace qubit_engine {
namespace parser {

QASMParser::QASMParser(const std::string& source) {
    tokenize(source);
}

void QASMParser::tokenize(const std::string& source) {
    size_t i = 0;
    while (i < source.length()) {
        char c = source[i];
        if (std::isspace(c)) {
            i++;
            continue;
        }

        // Comments
        if (c == '/' && i + 1 < source.length() && source[i+1] == '/') {
            while (i < source.length() && source[i] != '\n') i++;
            continue;
        }

        if (std::isalpha(c) || c == '_') {
            std::string val;
            while (i < source.length() && (std::isalnum(source[i]) || source[i] == '_')) {
                val += source[i++];
            }
            if (val == "qreg" || val == "creg" || val == "qubit" || val == "bit" || val == "measure" || val == "include" || val == "gate" || val == "if" || val == "else" || val == "barrier" || val == "reset") {
                tokens_.push_back({Token::KEYWORD, val});
            } else {
                tokens_.push_back({Token::IDENTIFIER, val});
            }
            continue;
        }

        if (c == ';' || c == ',' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '+' || c == '*' || c == '/' || c == '-' || c == '>' || c == '=' || c == '!') {
            // handle "->"
            if (c == '-' && i + 1 < source.length() && source[i+1] == '>') {
                tokens_.push_back({Token::PUNCTUATION, "->"});
                i += 2;
                continue;
            }
            if (c == '=' && i + 1 < source.length() && source[i+1] == '=') {
                tokens_.push_back({Token::PUNCTUATION, "=="});
                i += 2;
                continue;
            }
            if (c == '!' && i + 1 < source.length() && source[i+1] == '=') {
                tokens_.push_back({Token::PUNCTUATION, "!="});
                i += 2;
                continue;
            }
            tokens_.push_back({Token::PUNCTUATION, std::string(1, c)});
            i++;
            continue;
        }

        if (std::isdigit(c) || c == '.') {
            std::string val;
            while (i < source.length() && (std::isdigit(source[i]) || source[i] == '.')) {
                val += source[i++];
            }
            tokens_.push_back({Token::NUMBER, val});
            continue;
        }

        i++; // skip unknown
    }
    tokens_.push_back({Token::END_OF_FILE, ""});
}

Token QASMParser::advance() {
    if (!isAtEnd()) current_++;
    return tokens_[current_ - 1];
}

Token QASMParser::peek() const {
    if (isAtEnd()) return tokens_.back();
    return tokens_[current_];
}

Token QASMParser::peek(int offset) const {
    if (current_ + offset >= tokens_.size()) return tokens_.back();
    return tokens_[current_ + offset];
}

bool QASMParser::match(Token::Type type, const std::string& value) {
    if (isAtEnd()) return false;
    if (peek().type == type && (value.empty() || peek().value == value)) {
        advance();
        return true;
    }
    return false;
}

void QASMParser::consume(Token::Type type, const std::string& value, const std::string& err) {
    if (match(type, value)) return;
    throw std::runtime_error("QASM Parse Error: " + err);
}

bool QASMParser::isAtEnd() const {
    return current_ >= tokens_.size() || tokens_[current_].type == Token::END_OF_FILE;
}

std::shared_ptr<ASTProgram> QASMParser::parse() {
    auto program = std::make_shared<ASTProgram>();
    while (!isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) {
            program->statements.push_back(stmt);
        }
    }
    return program;
}

struct RecursionGuard {
    size_t& depth;
    RecursionGuard(size_t& d) : depth(d) {
        depth++;
        if (depth > 100) {
            throw std::runtime_error("QASM Parse Error: maximum nesting depth exceeded (100)");
        }
    }
    ~RecursionGuard() {
        if (depth > 0) depth--;
    }
};

std::shared_ptr<ASTNode> QASMParser::parseStatement() {
    RecursionGuard guard(recursion_depth_);
    if (match(Token::KEYWORD, "include")) {
        while (!isAtEnd() && (peek().type != Token::PUNCTUATION || peek().value != ";")) {
            advance();
        }
        consume(Token::PUNCTUATION, ";", "Expected ';' after include");
        return nullptr; // Ignore includes
    }
    
    if (match(Token::KEYWORD, "qreg") || match(Token::KEYWORD, "qubit")) return parseQReg();
    if (match(Token::KEYWORD, "creg") || match(Token::KEYWORD, "bit")) return parseCReg();
    if (match(Token::KEYWORD, "measure")) return parseMeasure();
    if (match(Token::KEYWORD, "barrier")) return parseBarrier();
    if (match(Token::KEYWORD, "reset")) return parseReset();
    if (match(Token::KEYWORD, "if")) return parseIfStmt();
    if (match(Token::KEYWORD, "gate")) return parseGateDefinition();
    if (peek().type == Token::PUNCTUATION && peek().value == "{") return parseBlock();
    
    if (peek().type == Token::IDENTIFIER) {
        if (peek().value == "OPENQASM") {
            advance(); advance(); consume(Token::PUNCTUATION, ";", "Expected ;");
            return nullptr;
        }
        // Could be an assignment: c[i] = measure q[i];
        // Or a gate call: h q[0];
        if (peek(1).type == Token::PUNCTUATION && (peek(1).value == "[" || peek(1).value == "=")) {
            // Peek further to see if there's an '='
            bool is_assignment = false;
            size_t i = 0;
            while (true) {
                if (peek(i).type == Token::PUNCTUATION && peek(i).value == "=") {
                    is_assignment = true;
                    break;
                }
                if (peek(i).type == Token::END_OF_FILE || (peek(i).type == Token::PUNCTUATION && peek(i).value == ";")) {
                    break;
                }
                i++;
                if (i > 15) break; // safety
            }
            if (is_assignment) {
                // Parse assignment (skip for now, or handle specifically for measure)
                // c[i] = measure q[i];
                auto clbit = advance().value;
                if (match(Token::PUNCTUATION, "[")) {
                    clbit += "[" + advance().value + "]";
                    consume(Token::PUNCTUATION, "]", "Expected ']'");
                }
                consume(Token::PUNCTUATION, "=", "Expected '='");
                if (match(Token::KEYWORD, "measure")) {
                    auto m = std::make_shared<ASTMeasure>();
                    m->target_bit = clbit;
                    m->source_qubit = advance().value;
                    if (match(Token::PUNCTUATION, "[")) {
                        m->source_qubit += "[" + advance().value + "]";
                        consume(Token::PUNCTUATION, "]", "Expected ']'");
                    }
                    consume(Token::PUNCTUATION, ";", "Expected ';'");
                    return m;
                } else {
                    throw std::runtime_error("QASM Parse Error: Expected 'measure' after '=' in assignment");
                }
            }
        }
        return parseGateCall();
    }
    
    advance(); // fallback
    return nullptr;
}

std::shared_ptr<ASTQRegDecl> QASMParser::parseQReg() {
    auto decl = std::make_shared<ASTQRegDecl>();
    if (peek().type == Token::PUNCTUATION && peek().value == "[") {
        // qubit[size] name;
        consume(Token::PUNCTUATION, "[", "Expected '['");
        int sz = std::stoi(advance().value);
        if (sz <= 0 || sz > 10000) {
            throw std::runtime_error("QASM Parse Error: qubit register size out of bounds (1..10000)");
        }
        decl->size = sz;
        consume(Token::PUNCTUATION, "]", "Expected ']'");
        decl->name = advance().value;
    } else {
        // qreg name[size];
        decl->name = advance().value;
        consume(Token::PUNCTUATION, "[", "Expected '['");
        int sz = std::stoi(advance().value);
        if (sz <= 0 || sz > 10000) {
            throw std::runtime_error("QASM Parse Error: qubit register size out of bounds (1..10000)");
        }
        decl->size = sz;
        consume(Token::PUNCTUATION, "]", "Expected ']'");
    }
    consume(Token::PUNCTUATION, ";", "Expected ';'");
    return decl;
}

std::shared_ptr<ASTCRegDecl> QASMParser::parseCReg() {
    auto decl = std::make_shared<ASTCRegDecl>();
    if (peek().type == Token::PUNCTUATION && peek().value == "[") {
        // bit[size] name;
        consume(Token::PUNCTUATION, "[", "Expected '['");
        int sz = std::stoi(advance().value);
        if (sz <= 0 || sz > 10000) {
            throw std::runtime_error("QASM Parse Error: classical register size out of bounds (1..10000)");
        }
        decl->size = sz;
        consume(Token::PUNCTUATION, "]", "Expected ']'");
        decl->name = advance().value;
    } else {
        // creg name[size];
        decl->name = advance().value;
        consume(Token::PUNCTUATION, "[", "Expected '['");
        int sz = std::stoi(advance().value);
        if (sz <= 0 || sz > 10000) {
            throw std::runtime_error("QASM Parse Error: classical register size out of bounds (1..10000)");
        }
        decl->size = sz;
        consume(Token::PUNCTUATION, "]", "Expected ']'");
    }
    consume(Token::PUNCTUATION, ";", "Expected ';'");
    return decl;
}

// Recursive descent math parser for expressions like pi/2
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
      return 3.14159265358979323846;
    }

    // Try parsing number
    size_t end;
    double val = 0.0;
    try {
      val = std::stod(text.substr(pos), &end);
      pos += end;
      skip_whitespace();
    } catch (...) {
      pos++;
      skip_whitespace();
      val = 0.0;
    }
    return val;
  }
};

std::shared_ptr<ASTGateCall> QASMParser::parseGateCall() {
    gate_count_++;
    if (gate_count_ > 1000000) {
        throw std::runtime_error("QASM Parse Error: maximum gate count exceeded (1,000,000)");
    }
    auto call = std::make_shared<ASTGateCall>();
    call->gate_name = advance().value;
    
    // Parse params if '('
    if (match(Token::PUNCTUATION, "(")) {
        if (!match(Token::PUNCTUATION, ")")) {
            do {
                // Collect expression string until ',' or ')'
                std::string expr;
                int parens = 0;
                while (!isAtEnd()) {
                    if (peek().type == Token::PUNCTUATION && peek().value == "(") parens++;
                    else if (peek().type == Token::PUNCTUATION && peek().value == ")") {
                        if (parens == 0) break;
                        parens--;
                    } else if (peek().type == Token::PUNCTUATION && peek().value == "," && parens == 0) {
                        break;
                    }
                    expr += advance().value;
                }
                MathParser p(expr);
                call->params.push_back(p.parse_expression());
            } while (match(Token::PUNCTUATION, ","));
            consume(Token::PUNCTUATION, ")", "Expected ')'");
        }
    }
    
    // Parse target qubits
    do {
        std::string q = advance().value;
        if (match(Token::PUNCTUATION, "[")) {
            q += "[" + advance().value + "]";
            consume(Token::PUNCTUATION, "]", "Expected ']'");
        }
        call->target_qubits.push_back(q);
    } while (match(Token::PUNCTUATION, ","));
    
    consume(Token::PUNCTUATION, ";", "Expected ';' after gate call");
    return call;
}

std::shared_ptr<ASTMeasure> QASMParser::parseMeasure() {
    auto m = std::make_shared<ASTMeasure>();
    std::string sq = advance().value;
    if (match(Token::PUNCTUATION, "[")) {
        sq += "[" + advance().value + "]";
        consume(Token::PUNCTUATION, "]", "Expected ']'");
    }
    m->source_qubit = sq;
    
    consume(Token::PUNCTUATION, "->", "Expected '->'");
    
    std::string tq = advance().value;
    if (match(Token::PUNCTUATION, "[")) {
        tq += "[" + advance().value + "]";
        consume(Token::PUNCTUATION, "]", "Expected ']'");
    }
    m->target_bit = tq;
    
    consume(Token::PUNCTUATION, ";", "Expected ';'");
    return m;
}

std::shared_ptr<ASTBarrier> QASMParser::parseBarrier() {
    auto b = std::make_shared<ASTBarrier>();
    if (peek().type == Token::IDENTIFIER || (peek().type == Token::KEYWORD && peek().value == "qreg")) {
        do {
            if (peek().type == Token::KEYWORD && peek().value == "qreg") {
                advance(); // some parsers allow 'barrier qreg;'
                b->qubits.push_back(advance().value);
            } else {
                std::string q = advance().value;
                if (match(Token::PUNCTUATION, "[")) {
                    q += "[" + advance().value + "]";
                    consume(Token::PUNCTUATION, "]", "Expected ']'");
                }
                b->qubits.push_back(q);
            }
        } while (match(Token::PUNCTUATION, ","));
    }
    consume(Token::PUNCTUATION, ";", "Expected ';'");
    return b;
}

std::shared_ptr<ASTReset> QASMParser::parseReset() {
    auto r = std::make_shared<ASTReset>();
    std::string q = advance().value;
    if (match(Token::PUNCTUATION, "[")) {
        q += "[" + advance().value + "]";
        consume(Token::PUNCTUATION, "]", "Expected ']'");
    }
    r->qubit = q;
    consume(Token::PUNCTUATION, ";", "Expected ';'");
    return r;
}

std::shared_ptr<ASTBlock> QASMParser::parseBlock() {
    auto block = std::make_shared<ASTBlock>();
    consume(Token::PUNCTUATION, "{", "Expected '{'");
    while (!isAtEnd() && !(peek().type == Token::PUNCTUATION && peek().value == "}")) {
        auto stmt = parseStatement();
        if (stmt) block->statements.push_back(stmt);
    }
    consume(Token::PUNCTUATION, "}", "Expected '}'");
    return block;
}

std::shared_ptr<ASTIfStmt> QASMParser::parseIfStmt() {
    auto if_stmt = std::make_shared<ASTIfStmt>();
    consume(Token::PUNCTUATION, "(", "Expected '('");
    if_stmt->condition_var = advance().value;
    if (match(Token::PUNCTUATION, "[")) {
        if_stmt->condition_var += "[" + advance().value + "]";
        consume(Token::PUNCTUATION, "]", "Expected ']'");
    }
    consume(Token::PUNCTUATION, "==", "Expected '=='");
    if_stmt->condition_value = std::stoi(advance().value);
    consume(Token::PUNCTUATION, ")", "Expected ')'");
    
    if (peek().type == Token::PUNCTUATION && peek().value == "{") {
        if_stmt->then_block = parseBlock();
    } else {
        auto b = std::make_shared<ASTBlock>();
        auto stmt = parseStatement();
        if (stmt) b->statements.push_back(stmt);
        if_stmt->then_block = b;
    }
    
    if (match(Token::KEYWORD, "else")) {
        if (peek().type == Token::PUNCTUATION && peek().value == "{") {
            if_stmt->else_block = parseBlock();
        } else {
            auto b = std::make_shared<ASTBlock>();
            auto stmt = parseStatement();
            if (stmt) b->statements.push_back(stmt);
            if_stmt->else_block = b;
        }
    }
    return if_stmt;
}

std::shared_ptr<ASTGateDefinition> QASMParser::parseGateDefinition() {
    auto def = std::make_shared<ASTGateDefinition>();
    def->name = advance().value;
    if (match(Token::PUNCTUATION, "(")) {
        if (!match(Token::PUNCTUATION, ")")) {
            do {
                def->params.push_back(advance().value);
            } while (match(Token::PUNCTUATION, ","));
            consume(Token::PUNCTUATION, ")", "Expected ')'");
        }
    }
    do {
        def->qubits.push_back(advance().value);
    } while (match(Token::PUNCTUATION, ","));
    
    def->body = parseBlock();
    return def;
}

} // namespace parser
} // namespace qubit_engine
