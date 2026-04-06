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
    LOOP_STMT,
    BREAK_STMT,
    CONTINUE_STMT,
    STRUCT_DEF,
    ENUM_DEF,
    IMPL_BLOCK,
    // Expressions
    ASSIGN_EXPR,
    BINARY_EXPR,
    UNARY_EXPR,
    CALL_EXPR,
    IDENT_EXPR,
    NUMBER_LITERAL,
    STRING_LITERAL,
    BOOL_LITERAL,
    FIELD_ACCESS_EXPR,
    METHOD_CALL_EXPR,
    INDEX_EXPR,
    PATH_EXPR,
    CAST_EXPR,
    ARRAY_LITERAL,
    STRUCT_LITERAL,
};

// ============================================================
// Base node
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

struct ParamNode {
    std::string name;
    std::string typeName;
    bool isSelf = false;
    bool isMutSelf = false;
    bool isMut = false;    // mut keyword on non-self params
    int line = 0;
};

struct FnDeclNode : AstNode {
    std::string name;
    std::vector<ParamNode> params;
    std::string returnType;
    AstNodePtr body;

    FnDeclNode(std::string n, int l = 0)
        : AstNode(NodeKind::FN_DECL, l), name(std::move(n)) {}
};

struct BlockNode : AstNode {
    std::vector<AstNodePtr> statements;
    explicit BlockNode(int l = 0) : AstNode(NodeKind::BLOCK, l) {}
};

struct LetStmtNode : AstNode {
    bool isMut;
    std::string name;
    std::string typeName;
    AstNodePtr init;

    LetStmtNode(bool m, std::string n, int l = 0)
        : AstNode(NodeKind::LET_STMT, l), isMut(m), name(std::move(n)) {}
};

struct ReturnStmtNode : AstNode {
    AstNodePtr value;
    explicit ReturnStmtNode(int l = 0) : AstNode(NodeKind::RETURN_STMT, l) {}
};

struct WhileStmtNode : AstNode {
    AstNodePtr condition;
    AstNodePtr body;
    explicit WhileStmtNode(int l = 0) : AstNode(NodeKind::WHILE_STMT, l) {}
};

struct IfStmtNode : AstNode {
    AstNodePtr condition;
    AstNodePtr thenBranch;
    AstNodePtr elseBranch;
    explicit IfStmtNode(int l = 0) : AstNode(NodeKind::IF_STMT, l) {}
};

struct ExprStmtNode : AstNode {
    AstNodePtr expr;
    explicit ExprStmtNode(int l = 0) : AstNode(NodeKind::EXPR_STMT, l) {}
};

struct LoopStmtNode : AstNode {
    AstNodePtr body;
    explicit LoopStmtNode(int l = 0) : AstNode(NodeKind::LOOP_STMT, l) {}
};

struct BreakStmtNode : AstNode {
    explicit BreakStmtNode(int l = 0) : AstNode(NodeKind::BREAK_STMT, l) {}
};

struct ContinueStmtNode : AstNode {
    explicit ContinueStmtNode(int l = 0) : AstNode(NodeKind::CONTINUE_STMT, l) {}
};

struct StructField {
    std::string name;
    std::string typeName;
    int line = 0;
};

struct StructDefNode : AstNode {
    std::string name;
    std::vector<StructField> fields;
    StructDefNode(std::string n, int l = 0)
        : AstNode(NodeKind::STRUCT_DEF, l), name(std::move(n)) {}
};

struct EnumDefNode : AstNode {
    std::string name;
    std::vector<std::string> variants;
    EnumDefNode(std::string n, int l = 0)
        : AstNode(NodeKind::ENUM_DEF, l), name(std::move(n)) {}
};

struct ImplBlockNode : AstNode {
    std::string targetName;
    std::vector<AstNodePtr> methods;
    ImplBlockNode(std::string n, int l = 0)
        : AstNode(NodeKind::IMPL_BLOCK, l), targetName(std::move(n)) {}
};

// ============================================================
// Expressions
// ============================================================

