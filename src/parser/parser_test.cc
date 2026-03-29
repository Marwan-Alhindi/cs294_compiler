#include "parser.h"
#include "ast.h"
#include <gtest/gtest.h>
#include <string>

// ============================================================
// Helpers
// ============================================================

// Parse source and return the first statement from ProgramNode.
static AstNode* firstStmt(const std::string& src, Parser& p) {
    auto prog = p.parseProgram();
    if (!prog || prog->statements.empty()) return nullptr;
    return prog->statements[0].get();
}

// Parse source expecting zero errors and at least one statement.
static std::unique_ptr<ProgramNode> parseOk(const std::string& src) {
    Parser p(src);
    auto prog = p.parseProgram();
    EXPECT_FALSE(p.hasErrors()) << "Unexpected parse error";
    return prog;
}

// Cast base node to a concrete type (unchecked — test must verify kind first).
template<typename T>
static T* as(AstNode* n) { return static_cast<T*>(n); }

// ============================================================
// Empty program
// ============================================================

TEST(Parser, EmptyProgram) {
    auto prog = parseOk("");
    ASSERT_TRUE(prog != nullptr);
    EXPECT_TRUE(prog->statements.empty());
}

// ============================================================
// Literals and primary expressions
// ============================================================

TEST(Parser, NumberLiteral) {
    auto prog = parseOk("42;");
    ASSERT_EQ(prog->statements.size(), 1u);
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    EXPECT_EQ(stmt->kind, NodeKind::EXPR_STMT);
    EXPECT_EQ(stmt->expr->kind, NodeKind::NUMBER_LITERAL);
    EXPECT_EQ(as<NumberLiteralNode>(stmt->expr.get())->value, "42");
}

TEST(Parser, StringLiteral) {
    auto prog = parseOk("\"hello\";");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    EXPECT_EQ(stmt->expr->kind, NodeKind::STRING_LITERAL);
    EXPECT_EQ(as<StringLiteralNode>(stmt->expr.get())->value, "hello");
}

TEST(Parser, IdentExpression) {
    auto prog = parseOk("foo;");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    EXPECT_EQ(stmt->expr->kind, NodeKind::IDENT_EXPR);
    EXPECT_EQ(as<IdentExprNode>(stmt->expr.get())->name, "foo");
}

TEST(Parser, GroupedExpression) {
    auto prog = parseOk("(42);");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    EXPECT_EQ(stmt->expr->kind, NodeKind::NUMBER_LITERAL);
}

// ============================================================
// Unary and binary expressions
// ============================================================

TEST(Parser, UnaryMinus) {
    auto prog = parseOk("-5;");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    ASSERT_EQ(stmt->expr->kind, NodeKind::UNARY_EXPR);
    auto* u = as<UnaryExprNode>(stmt->expr.get());
    EXPECT_EQ(u->op, "-");
    EXPECT_EQ(u->operand->kind, NodeKind::NUMBER_LITERAL);
}

TEST(Parser, AdditionExpr) {
    auto prog = parseOk("1 + 2;");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    ASSERT_EQ(stmt->expr->kind, NodeKind::BINARY_EXPR);
    auto* b = as<BinaryExprNode>(stmt->expr.get());
    EXPECT_EQ(b->op, "+");
}

TEST(Parser, SubtractionExpr) {
    auto prog = parseOk("x - 1;");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    ASSERT_EQ(stmt->expr->kind, NodeKind::BINARY_EXPR);
    EXPECT_EQ(as<BinaryExprNode>(stmt->expr.get())->op, "-");
}

TEST(Parser, MultiplicationExpr) {
    auto prog = parseOk("a * b;");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    EXPECT_EQ(as<BinaryExprNode>(stmt->expr.get())->op, "*");
}

TEST(Parser, DivisionExpr) {
    auto prog = parseOk("a / b;");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    EXPECT_EQ(as<BinaryExprNode>(stmt->expr.get())->op, "/");
}

// ============================================================
// Operator precedence
// ============================================================

TEST(Parser, PrecedenceMulBeforeAdd) {
    // 1 + 2 * 3  should parse as  1 + (2 * 3)
    auto prog = parseOk("1 + 2 * 3;");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    auto* add = as<BinaryExprNode>(stmt->expr.get());
    EXPECT_EQ(add->op, "+");
    // right side should be the multiplication
    ASSERT_EQ(add->right->kind, NodeKind::BINARY_EXPR);
    EXPECT_EQ(as<BinaryExprNode>(add->right.get())->op, "*");
}

