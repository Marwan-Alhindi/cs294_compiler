#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../parser/ast.h"
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================
// SemanticError — one entry per detected issue
// ============================================================
struct SemanticError {
    std::string message;
    int line;
};

// ============================================================
// SymbolInfo — entry in the scoped symbol table
// ============================================================
struct SymbolInfo {
    std::string type;   // "i32", "string", "bool", "fn", "void", "unknown"
    bool isMutable;
    int line;           // line where declared
};

// ============================================================
// FunctionInfo — registered top-level function signature
// ============================================================
struct FunctionInfo {
    std::string name;
    std::vector<std::string> paramTypes;
    int line;
};

// ============================================================
// SemanticAnalyzer — validates the AST for semantic correctness
// ============================================================
class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(ProgramNode* root);

    // Run analysis. Returns true if no errors.
    bool analyze();

    const std::vector<SemanticError>& errors() const;

private:
    ProgramNode* root_;
    std::vector<SemanticError> errors_;

    // Scoped symbol table: stack of scopes (innermost = back)
    std::vector<std::unordered_map<std::string, SymbolInfo>> scopes_;

    // Top-level function signatures (pre-registered for forward calls)
    std::unordered_map<std::string, FunctionInfo> functions_;

    // Scope management
    void pushScope();
    void popScope();
    bool declareVariable(const std::string& name, const std::string& type,
                         bool isMutable, int line);
    SymbolInfo* lookupVariable(const std::string& name);

    void recordError(const std::string& msg, int line);

    // Pre-registration pass: collect all top-level function signatures
    void registerFunctions();

    // Statement analysis
    void analyzeNode(AstNode* node);
    void analyzeProgram(ProgramNode* node);
    void analyzeFnDecl(FnDeclNode* node);
    void analyzeBlock(BlockNode* node);
    void analyzeLetStmt(LetStmtNode* node);
    void analyzeReturnStmt(ReturnStmtNode* node);
    void analyzeWhileStmt(WhileStmtNode* node);
    void analyzeIfStmt(IfStmtNode* node);
    void analyzeExprStmt(ExprStmtNode* node);

    // Expression analysis — returns inferred type string
    std::string analyzeExpr(AstNode* node);
    std::string analyzeAssignExpr(AssignExprNode* node);
    std::string analyzeBinaryExpr(BinaryExprNode* node);
    std::string analyzeUnaryExpr(UnaryExprNode* node);
    std::string analyzeCallExpr(CallExprNode* node);
    std::string analyzeIdentExpr(IdentExprNode* node);
};

#endif // SEMANTIC_H
