#include "ast.h"
#include <gtest/gtest.h>

// ============================================================
// Construction tests — verify each node can be created and
// that the `kind` field is set correctly.
// ============================================================

TEST(Ast, ProgramNodeKind) {
    ProgramNode node;
    EXPECT_EQ(node.kind, NodeKind::PROGRAM);
}

TEST(Ast, FnDeclNodeKind) {
    FnDeclNode node("main", 1);
    EXPECT_EQ(node.kind, NodeKind::FN_DECL);
    EXPECT_EQ(node.name, "main");
    EXPECT_EQ(node.line, 1);
}

TEST(Ast, BlockNodeKind) {
    BlockNode node(2);
    EXPECT_EQ(node.kind, NodeKind::BLOCK);
    EXPECT_EQ(node.line, 2);
}

TEST(Ast, LetStmtNodeKind) {
    LetStmtNode node(true, "x", 3);
    EXPECT_EQ(node.kind, NodeKind::LET_STMT);
    EXPECT_TRUE(node.isMut);
    EXPECT_EQ(node.name, "x");
    EXPECT_EQ(node.line, 3);
}

TEST(Ast, LetStmtNotMut) {
    LetStmtNode node(false, "y");
    EXPECT_EQ(node.kind, NodeKind::LET_STMT);
    EXPECT_FALSE(node.isMut);
}

TEST(Ast, ReturnStmtNodeKind) {
    ReturnStmtNode node(4);
    EXPECT_EQ(node.kind, NodeKind::RETURN_STMT);
    EXPECT_EQ(node.value, nullptr);
}

TEST(Ast, WhileStmtNodeKind) {
    WhileStmtNode node(5);
    EXPECT_EQ(node.kind, NodeKind::WHILE_STMT);
    EXPECT_EQ(node.condition, nullptr);
    EXPECT_EQ(node.body, nullptr);
}

TEST(Ast, IfStmtNodeKind) {
    IfStmtNode node(6);
    EXPECT_EQ(node.kind, NodeKind::IF_STMT);
    EXPECT_EQ(node.elseBranch, nullptr);
}

TEST(Ast, ExprStmtNodeKind) {
    ExprStmtNode node(7);
    EXPECT_EQ(node.kind, NodeKind::EXPR_STMT);
}

TEST(Ast, AssignExprNodeKind) {
    AssignExprNode node("x", 8);
    EXPECT_EQ(node.kind, NodeKind::ASSIGN_EXPR);
    EXPECT_EQ(node.target, "x");
}

TEST(Ast, BinaryExprNodeKind) {
    BinaryExprNode node("+", 9);
    EXPECT_EQ(node.kind, NodeKind::BINARY_EXPR);
    EXPECT_EQ(node.op, "+");
}

TEST(Ast, UnaryExprNodeKind) {
    UnaryExprNode node("-", 10);
    EXPECT_EQ(node.kind, NodeKind::UNARY_EXPR);
    EXPECT_EQ(node.op, "-");
}

TEST(Ast, CallExprNodeKind) {
    CallExprNode node("println", 11);
    EXPECT_EQ(node.kind, NodeKind::CALL_EXPR);
    EXPECT_EQ(node.callee, "println");
    EXPECT_TRUE(node.args.empty());
}

TEST(Ast, IdentExprNodeKind) {
    IdentExprNode node("foo", 12);
    EXPECT_EQ(node.kind, NodeKind::IDENT_EXPR);
    EXPECT_EQ(node.name, "foo");
}

TEST(Ast, NumberLiteralNodeKind) {
    NumberLiteralNode node("42", 13);
    EXPECT_EQ(node.kind, NodeKind::NUMBER_LITERAL);
    EXPECT_EQ(node.value, "42");
}

TEST(Ast, StringLiteralNodeKind) {
    StringLiteralNode node("hello", 14);
    EXPECT_EQ(node.kind, NodeKind::STRING_LITERAL);
    EXPECT_EQ(node.value, "hello");
}

// ============================================================
// Ownership / tree construction
// ============================================================

TEST(Ast, ProgramNodeOwnsChildren) {
    auto prog = std::make_unique<ProgramNode>();
    prog->statements.push_back(std::make_unique<ExprStmtNode>(1));
    prog->statements.push_back(std::make_unique<ExprStmtNode>(2));
    EXPECT_EQ(prog->statements.size(), 2u);
    EXPECT_EQ(prog->statements[0]->kind, NodeKind::EXPR_STMT);
}

TEST(Ast, FnDeclHasParams) {
    FnDeclNode fn("add", 1);
    fn.params.push_back(ParamNode{"a", "i32", 1});
    fn.params.push_back(ParamNode{"b", "i32", 1});
    EXPECT_EQ(fn.params.size(), 2u);
    EXPECT_EQ(fn.params[0].name, "a");
    EXPECT_EQ(fn.params[1].typeName, "i32");
}

TEST(Ast, DowncastViaKind) {
    AstNodePtr node = std::make_unique<NumberLiteralNode>("99", 5);
    EXPECT_EQ(node->kind, NodeKind::NUMBER_LITERAL);
    auto* numNode = static_cast<NumberLiteralNode*>(node.get());
    EXPECT_EQ(numNode->value, "99");
    EXPECT_EQ(numNode->line, 5);
}
