#include "token.h"
#include <unordered_map>

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::FN:            return "FN";
        case TokenType::LET:           return "LET";
        case TokenType::MUT:           return "MUT";
        case TokenType::IF:            return "IF";
        case TokenType::ELSE:          return "ELSE";
        case TokenType::WHILE:         return "WHILE";
        case TokenType::RETURN:        return "RETURN";
        case TokenType::TRUE:          return "TRUE";
        case TokenType::FALSE:         return "FALSE";
        case TokenType::LOOP:          return "LOOP";
        case TokenType::BREAK:         return "BREAK";
        case TokenType::CONTINUE:      return "CONTINUE";
        case TokenType::STRUCT:        return "STRUCT";
        case TokenType::ENUM:          return "ENUM";
        case TokenType::IMPL:          return "IMPL";
        case TokenType::AS:            return "AS";
        case TokenType::CONST:         return "CONST";
        case TokenType::SELF_LOWER:    return "SELF";
        case TokenType::IDENT:         return "IDENT";
        case TokenType::NUMBER:        return "NUMBER";
        case TokenType::STRING:        return "STRING";
        case TokenType::PLUS:          return "PLUS";
        case TokenType::MINUS:         return "MINUS";
        case TokenType::STAR:          return "STAR";
        case TokenType::SLASH:         return "SLASH";
        case TokenType::PERCENT:       return "PERCENT";
        case TokenType::ASSIGN:        return "ASSIGN";
        case TokenType::EQ:            return "EQ";
        case TokenType::NEQ:           return "NEQ";
        case TokenType::LT:            return "LT";
        case TokenType::GT:            return "GT";
        case TokenType::LTE:           return "LTE";
        case TokenType::GTE:           return "GTE";
        case TokenType::PLUS_ASSIGN:   return "PLUS_ASSIGN";
        case TokenType::MINUS_ASSIGN:  return "MINUS_ASSIGN";
        case TokenType::STAR_ASSIGN:   return "STAR_ASSIGN";
        case TokenType::SLASH_ASSIGN:  return "SLASH_ASSIGN";
        case TokenType::PERCENT_ASSIGN: return "PERCENT_ASSIGN";
        case TokenType::AND_AND:       return "AND_AND";
        case TokenType::OR_OR:         return "OR_OR";
        case TokenType::NOT:           return "NOT";
        case TokenType::ARROW:         return "ARROW";
        case TokenType::COLON_COLON:   return "COLON_COLON";
        case TokenType::DOT:           return "DOT";
        case TokenType::LPAREN:        return "LPAREN";
        case TokenType::RPAREN:        return "RPAREN";
        case TokenType::LBRACE:        return "LBRACE";
        case TokenType::RBRACE:        return "RBRACE";
        case TokenType::LBRACKET:      return "LBRACKET";
        case TokenType::RBRACKET:      return "RBRACKET";
        case TokenType::SEMICOLON:     return "SEMICOLON";
        case TokenType::COLON:         return "COLON";
        case TokenType::COMMA:         return "COMMA";
        case TokenType::AMP:           return "AMP";
        case TokenType::EOF_TOKEN:     return "EOF";
        case TokenType::ILLEGAL:       return "ILLEGAL";
    }
    return "UNKNOWN";
}

TokenType lookupKeyword(const std::string& ident) {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"fn",       TokenType::FN},
        {"let",      TokenType::LET},
        {"mut",      TokenType::MUT},
        {"if",       TokenType::IF},
        {"else",     TokenType::ELSE},
        {"while",    TokenType::WHILE},
        {"return",   TokenType::RETURN},
        {"true",     TokenType::TRUE},
        {"false",    TokenType::FALSE},
        {"loop",     TokenType::LOOP},
        {"break",    TokenType::BREAK},
        {"continue", TokenType::CONTINUE},
        {"struct",   TokenType::STRUCT},
        {"enum",     TokenType::ENUM},
        {"impl",     TokenType::IMPL},
        {"as",       TokenType::AS},
        {"const",    TokenType::CONST},
        {"self",     TokenType::SELF_LOWER},
    };

    auto it = keywords.find(ident);
    if (it != keywords.end()) {
        return it->second;
    }
    return TokenType::IDENT;
}
