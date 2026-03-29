#include "token.h"
#include <gtest/gtest.h>

// --- tokenTypeToString tests ---

TEST(Token, TypeToStringKeywords) {
    EXPECT_EQ(tokenTypeToString(TokenType::FN), "FN");
    EXPECT_EQ(tokenTypeToString(TokenType::LET), "LET");
    EXPECT_EQ(tokenTypeToString(TokenType::MUT), "MUT");
    EXPECT_EQ(tokenTypeToString(TokenType::IF), "IF");
    EXPECT_EQ(tokenTypeToString(TokenType::ELSE), "ELSE");
    EXPECT_EQ(tokenTypeToString(TokenType::WHILE), "WHILE");
    EXPECT_EQ(tokenTypeToString(TokenType::RETURN), "RETURN");
}

TEST(Token, TypeToStringLiterals) {
    EXPECT_EQ(tokenTypeToString(TokenType::IDENT), "IDENT");
    EXPECT_EQ(tokenTypeToString(TokenType::NUMBER), "NUMBER");
    EXPECT_EQ(tokenTypeToString(TokenType::STRING), "STRING");
}

TEST(Token, TypeToStringOperators) {
    EXPECT_EQ(tokenTypeToString(TokenType::PLUS), "PLUS");
    EXPECT_EQ(tokenTypeToString(TokenType::MINUS), "MINUS");
    EXPECT_EQ(tokenTypeToString(TokenType::STAR), "STAR");
    EXPECT_EQ(tokenTypeToString(TokenType::SLASH), "SLASH");
    EXPECT_EQ(tokenTypeToString(TokenType::ASSIGN), "ASSIGN");
    EXPECT_EQ(tokenTypeToString(TokenType::EQ), "EQ");
    EXPECT_EQ(tokenTypeToString(TokenType::NEQ), "NEQ");
    EXPECT_EQ(tokenTypeToString(TokenType::LT), "LT");
    EXPECT_EQ(tokenTypeToString(TokenType::GT), "GT");
    EXPECT_EQ(tokenTypeToString(TokenType::LTE), "LTE");
    EXPECT_EQ(tokenTypeToString(TokenType::GTE), "GTE");
}

TEST(Token, TypeToStringPunctuation) {
    EXPECT_EQ(tokenTypeToString(TokenType::LPAREN), "LPAREN");
    EXPECT_EQ(tokenTypeToString(TokenType::RPAREN), "RPAREN");
    EXPECT_EQ(tokenTypeToString(TokenType::LBRACE), "LBRACE");
    EXPECT_EQ(tokenTypeToString(TokenType::RBRACE), "RBRACE");
    EXPECT_EQ(tokenTypeToString(TokenType::SEMICOLON), "SEMICOLON");
    EXPECT_EQ(tokenTypeToString(TokenType::COLON), "COLON");
    EXPECT_EQ(tokenTypeToString(TokenType::COMMA), "COMMA");
}

TEST(Token, TypeToStringSpecial) {
    EXPECT_EQ(tokenTypeToString(TokenType::EOF_TOKEN), "EOF");
    EXPECT_EQ(tokenTypeToString(TokenType::ILLEGAL), "ILLEGAL");
}

// --- lookupKeyword tests ---

TEST(Token, LookupKeywordFindsKeywords) {
    EXPECT_EQ(lookupKeyword("fn"), TokenType::FN);
    EXPECT_EQ(lookupKeyword("let"), TokenType::LET);
    EXPECT_EQ(lookupKeyword("mut"), TokenType::MUT);
    EXPECT_EQ(lookupKeyword("if"), TokenType::IF);
    EXPECT_EQ(lookupKeyword("else"), TokenType::ELSE);
    EXPECT_EQ(lookupKeyword("while"), TokenType::WHILE);
    EXPECT_EQ(lookupKeyword("return"), TokenType::RETURN);
}

TEST(Token, LookupKeywordReturnsIdentForNonKeywords) {
    EXPECT_EQ(lookupKeyword("foo"), TokenType::IDENT);
    EXPECT_EQ(lookupKeyword("_bar"), TokenType::IDENT);
    EXPECT_EQ(lookupKeyword("x1"), TokenType::IDENT);
    EXPECT_EQ(lookupKeyword("fn_name"), TokenType::IDENT);
    EXPECT_EQ(lookupKeyword("lettuce"), TokenType::IDENT);
}

TEST(Token, LookupKeywordIsCaseSensitive) {
    EXPECT_EQ(lookupKeyword("FN"), TokenType::IDENT);
    EXPECT_EQ(lookupKeyword("Fn"), TokenType::IDENT);
    EXPECT_EQ(lookupKeyword("IF"), TokenType::IDENT);
}
