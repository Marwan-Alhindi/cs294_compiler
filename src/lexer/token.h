#ifndef TOKEN_H
#define TOKEN_H

#include <string>

enum class TokenType {
    // Keywords
    FN, LET, MUT, IF, ELSE, WHILE, RETURN,
    TRUE, FALSE, LOOP, BREAK, CONTINUE,
    STRUCT, ENUM, IMPL, AS, CONST,
    SELF_LOWER,  // self

    // Literals
    IDENT, NUMBER, STRING,

    // Operators
    PLUS, MINUS, STAR, SLASH, PERCENT,
    ASSIGN, EQ, NEQ,
    LT, GT, LTE, GTE,
    PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN, PERCENT_ASSIGN,
    CARET_ASSIGN, PIPE_ASSIGN,
    AND_AND, OR_OR, NOT,
    SHL, SHR,            // <<  >>
    SHL_ASSIGN, SHR_ASSIGN, // <<=  >>=
    ARROW,       // ->
    COLON_COLON, // ::
    DOT,         // .

    // Punctuation
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    SEMICOLON, COLON, COMMA,
    AMP,         // &
    PIPE,        // |
    CARET,       // ^

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
