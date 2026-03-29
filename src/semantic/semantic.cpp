#include "semantic.h"

// ============================================================
// Constructor
// ============================================================
SemanticAnalyzer::SemanticAnalyzer(ProgramNode* root) : root_(root) {}

// ============================================================
// Public API
// ============================================================

bool SemanticAnalyzer::analyze() {
    errors_.clear();
    scopes_.clear();
    functions_.clear();

    pushScope();            // global scope
    registerFunctions();    // pre-register top-level fn signatures
    analyzeProgram(root_);
    popScope();

    return errors_.empty();
}

const std::vector<SemanticError>& SemanticAnalyzer::errors() const {
    return errors_;
}

// ============================================================
// Scope management
// ============================================================

void SemanticAnalyzer::pushScope() {
    scopes_.emplace_back();
}

void SemanticAnalyzer::popScope() {
    scopes_.pop_back();
}

bool SemanticAnalyzer::declareVariable(const std::string& name, const std::string& type,
                                       bool isMutable, int line) {
    auto& current = scopes_.back();
    if (current.count(name)) {
        recordError("Redeclaration of '" + name + "' in the same scope", line);
        return false;
    }
    current[name] = SymbolInfo{type, isMutable, line};
    return true;
}

SymbolInfo* SemanticAnalyzer::lookupVariable(const std::string& name) {
    // Walk scopes from innermost to outermost
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        auto it = scopes_[i].find(name);
        if (it != scopes_[i].end()) {
            return &it->second;
        }
    }
    return nullptr;
}

void SemanticAnalyzer::recordError(const std::string& msg, int line) {
    errors_.push_back(SemanticError{msg, line});
}

// ============================================================
// Pre-registration: collect top-level function signatures so
// functions can call each other regardless of declaration order.
// ============================================================

void SemanticAnalyzer::registerFunctions() {
    for (auto& stmt : root_->statements) {
        if (stmt->kind == NodeKind::FN_DECL) {
            auto* fn = static_cast<FnDeclNode*>(stmt.get());
            if (functions_.count(fn->name)) {
                recordError("Redefinition of function '" + fn->name + "'", fn->line);
                continue;
            }
            FunctionInfo info;
            info.name = fn->name;
            info.line = fn->line;
            for (auto& param : fn->params) {
                info.paramTypes.push_back(param.typeName.empty() ? "unknown" : param.typeName);
            }
            functions_[fn->name] = info;
            // Also make the function name visible as a symbol in the global scope
            declareVariable(fn->name, "fn", false, fn->line);
        }
    }
}

// ============================================================
// Generic dispatch
// ============================================================

void SemanticAnalyzer::analyzeNode(AstNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NodeKind::PROGRAM:     analyzeProgram(static_cast<ProgramNode*>(node)); break;
        case NodeKind::FN_DECL:     analyzeFnDecl(static_cast<FnDeclNode*>(node)); break;
        case NodeKind::BLOCK:       analyzeBlock(static_cast<BlockNode*>(node)); break;
        case NodeKind::LET_STMT:    analyzeLetStmt(static_cast<LetStmtNode*>(node)); break;
        case NodeKind::RETURN_STMT: analyzeReturnStmt(static_cast<ReturnStmtNode*>(node)); break;
        case NodeKind::WHILE_STMT:  analyzeWhileStmt(static_cast<WhileStmtNode*>(node)); break;
        case NodeKind::IF_STMT:     analyzeIfStmt(static_cast<IfStmtNode*>(node)); break;
        case NodeKind::EXPR_STMT:   analyzeExprStmt(static_cast<ExprStmtNode*>(node)); break;
        default:
            // Expression used as a statement — still analyze it
            analyzeExpr(node);
            break;
    }
}

// ============================================================
// Statement analysis
// ============================================================

void SemanticAnalyzer::analyzeProgram(ProgramNode* node) {
    for (auto& stmt : node->statements) {
        analyzeNode(stmt.get());
    }
}

void SemanticAnalyzer::analyzeFnDecl(FnDeclNode* node) {
    pushScope();  // scope for function parameters
    for (auto& param : node->params) {
        std::string type = param.typeName.empty() ? "unknown" : param.typeName;
        declareVariable(param.name, type, false, param.line);
    }
    if (node->body) {
        analyzeNode(node->body.get());  // BlockNode pushes its own inner scope
    }
    popScope();
}

void SemanticAnalyzer::analyzeBlock(BlockNode* node) {
    pushScope();
    for (auto& stmt : node->statements) {
        analyzeNode(stmt.get());
    }
    popScope();
}

void SemanticAnalyzer::analyzeLetStmt(LetStmtNode* node) {
    // Analyze initializer BEFORE declaring the variable (prevents let x = x;)
    std::string initType = "unknown";
    if (node->init) {
        initType = analyzeExpr(node->init.get());
    }

    // Determine variable type: explicit annotation wins, else infer from init
    std::string varType = initType;
    if (!node->typeName.empty()) {
        varType = node->typeName;
        // Check annotation vs. initializer type
        if (initType != "unknown" && varType != initType) {
            recordError("Type mismatch: variable '" + node->name + "' declared as " +
                        varType + " but initialized with " + initType, node->line);
        }
    }

    declareVariable(node->name, varType, node->isMut, node->line);
}

void SemanticAnalyzer::analyzeReturnStmt(ReturnStmtNode* node) {
    if (node->value) {
        analyzeExpr(node->value.get());
    }
}

void SemanticAnalyzer::analyzeWhileStmt(WhileStmtNode* node) {
    if (node->condition) {
        analyzeExpr(node->condition.get());
    }
    if (node->body) {
        analyzeNode(node->body.get());
    }
}

