#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "../lexer/lexer.h"
#include "../lexer/token.h"
#include <memory>
#include <string>
#include <vector>

// ============================================================
// ParseError
// ============================================================
struct ParseError {
    std::string message;
    int line;
};

// ============================================================
// Parser — recursive descent, one-token lookahead
// ============================================================
class Parser {
public:
    explicit Parser(const std::string& source);

    // Entry point. Returns the AST root. May be partial if hasErrors().
    std::unique_ptr<ProgramNode> parseProgram();

    bool hasErrors() const;
    const std::vector<ParseError>& errors() const;

private:
    Lexer lexer_;
    Token current_;
    Token peek_;
    std::vector<ParseError> errors_;

    // Token navigation
    void advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token expect(TokenType type, const std::string& errorMsg);

    // Error handling
    void recordError(const std::string& msg, int line);
    void synchronize();

    // Statement parsers
    AstNodePtr parseStatement();
    AstNodePtr parseFnDecl();
    AstNodePtr parseBlock();
    AstNodePtr parseLetStmt();
    AstNodePtr parseReturnStmt();
    AstNodePtr parseWhileStmt();
    AstNodePtr parseIfStmt();
    AstNodePtr parseExprStmt();

    // Expression parsers (precedence climbing, bottom-up by binding strength)
    AstNodePtr parseExpression();
    AstNodePtr parseAssignment();
    AstNodePtr parseComparison();
    AstNodePtr parseAdditive();
    AstNodePtr parseMultiplicative();
    AstNodePtr parseUnary();
    AstNodePtr parsePrimary();
};

#endif // PARSER_H
