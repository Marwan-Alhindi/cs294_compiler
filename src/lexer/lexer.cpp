#include "lexer.h"

Lexer::Lexer(const std::string& source)
    : source(source), pos(0), line(1) {}

char Lexer::peekChar() const {
    if (pos < source.length()) {
        return source[pos];
    }
    return '\0';
}

char Lexer::advance() {
    if (pos < source.length()) {
        char c = source[pos];
        pos++;
        return c;
    }
    return '\0';
}

void Lexer::skipWhitespace() {
    while (pos < source.length()) {
        char c = peekChar();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (c == '\n') {
                line++;
            }
            advance();
        } else {
            break;
        }
    }
}

void Lexer::skipComments() {
    if (pos + 1 < source.length() && source[pos] == '/') {
        if (source[pos + 1] == '/') {
            // Single-line comment
            while (pos < source.length() && peekChar() != '\n') {
                advance();
            }
        } else if (source[pos + 1] == '*') {
            // Multi-line comment
            advance(); // skip /
            advance(); // skip *
            while (pos + 1 <= source.length()) {
                if (peekChar() == '\0') {
                    break;
                }
                if (peekChar() == '\n') {
                    line++;
                }
                if (peekChar() == '*' && pos + 1 < source.length() && source[pos + 1] == '/') {
                    advance(); // skip *
                    advance(); // skip /
                    break;
                }
                advance();
            }
        }
    }
}

Token Lexer::readIdentifier() {
    size_t start = pos;
    while (pos < source.length() && (std::isalnum(peekChar()) || peekChar() == '_')) {
        advance();
    }
    std::string lexeme = source.substr(start, pos - start);
    TokenType type = lookupKeyword(lexeme);
    return Token{type, lexeme, line};
}

Token Lexer::readNumber() {
    size_t start = pos;
    while (pos < source.length() && std::isdigit(peekChar())) {
        advance();
    }
    std::string lexeme = source.substr(start, pos - start);
    // Skip optional type suffix (e.g., u32, i32, usize)
    if (pos < source.length() && std::isalpha(peekChar())) {
        while (pos < source.length() && (std::isalnum(peekChar()) || peekChar() == '_')) {
            advance();
        }
    }
    return Token{TokenType::NUMBER, lexeme, line};
}

Token Lexer::readString() {
    advance(); // skip opening "
    size_t start = pos;
    while (pos < source.length() && peekChar() != '"') {
        if (peekChar() == '\n') {
            line++;
        }
        advance();
    }
    if (pos >= source.length()) {
        // Unterminated string
        return Token{TokenType::ILLEGAL, source.substr(start, pos - start), line};
    }
    std::string lexeme = source.substr(start, pos - start);
    advance(); // skip closing "
    return Token{TokenType::STRING, lexeme, line};
}