void SemanticAnalyzer::analyzeIfStmt(IfStmtNode* node) {
    if (node->condition) {
        analyzeExpr(node->condition.get());
    }
    if (node->thenBranch) {
        analyzeNode(node->thenBranch.get());
    }
    if (node->elseBranch) {
        analyzeNode(node->elseBranch.get());
    }
}

void SemanticAnalyzer::analyzeExprStmt(ExprStmtNode* node) {
    if (node->expr) {
        analyzeExpr(node->expr.get());
    }
}

// ============================================================
// Expression analysis — returns inferred type
// ============================================================

std::string SemanticAnalyzer::analyzeExpr(AstNode* node) {
    if (!node) return "unknown";

    switch (node->kind) {
        case NodeKind::ASSIGN_EXPR:
            return analyzeAssignExpr(static_cast<AssignExprNode*>(node));
        case NodeKind::BINARY_EXPR:
            return analyzeBinaryExpr(static_cast<BinaryExprNode*>(node));
        case NodeKind::UNARY_EXPR:
            return analyzeUnaryExpr(static_cast<UnaryExprNode*>(node));
        case NodeKind::CALL_EXPR:
            return analyzeCallExpr(static_cast<CallExprNode*>(node));
        case NodeKind::IDENT_EXPR:
            return analyzeIdentExpr(static_cast<IdentExprNode*>(node));
        case NodeKind::NUMBER_LITERAL:
            return "i32";
        case NodeKind::STRING_LITERAL:
            return "string";
        default:
            return "unknown";
    }
}

std::string SemanticAnalyzer::analyzeAssignExpr(AssignExprNode* node) {
    // Target must exist
    auto* sym = lookupVariable(node->target);
    if (!sym) {
        recordError("Undefined variable '" + node->target + "'", node->line);
        if (node->value) analyzeExpr(node->value.get());
        return "unknown";
    }

    // Target must be mutable
    if (!sym->isMutable) {
        recordError("Cannot assign to immutable variable '" + node->target + "'", node->line);
    }

    // Analyze the right-hand side
    std::string valueType = "unknown";
    if (node->value) {
        valueType = analyzeExpr(node->value.get());
    }

    // Type compatibility
    if (valueType != "unknown" && sym->type != "unknown" && sym->type != valueType) {
        recordError("Type mismatch: cannot assign " + valueType + " to variable '" +
                    node->target + "' of type " + sym->type, node->line);
    }

    return sym->type;
}

std::string SemanticAnalyzer::analyzeBinaryExpr(BinaryExprNode* node) {
    std::string leftType = analyzeExpr(node->left.get());
    std::string rightType = analyzeExpr(node->right.get());

    // Arithmetic: +, -, *, / — both operands must be i32, result is i32
    if (node->op == "+" || node->op == "-" || node->op == "*" || node->op == "/") {
        if (leftType != "unknown" && leftType != "i32") {
            recordError("Left operand of '" + node->op + "' must be i32, got " + leftType,
                        node->line);
        }
        if (rightType != "unknown" && rightType != "i32") {
            recordError("Right operand of '" + node->op + "' must be i32, got " + rightType,
                        node->line);
        }
        return "i32";
    }

    // Comparison: ==, !=, <, >, <=, >= — operands must match, result is bool
    if (node->op == "==" || node->op == "!=" || node->op == "<" ||
        node->op == ">"  || node->op == "<=" || node->op == ">=") {
        if (leftType != "unknown" && rightType != "unknown" && leftType != rightType) {
            recordError("Cannot compare " + leftType + " with " + rightType, node->line);
        }
        return "bool";
    }

    return "unknown";
}

std::string SemanticAnalyzer::analyzeUnaryExpr(UnaryExprNode* node) {
    std::string operandType = analyzeExpr(node->operand.get());

    if (node->op == "-") {
        if (operandType != "unknown" && operandType != "i32") {
            recordError("Unary '-' requires i32 operand, got " + operandType, node->line);
        }
        return "i32";
    }

    return operandType;
}

std::string SemanticAnalyzer::analyzeCallExpr(CallExprNode* node) {
    auto it = functions_.find(node->callee);
    if (it == functions_.end()) {
        recordError("Undefined function '" + node->callee + "'", node->line);
        // Still analyze arguments so nested errors are reported
        for (auto& arg : node->args) {
            analyzeExpr(arg.get());
        }
        return "unknown";
    }

    auto& fnInfo = it->second;

    // Arity check
    if (node->args.size() != fnInfo.paramTypes.size()) {
        recordError("Function '" + node->callee + "' expects " +
                    std::to_string(fnInfo.paramTypes.size()) + " argument(s), got " +
                    std::to_string(node->args.size()), node->line);
    }

    // Type-check each argument
    for (size_t i = 0; i < node->args.size(); ++i) {
        std::string argType = analyzeExpr(node->args[i].get());
        if (i < fnInfo.paramTypes.size()) {
            if (argType != "unknown" && fnInfo.paramTypes[i] != "unknown" &&
                argType != fnInfo.paramTypes[i]) {
                recordError("Argument " + std::to_string(i + 1) + " of '" + node->callee +
                            "' expects " + fnInfo.paramTypes[i] + ", got " + argType,
                            node->line);
            }
        }
    }

    // No return-type annotations in the grammar, so the result type is unknown
    return "unknown";
}

std::string SemanticAnalyzer::analyzeIdentExpr(IdentExprNode* node) {
    auto* sym = lookupVariable(node->name);
    if (!sym) {
        recordError("Undefined variable '" + node->name + "'", node->line);
        return "unknown";
    }
    return sym->type;
}
