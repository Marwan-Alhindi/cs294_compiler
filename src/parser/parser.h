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

    // Type parsing
    std::string parseType();

    // Statement parsers
    AstNodePtr parseStatement();
    AstNodePtr parseFnDecl();
    AstNodePtr parseBlock();
    AstNodePtr parseLetStmt();
    AstNodePtr parseReturnStmt();
    AstNodePtr parseWhileStmt();
    AstNodePtr parseIfStmt();
    AstNodePtr parseExprStmt();
    AstNodePtr parseLoopStmt();
    AstNodePtr parseBreakStmt();
    AstNodePtr parseContinueStmt();
    AstNodePtr parseStructDef();
    AstNodePtr parseEnumDef();
    AstNodePtr parseImplBlock();

    // Expression parsers (precedence climbing)
    AstNodePtr parseExpression();
    AstNodePtr parseAssignment();
    AstNodePtr parseLogicalOr();
    AstNodePtr parseLogicalAnd();
    AstNodePtr parseComparison();
    AstNodePtr parseAdditive();
    AstNodePtr parseMultiplicative();
    AstNodePtr parseCast();
    AstNodePtr parseUnary();
    AstNodePtr parsePostfix(AstNodePtr left);
    AstNodePtr parsePrimary();
    AstNodePtr parseArrayLiteral();
};

#endif // PARSER_H