struct AssignExprNode : AstNode {
    AstNodePtr target;   // lvalue: IdentExpr, FieldAccess, IndexExpr
    AstNodePtr value;
    std::string op;      // "" for plain =, "+" for +=, "-" for -=
    explicit AssignExprNode(int l = 0)
        : AstNode(NodeKind::ASSIGN_EXPR, l) {}
};

struct BinaryExprNode : AstNode {
    std::string op;
    AstNodePtr left;
    AstNodePtr right;
    BinaryExprNode(std::string o, int l = 0)
        : AstNode(NodeKind::BINARY_EXPR, l), op(std::move(o)) {}
};

struct UnaryExprNode : AstNode {
    std::string op;
    AstNodePtr operand;
    UnaryExprNode(std::string o, int l = 0)
        : AstNode(NodeKind::UNARY_EXPR, l), op(std::move(o)) {}
};

struct CallExprNode : AstNode {
    std::string callee;
    std::vector<AstNodePtr> args;
    CallExprNode(std::string c, int l = 0)
        : AstNode(NodeKind::CALL_EXPR, l), callee(std::move(c)) {}
};

struct IdentExprNode : AstNode {
    std::string name;
    IdentExprNode(std::string n, int l = 0)
        : AstNode(NodeKind::IDENT_EXPR, l), name(std::move(n)) {}
};

struct NumberLiteralNode : AstNode {
    std::string value;
    NumberLiteralNode(std::string v, int l = 0)
        : AstNode(NodeKind::NUMBER_LITERAL, l), value(std::move(v)) {}
};

struct StringLiteralNode : AstNode {
    std::string value;
    StringLiteralNode(std::string v, int l = 0)
        : AstNode(NodeKind::STRING_LITERAL, l), value(std::move(v)) {}
};

struct BoolLiteralNode : AstNode {
    bool value;
    BoolLiteralNode(bool v, int l = 0)
        : AstNode(NodeKind::BOOL_LITERAL, l), value(v) {}
};

struct FieldAccessExprNode : AstNode {
    AstNodePtr object;
    std::string field;
    FieldAccessExprNode(std::string f, int l = 0)
        : AstNode(NodeKind::FIELD_ACCESS_EXPR, l), field(std::move(f)) {}
};

struct MethodCallExprNode : AstNode {
    AstNodePtr object;
    std::string method;
    std::vector<AstNodePtr> args;
    MethodCallExprNode(std::string m, int l = 0)
        : AstNode(NodeKind::METHOD_CALL_EXPR, l), method(std::move(m)) {}
};

struct IndexExprNode : AstNode {
    AstNodePtr object;
    AstNodePtr index;
    explicit IndexExprNode(int l = 0) : AstNode(NodeKind::INDEX_EXPR, l) {}
};

struct PathExprNode : AstNode {
    std::string base;
    std::string member;
    PathExprNode(std::string b, std::string m, int l = 0)
        : AstNode(NodeKind::PATH_EXPR, l), base(std::move(b)), member(std::move(m)) {}
};

struct CastExprNode : AstNode {
    AstNodePtr expr;
    std::string targetType;
    CastExprNode(std::string t, int l = 0)
        : AstNode(NodeKind::CAST_EXPR, l), targetType(std::move(t)) {}
};

struct ArrayLiteralNode : AstNode {
    std::vector<AstNodePtr> elements;
    AstNodePtr repeatValue;
    AstNodePtr repeatCount;
    bool isRepeat = false;
    explicit ArrayLiteralNode(int l = 0) : AstNode(NodeKind::ARRAY_LITERAL, l) {}
};

struct StructLiteralField {
    std::string name;
    AstNodePtr value;
};

struct StructLiteralNode : AstNode {
    std::string structName;
    std::vector<StructLiteralField> fields;
    StructLiteralNode(std::string n, int l = 0)
        : AstNode(NodeKind::STRUCT_LITERAL, l), structName(std::move(n)) {}
};

#endif // AST_H
