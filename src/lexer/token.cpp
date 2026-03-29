#include "token.h"
#include <unordered_map>

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::FN:        return "FN";
        case TokenType::LET:       return "LET";
        case TokenType::MUT:       return "MUT";
        case TokenType::IF:        return "IF";
        case TokenType::ELSE:      return "ELSE";
        case TokenType::WHILE:     return "WHILE";
        case TokenType::RETURN:    return "RETURN";
        case TokenType::IDENT:     return "IDENT";
        case TokenType::NUMBER:    return "NUMBER";
        case TokenType::STRING:    return "STRING";
        case TokenType::PLUS:      return "PLUS";
        case TokenType::MINUS:     return "MINUS";
        case TokenType::STAR:      return "STAR";
        case TokenType::SLASH:     return "SLASH";
        case TokenType::ASSIGN:    return "ASSIGN";
        case TokenType::EQ:        return "EQ";
        case TokenType::NEQ:       return "NEQ";
        case TokenType::LT:        return "LT";
        case TokenType::GT:        return "GT";
        case TokenType::LTE:       return "LTE";
        case TokenType::GTE:       return "GTE";
        case TokenType::LPAREN:    return "LPAREN";
        case TokenType::RPAREN:    return "RPAREN";
        case TokenType::LBRACE:    return "LBRACE";
        case TokenType::RBRACE:    return "RBRACE";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COLON:     return "COLON";
        case TokenType::COMMA:     return "COMMA";
        case TokenType::EOF_TOKEN: return "EOF";
        case TokenType::ILLEGAL:   return "ILLEGAL";
    }
    return "UNKNOWN";
}

TokenType lookupKeyword(const std::string& ident) {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"fn",     TokenType::FN},
        {"let",    TokenType::LET},
        {"mut",    TokenType::MUT},
        {"if",     TokenType::IF},
        {"else",   TokenType::ELSE},
        {"while",  TokenType::WHILE},
        {"return", TokenType::RETURN},
    };

    auto it = keywords.find(ident);
    if (it != keywords.end()) {
        return it->second;
    }
    return TokenType::IDENT;
}
