#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(const std::string& source);
    Token nextToken();
    std::vector<Token> tokenize();

private:
    std::string source;
    size_t pos;
    int line;

    char peekChar() const;
    char advance();
    void skipWhitespace();
    void skipComments();
    Token readIdentifier();
    Token readNumber();
    Token readString();
};

#endif // LEXER_H
