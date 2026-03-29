#ifndef TOKEN_H
#define TOKEN_H

#include <string>

enum class TokenType {
    // Keywords
    FN, LET, MUT, IF, ELSE, WHILE, RETURN,

    // Literals
    IDENT, NUMBER, STRING,

    // Operators
    PLUS, MINUS, STAR, SLASH,
    ASSIGN, EQ, NEQ,
    LT, GT, LTE, GTE,

    // Punctuation
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    SEMICOLON, COLON, COMMA,

    // Special
    EOF_TOKEN,
    ILLEGAL
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
};

std::string tokenTypeToString(TokenType type);
TokenType lookupKeyword(const std::string& ident);

#endif // TOKEN_H