TEST(Parser, PrecedenceParensOverride) {
    // (1 + 2) * 3  should parse as  (1+2) * 3
    auto prog = parseOk("(1 + 2) * 3;");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    auto* mul = as<BinaryExprNode>(stmt->expr.get());
    EXPECT_EQ(mul->op, "*");
    // left side should be the addition
    ASSERT_EQ(mul->left->kind, NodeKind::BINARY_EXPR);
    EXPECT_EQ(as<BinaryExprNode>(mul->left.get())->op, "+");
}

TEST(Parser, ComparisonExpr) {
    auto prog = parseOk("x == 0;");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    ASSERT_EQ(stmt->expr->kind, NodeKind::BINARY_EXPR);
    EXPECT_EQ(as<BinaryExprNode>(stmt->expr.get())->op, "==");
}

TEST(Parser, AllComparisonOps) {
    for (const std::string& src : {"x != y;", "x < y;", "x > y;", "x <= y;", "x >= y;"}) {
        Parser p(src);
        auto prog = p.parseProgram();
        EXPECT_FALSE(p.hasErrors()) << "Error in: " << src;
        auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
        EXPECT_EQ(stmt->expr->kind, NodeKind::BINARY_EXPR) << "In: " << src;
    }
}

// ============================================================
// Assignment expression
// ============================================================

TEST(Parser, AssignmentExpr) {
    auto prog = parseOk("x = 42;");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    ASSERT_EQ(stmt->expr->kind, NodeKind::ASSIGN_EXPR);
    auto* asgn = as<AssignExprNode>(stmt->expr.get());
    EXPECT_EQ(asgn->target, "x");
    EXPECT_EQ(asgn->value->kind, NodeKind::NUMBER_LITERAL);
}

// ============================================================
// Function calls
// ============================================================

TEST(Parser, FunctionCallNoArgs) {
    auto prog = parseOk("foo();");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    ASSERT_EQ(stmt->expr->kind, NodeKind::CALL_EXPR);
    auto* call = as<CallExprNode>(stmt->expr.get());
    EXPECT_EQ(call->callee, "foo");
    EXPECT_TRUE(call->args.empty());
}

TEST(Parser, FunctionCallWithArgs) {
    auto prog = parseOk("add(1, 2);");
    auto* stmt = as<ExprStmtNode>(prog->statements[0].get());
    ASSERT_EQ(stmt->expr->kind, NodeKind::CALL_EXPR);
    auto* call = as<CallExprNode>(stmt->expr.get());
    EXPECT_EQ(call->callee, "add");
    EXPECT_EQ(call->args.size(), 2u);
}

// ============================================================
// Let statements
// ============================================================

TEST(Parser, LetStmtSimple) {
    auto prog = parseOk("let x = 42;");
    ASSERT_EQ(prog->statements.size(), 1u);
    ASSERT_EQ(prog->statements[0]->kind, NodeKind::LET_STMT);
    auto* let = as<LetStmtNode>(prog->statements[0].get());
    EXPECT_EQ(let->name, "x");
    EXPECT_FALSE(let->isMut);
    EXPECT_TRUE(let->typeName.empty());
    EXPECT_EQ(let->init->kind, NodeKind::NUMBER_LITERAL);
}

TEST(Parser, LetStmtMut) {
    auto prog = parseOk("let mut y = 0;");
    auto* let = as<LetStmtNode>(prog->statements[0].get());
    EXPECT_TRUE(let->isMut);
    EXPECT_EQ(let->name, "y");
}

TEST(Parser, LetStmtWithTypeAnnotation) {
    auto prog = parseOk("let x: i32 = 5;");
    auto* let = as<LetStmtNode>(prog->statements[0].get());
    EXPECT_EQ(let->typeName, "i32");
}

// ============================================================
// Return statements
// ============================================================

TEST(Parser, ReturnWithValue) {
    auto prog = parseOk("return 42;");
    ASSERT_EQ(prog->statements[0]->kind, NodeKind::RETURN_STMT);
    auto* ret = as<ReturnStmtNode>(prog->statements[0].get());
    ASSERT_NE(ret->value, nullptr);
    EXPECT_EQ(ret->value->kind, NodeKind::NUMBER_LITERAL);
}

TEST(Parser, ReturnBare) {
    auto prog = parseOk("return;");
    auto* ret = as<ReturnStmtNode>(prog->statements[0].get());
    EXPECT_EQ(ret->value, nullptr);
}

// ============================================================
// While statement
// ============================================================

TEST(Parser, WhileStmt) {
    auto prog = parseOk("while x > 0 { x = x - 1; }");
    ASSERT_EQ(prog->statements[0]->kind, NodeKind::WHILE_STMT);
    auto* w = as<WhileStmtNode>(prog->statements[0].get());
    EXPECT_EQ(w->condition->kind, NodeKind::BINARY_EXPR);
    EXPECT_EQ(w->body->kind, NodeKind::BLOCK);
    auto* body = as<BlockNode>(w->body.get());
    EXPECT_EQ(body->statements.size(), 1u);
}