Token Lexer::nextToken() {
    // Skip whitespace and comments in a loop
    while (true) {
        skipWhitespace();
        size_t before = pos;
        skipComments();
        if (pos == before) {
            break;
        }
    }

    if (pos >= source.length()) {
        return Token{TokenType::EOF_TOKEN, "", line};
    }

    char c = peekChar();

    if (std::isalpha(c) || c == '_') {
        return readIdentifier();
    }
    if (std::isdigit(c)) {
        return readNumber();
    }
    if (c == '"') {
        return readString();
    }
    // Character literal: 'x' or '\n' etc
    if (c == '\'') {
        size_t start = pos;   // pos points at the opening '
        advance();            // skip opening '
        if (pos < source.length() && peekChar() == '\\') {
            advance(); // skip backslash
            advance(); // skip escaped char
        } else if (pos < source.length()) {
            advance(); // skip the character
        }
        if (pos < source.length() && peekChar() == '\'') {
            advance(); // skip closing '
            std::string lexeme = source.substr(start, pos - start);
            return Token{TokenType::NUMBER, lexeme, line};
        }
        // Not a valid char literal — treat as number anyway
        std::string lexeme = source.substr(start, pos - start);
        return Token{TokenType::NUMBER, lexeme, line};
    }

    advance();
    switch (c) {
        case '+':
            if (peekChar() == '=') {
                advance();
                return Token{TokenType::PLUS_ASSIGN, "+=", line};
            }
            return Token{TokenType::PLUS, "+", line};
        case '-':
            if (peekChar() == '>') {
                advance();
                return Token{TokenType::ARROW, "->", line};
            }
            if (peekChar() == '=') {
                advance();
                return Token{TokenType::MINUS_ASSIGN, "-=", line};
            }
            return Token{TokenType::MINUS, "-", line};
        case '*':
            if (peekChar() == '=') {
                advance();
                return Token{TokenType::STAR_ASSIGN, "*=", line};
            }
            return Token{TokenType::STAR, "*", line};
        case '/':
            if (peekChar() == '=') {
                advance();
                return Token{TokenType::SLASH_ASSIGN, "/=", line};
            }
            return Token{TokenType::SLASH, "/", line};
        case '%':
            if (peekChar() == '=') {
                advance();
                return Token{TokenType::PERCENT_ASSIGN, "%=", line};
            }
            return Token{TokenType::PERCENT, "%", line};
        case '=':
            if (peekChar() == '=') {
                advance();
                return Token{TokenType::EQ, "==", line};
            }
            return Token{TokenType::ASSIGN, "=", line};
        case '!':
            if (peekChar() == '=') {
                advance();
                return Token{TokenType::NEQ, "!=", line};
            }
            return Token{TokenType::NOT, "!", line};
        case '<':
            if (peekChar() == '<') {
                advance();
                if (peekChar() == '=') {
                    advance();
                    return Token{TokenType::SHL_ASSIGN, "<<=", line};
                }
                return Token{TokenType::SHL, "<<", line};
            }
            if (peekChar() == '=') {
                advance();
                return Token{TokenType::LTE, "<=", line};
            }
            return Token{TokenType::LT, "<", line};
        case '>':
            if (peekChar() == '>') {
                advance();
                if (peekChar() == '=') {
                    advance();
                    return Token{TokenType::SHR_ASSIGN, ">>=", line};
                }
                return Token{TokenType::SHR, ">>", line};
            }
            if (peekChar() == '=') {
                advance();
                return Token{TokenType::GTE, ">=", line};
            }
            return Token{TokenType::GT, ">", line};
        case '&':
            if (peekChar() == '&') {
                advance();
                return Token{TokenType::AND_AND, "&&", line};
            }
            return Token{TokenType::AMP, "&", line};
        case '|':
            if (peekChar() == '|') {
                advance();
                return Token{TokenType::OR_OR, "||", line};
            }
            if (peekChar() == '=') {
                advance();
                return Token{TokenType::PIPE_ASSIGN, "|=", line};
            }
            return Token{TokenType::PIPE, "|", line};
        case '^':
            if (peekChar() == '=') {
                advance();
                return Token{TokenType::CARET_ASSIGN, "^=", line};
            }
            return Token{TokenType::CARET, "^", line};
        case ':':
            if (peekChar() == ':') {
                advance();
                return Token{TokenType::COLON_COLON, "::", line};
            }
            return Token{TokenType::COLON, ":", line};
        case '.': return Token{TokenType::DOT, ".", line};
        case '(': return Token{TokenType::LPAREN, "(", line};
        case ')': return Token{TokenType::RPAREN, ")", line};
        case '{': return Token{TokenType::LBRACE, "{", line};
        case '}': return Token{TokenType::RBRACE, "}", line};
        case '[': return Token{TokenType::LBRACKET, "[", line};
        case ']': return Token{TokenType::RBRACKET, "]", line};
        case ';': return Token{TokenType::SEMICOLON, ";", line};
        case ',': return Token{TokenType::COMMA, ",", line};
        default:
            return Token{TokenType::ILLEGAL, std::string(1, c), line};
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token tok = nextToken();
        tokens.push_back(tok);
        if (tok.type == TokenType::EOF_TOKEN) {
            break;
        }
    }
    return tokens;
}
