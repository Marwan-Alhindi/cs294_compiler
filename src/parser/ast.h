#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <vector>

// ============================================================
// NodeKind — one entry per concrete node type
// ============================================================
enum class NodeKind {
    // Statements
    PROGRAM,
    FN_DECL,
    BLOCK,
    LET_STMT,
    RETURN_STMT,
    WHILE_STMT,
    IF_STMT,
    EXPR_STMT,
    // Expressions
    ASSIGN_EXPR,
    BINARY_EXPR,
    UNARY_EXPR,
    CALL_EXPR,
    IDENT_EXPR,
    NUMBER_LITERAL,
    STRING_LITERAL,
};

// ============================================================
// Base node — virtual destructor enables polymorphic deletion
// ============================================================
struct AstNode {
    NodeKind kind;
    int line;

    explicit AstNode(NodeKind k, int l = 0) : kind(k), line(l) {}
    virtual ~AstNode() = default;

    AstNode(const AstNode&) = delete;
    AstNode& operator=(const AstNode&) = delete;
};

using AstNodePtr = std::unique_ptr<AstNode>;

// ============================================================
// Statements
// ============================================================

struct ProgramNode : AstNode {
    std::vector<AstNodePtr> statements;

    ProgramNode() : AstNode(NodeKind::PROGRAM) {}
};

// Parameter within a function declaration
struct ParamNode {
    std::string name;
    std::string typeName;
    int line = 0;
};

// fn <name>(<params>) <body>
struct FnDeclNode : AstNode {
    std::string name;
    std::vector<ParamNode> params;
    AstNodePtr body;  // BlockNode

    FnDeclNode(std::string n, int l = 0)
        : AstNode(NodeKind::FN_DECL, l), name(std::move(n)) {}
};

// { statement* }
struct BlockNode : AstNode {
    std::vector<AstNodePtr> statements;

    explicit BlockNode(int l = 0) : AstNode(NodeKind::BLOCK, l) {}
};

// let mut? <name> (: <type>)? = <init>;
struct LetStmtNode : AstNode {
    bool isMut;
    std::string name;
    std::string typeName;  // empty if no type annotation
    AstNodePtr init;

    LetStmtNode(bool m, std::string n, int l = 0)
        : AstNode(NodeKind::LET_STMT, l), isMut(m), name(std::move(n)) {}
};

// return <value>?;
struct ReturnStmtNode : AstNode {
    AstNodePtr value;  // nullptr if bare return

    explicit ReturnStmtNode(int l = 0) : AstNode(NodeKind::RETURN_STMT, l) {}
};

// while <condition> <body>
struct WhileStmtNode : AstNode {
    AstNodePtr condition;
    AstNodePtr body;  // BlockNode

    explicit WhileStmtNode(int l = 0) : AstNode(NodeKind::WHILE_STMT, l) {}
};

// if <condition> <thenBranch> (else <elseBranch>)?
struct IfStmtNode : AstNode {
    AstNodePtr condition;
    AstNodePtr thenBranch;  // BlockNode
    AstNodePtr elseBranch;  // BlockNode or IfStmtNode; nullptr if absent

    explicit IfStmtNode(int l = 0) : AstNode(NodeKind::IF_STMT, l) {}
};

// <expr>;
struct ExprStmtNode : AstNode {
    AstNodePtr expr;

    explicit ExprStmtNode(int l = 0) : AstNode(NodeKind::EXPR_STMT, l) {}
};

// ============================================================
// Expressions
// ============================================================

// <target> = <value>
struct AssignExprNode : AstNode {
    std::string target;
    AstNodePtr value;

    AssignExprNode(std::string t, int l = 0)
        : AstNode(NodeKind::ASSIGN_EXPR, l), target(std::move(t)) {}
};

// <left> <op> <right>  (op: "+", "-", "*", "/", "==", "!=", "<", ">", "<=", ">=")
struct BinaryExprNode : AstNode {
    std::string op;
    AstNodePtr left;
    AstNodePtr right;

    BinaryExprNode(std::string o, int l = 0)
        : AstNode(NodeKind::BINARY_EXPR, l), op(std::move(o)) {}
};

// -<operand>
struct UnaryExprNode : AstNode {
    std::string op;  // "-"
    AstNodePtr operand;

    UnaryExprNode(std::string o, int l = 0)
        : AstNode(NodeKind::UNARY_EXPR, l), op(std::move(o)) {}
};

// <callee>(<args>)
struct CallExprNode : AstNode {
    std::string callee;
    std::vector<AstNodePtr> args;

    CallExprNode(std::string c, int l = 0)
        : AstNode(NodeKind::CALL_EXPR, l), callee(std::move(c)) {}
};

// bare identifier reference
struct IdentExprNode : AstNode {
    std::string name;

    IdentExprNode(std::string n, int l = 0)
        : AstNode(NodeKind::IDENT_EXPR, l), name(std::move(n)) {}
};

// integer literal stored as string; numeric conversion in semantic phase
struct NumberLiteralNode : AstNode {
    std::string value;

    NumberLiteralNode(std::string v, int l = 0)
        : AstNode(NodeKind::NUMBER_LITERAL, l), value(std::move(v)) {}
};

// string literal (contents without surrounding quotes)
struct StringLiteralNode : AstNode {
    std::string value;

    StringLiteralNode(std::string v, int l = 0)
        : AstNode(NodeKind::STRING_LITERAL, l), value(std::move(v)) {}
};

#endif // AST_H