// ============================================================
// If / if-else / if-else-if
// ============================================================

TEST(Parser, IfStmtNoElse) {
    auto prog = parseOk("if x == 0 { return; }");
    ASSERT_EQ(prog->statements[0]->kind, NodeKind::IF_STMT);
    auto* ifn = as<IfStmtNode>(prog->statements[0].get());
    EXPECT_EQ(ifn->condition->kind, NodeKind::BINARY_EXPR);
    EXPECT_EQ(ifn->thenBranch->kind, NodeKind::BLOCK);
    EXPECT_EQ(ifn->elseBranch, nullptr);
}

TEST(Parser, IfElseStmt) {
    auto prog = parseOk("if x == 0 { return; } else { x = 1; }");
    auto* ifn = as<IfStmtNode>(prog->statements[0].get());
    EXPECT_NE(ifn->elseBranch, nullptr);
    EXPECT_EQ(ifn->elseBranch->kind, NodeKind::BLOCK);
}

TEST(Parser, IfElseIfChain) {
    auto prog = parseOk("if x == 0 { return; } else if x == 1 { return; }");
    auto* ifn = as<IfStmtNode>(prog->statements[0].get());
    ASSERT_NE(ifn->elseBranch, nullptr);
    EXPECT_EQ(ifn->elseBranch->kind, NodeKind::IF_STMT);
}

// ============================================================
// Function declarations
// ============================================================

TEST(Parser, FnDeclNoParams) {
    auto prog = parseOk("fn main() { return; }");
    ASSERT_EQ(prog->statements[0]->kind, NodeKind::FN_DECL);
    auto* fn = as<FnDeclNode>(prog->statements[0].get());
    EXPECT_EQ(fn->name, "main");
    EXPECT_TRUE(fn->params.empty());
    EXPECT_EQ(fn->body->kind, NodeKind::BLOCK);
}

TEST(Parser, FnDeclWithParams) {
    auto prog = parseOk("fn add(a: i32, b: i32) { return a; }");
    auto* fn = as<FnDeclNode>(prog->statements[0].get());
    EXPECT_EQ(fn->name, "add");
    ASSERT_EQ(fn->params.size(), 2u);
    EXPECT_EQ(fn->params[0].name, "a");
    EXPECT_EQ(fn->params[0].typeName, "i32");
    EXPECT_EQ(fn->params[1].name, "b");
}

// ============================================================
// Full program (integration)
// ============================================================

TEST(Parser, FullProgram) {
    std::string src = R"(
fn main() {
    let x = 42;
    let mut y = 0;
    if x == y {
        return;
    } else {
        y = x + 1;
    }
    while y > 0 {
        y = y - 1;
    }
}
)";
    auto prog = parseOk(src);
    ASSERT_FALSE(prog == nullptr);
    EXPECT_EQ(prog->statements.size(), 1u);  // one top-level fn_decl
    EXPECT_EQ(prog->statements[0]->kind, NodeKind::FN_DECL);
    auto* fn = as<FnDeclNode>(prog->statements[0].get());
    auto* body = as<BlockNode>(fn->body.get());
    EXPECT_EQ(body->statements.size(), 4u);  // let, let, if, while
}

// ============================================================
// Line number tracking
// ============================================================

TEST(Parser, LineNumbers) {
    std::string src = "let x = 1;\nlet y = 2;\n";
    auto prog = parseOk(src);
    ASSERT_EQ(prog->statements.size(), 2u);
    EXPECT_EQ(prog->statements[0]->line, 1);
    EXPECT_EQ(prog->statements[1]->line, 2);
}

// ============================================================
// Error recovery
// ============================================================

TEST(Parser, ErrorOnMissingSemicolon) {
    Parser p("let x = 42 let y = 0;");
    auto prog = p.parseProgram();
    EXPECT_TRUE(p.hasErrors());
}

TEST(Parser, ErrorRecoveryParsesMultipleErrors) {
    // Two bad statements — should record at least one error but not crash
    Parser p("let = 1; let = 2;");
    auto prog = p.parseProgram();
    EXPECT_TRUE(p.hasErrors());
    EXPECT_GT(p.errors().size(), 0u);
}

TEST(Parser, ErrorContainsLineNumber) {
    Parser p("let = 1;");  // missing name on line 1
    p.parseProgram();
    ASSERT_FALSE(p.errors().empty());
    EXPECT_EQ(p.errors()[0].line, 1);
}
