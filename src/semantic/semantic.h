#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../parser/ast.h"
#include <string>
#include <unordered_map>
#include <vector>

struct SemanticError {
    std::string message;
    int line;
};

struct SymbolInfo {
    std::string type;
    bool isMutable;
    int line;
};

struct FunctionInfo {
    std::string name;
    std::vector<std::string> paramTypes;
    std::string returnType;
    int line;
};

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(ProgramNode* root);
    bool analyze();
    const std::vector<SemanticError>& errors() const;

private:
    ProgramNode* root_;
    std::vector<SemanticError> errors_;

    // Scoped symbol table
    std::vector<std::unordered_map<std::string, SymbolInfo>> scopes_;

    // Top-level + built-in function signatures
    std::unordered_map<std::string, FunctionInfo> functions_;

    // Struct definitions: name -> fields
    std::unordered_map<std::string, std::vector<StructField>> structDefs_;

    // Enum definitions: name -> variants
    std::unordered_map<std::string, std::vector<std::string>> enumDefs_;

    // Methods: "TypeName.methodName" -> FunctionInfo
    std::unordered_map<std::string, FunctionInfo> methods_;

    // Scope management
    void pushScope();
    void popScope();
    bool declareVariable(const std::string& name, const std::string& type,
                         bool isMutable, int line);
    SymbolInfo* lookupVariable(const std::string& name);

    void recordError(const std::string& msg, int line);

    // Normalize integer type aliases to i32
    std::string normalizeType(const std::string& type) const;

    void registerBuiltins();
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
    void analyzeLoopStmt(LoopStmtNode* node);
    void analyzeStructDef(StructDefNode* node);
    void analyzeEnumDef(EnumDefNode* node);
    void analyzeImplBlock(ImplBlockNode* node);

    // Expression analysis
    std::string analyzeExpr(AstNode* node);
    std::string analyzeAssignExpr(AssignExprNode* node);
    std::string analyzeBinaryExpr(BinaryExprNode* node);
    std::string analyzeUnaryExpr(UnaryExprNode* node);
    std::string analyzeCallExpr(CallExprNode* node);
    std::string analyzeIdentExpr(IdentExprNode* node);
    std::string analyzeFieldAccessExpr(FieldAccessExprNode* node);
    std::string analyzeMethodCallExpr(MethodCallExprNode* node);
    std::string analyzeIndexExpr(IndexExprNode* node);
    std::string analyzePathExpr(PathExprNode* node);
    std::string analyzeCastExpr(CastExprNode* node);
};

#endif // SEMANTIC_H
