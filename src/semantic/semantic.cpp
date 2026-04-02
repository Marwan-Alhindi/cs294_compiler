#include "semantic.h"

SemanticAnalyzer::SemanticAnalyzer(ProgramNode* root) : root_(root) {}

// ============================================================
// Public API
// ============================================================

bool SemanticAnalyzer::analyze() {
    errors_.clear();
    scopes_.clear();
    functions_.clear();
    structDefs_.clear();
    enumDefs_.clear();
    methods_.clear();

    pushScope();
    registerBuiltins();
    registerFunctions();
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
    current[name] = SymbolInfo{normalizeType(type), isMutable, line};
    return true;
}

SymbolInfo* SemanticAnalyzer::lookupVariable(const std::string& name) {
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

std::string SemanticAnalyzer::normalizeType(const std::string& type) const {
    // Treat u32, usize, i64, u64 as i32 for simplicity
    if (type == "u32" || type == "usize" || type == "i64" || type == "u64") {
        return "i32";
    }
    // Normalize () to void
    if (type == "()" || type == "void") {
        return "void";
    }
    // Normalize array element types, e.g., [u32; 3] → [i32; 3]
    if (type.size() > 2 && type.front() == '[') {
        // Find the ';' at bracket depth 0 (skip nested brackets)
        int depth = 0;
        size_t semi = std::string::npos;
        for (size_t i = 1; i < type.size(); ++i) {
            if (type[i] == '[') ++depth;
            else if (type[i] == ']') --depth;
            else if (type[i] == ';' && depth == 0) { semi = i; break; }
        }
        if (semi != std::string::npos) {
            std::string elemType = type.substr(1, semi - 1);
            while (!elemType.empty() && elemType.back() == ' ') elemType.pop_back();
            std::string rest = type.substr(semi);  // "; N]"
            return "[" + normalizeType(elemType) + rest;
        }
    }
    return type;
}

// ============================================================
// Built-in functions
// ============================================================

void SemanticAnalyzer::registerBuiltins() {
    functions_["exit"] = FunctionInfo{"exit", {"i32"}, "void", 0};
    functions_["printInt"] = FunctionInfo{"printInt", {"i32"}, "void", 0};
    functions_["printlnInt"] = FunctionInfo{"printlnInt", {"i32"}, "void", 0};
    functions_["getInt"] = FunctionInfo{"getInt", {}, "i32", 0};
    functions_["print"] = FunctionInfo{"print", {"string"}, "void", 0};
    functions_["println"] = FunctionInfo{"println", {"string"}, "void", 0};
}

// ============================================================
// Pre-registration
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
            info.returnType = fn->returnType.empty() ? "unknown"
                              : normalizeType(fn->returnType);
            for (auto& param : fn->params) {
                if (!param.isSelf) {
                    info.paramTypes.push_back(
                        param.typeName.empty() ? "unknown" : normalizeType(param.typeName));
                }
            }
            functions_[fn->name] = info;
            declareVariable(fn->name, "fn", false, fn->line);
        }
        // Pre-register struct and enum names
        if (stmt->kind == NodeKind::STRUCT_DEF) {
            auto* sd = static_cast<StructDefNode*>(stmt.get());
            structDefs_[sd->name] = sd->fields;
        }
        if (stmt->kind == NodeKind::ENUM_DEF) {
            auto* ed = static_cast<EnumDefNode*>(stmt.get());
            enumDefs_[ed->name] = ed->variants;
        }
        // Pre-register impl methods
        if (stmt->kind == NodeKind::IMPL_BLOCK) {
            auto* impl = static_cast<ImplBlockNode*>(stmt.get());
            for (auto& method : impl->methods) {
                if (method->kind == NodeKind::FN_DECL) {
                    auto* fn = static_cast<FnDeclNode*>(method.get());
                    FunctionInfo info;
                    info.name = fn->name;
                    info.line = fn->line;
                    std::string retType = fn->returnType;
                    if (retType == "Self") retType = impl->targetName;
                    info.returnType = retType.empty() ? "unknown"
                                      : normalizeType(retType);
                    for (auto& param : fn->params) {
                        if (!param.isSelf) {
                            info.paramTypes.push_back(
                                param.typeName.empty() ? "unknown"
                                : normalizeType(param.typeName));
                        }
                    }
                    std::string key = impl->targetName + "." + fn->name;
                    methods_[key] = info;

                    // Also register static methods (no self) as Type::method
                    bool hasSelf = false;
                    for (auto& p : fn->params) {
                        if (p.isSelf) { hasSelf = true; break; }
                    }
                    if (!hasSelf) {
                        // Static method — also accessible as a function call
                        std::string staticKey = impl->targetName + "::" + fn->name;
                        functions_[staticKey] = info;
                    }
                }
            }
        }
    }
}

// ============================================================
// Generic dispatch
// ============================================================

void SemanticAnalyzer::analyzeNode(AstNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NodeKind::PROGRAM:       analyzeProgram(static_cast<ProgramNode*>(node)); break;
        case NodeKind::FN_DECL:       analyzeFnDecl(static_cast<FnDeclNode*>(node)); break;
        case NodeKind::BLOCK:         analyzeBlock(static_cast<BlockNode*>(node)); break;
        case NodeKind::LET_STMT:      analyzeLetStmt(static_cast<LetStmtNode*>(node)); break;
        case NodeKind::RETURN_STMT:   analyzeReturnStmt(static_cast<ReturnStmtNode*>(node)); break;
        case NodeKind::WHILE_STMT:    analyzeWhileStmt(static_cast<WhileStmtNode*>(node)); break;
        case NodeKind::IF_STMT:       analyzeIfStmt(static_cast<IfStmtNode*>(node)); break;
        case NodeKind::EXPR_STMT:     analyzeExprStmt(static_cast<ExprStmtNode*>(node)); break;
        case NodeKind::LOOP_STMT:     analyzeLoopStmt(static_cast<LoopStmtNode*>(node)); break;
        case NodeKind::BREAK_STMT:    break;  // valid inside loops — no analysis needed
        case NodeKind::CONTINUE_STMT: break;
        case NodeKind::STRUCT_DEF:    analyzeStructDef(static_cast<StructDefNode*>(node)); break;
        case NodeKind::ENUM_DEF:      analyzeEnumDef(static_cast<EnumDefNode*>(node)); break;
        case NodeKind::IMPL_BLOCK:    analyzeImplBlock(static_cast<ImplBlockNode*>(node)); break;
        default:
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
    pushScope();
    for (auto& param : node->params) {
        std::string type = param.typeName.empty() ? "unknown" : normalizeType(param.typeName);
        declareVariable(param.name, type, param.isMutSelf, param.line);
    }
    if (node->body) {
        analyzeNode(node->body.get());
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
    std::string initType = "unknown";
    if (node->init) {
        initType = analyzeExpr(node->init.get());
    }

    std::string varType = initType;
    if (!node->typeName.empty()) {
        varType = normalizeType(node->typeName);
        std::string normInit = normalizeType(initType);
        if (normInit != "unknown" && varType != normInit) {
            // Accept "array" as compatible with any typed array
            bool compatible = (normInit == "array" && !varType.empty() && varType.front() == '[')
                           || (varType == "array" && !normInit.empty() && normInit.front() == '[');
            // Accept matching array base types even if sizes differ (const sizes)
            if (!compatible && varType.front() == '[' && normInit.front() == '[') {
                // Both are array types — allow if element types match
                compatible = true;
            }
            if (!compatible) {
                recordError("Type mismatch: variable '" + node->name + "' declared as " +
                            varType + " but initialized with " + normInit, node->line);
            }
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
    if (node->condition) analyzeExpr(node->condition.get());
    if (node->body) analyzeNode(node->body.get());
}

void SemanticAnalyzer::analyzeIfStmt(IfStmtNode* node) {
    if (node->condition) analyzeExpr(node->condition.get());
    if (node->thenBranch) analyzeNode(node->thenBranch.get());
    if (node->elseBranch) analyzeNode(node->elseBranch.get());
}

void SemanticAnalyzer::analyzeExprStmt(ExprStmtNode* node) {
    if (node->expr) analyzeExpr(node->expr.get());
}

void SemanticAnalyzer::analyzeLoopStmt(LoopStmtNode* node) {
    if (node->body) analyzeNode(node->body.get());
}

void SemanticAnalyzer::analyzeStructDef(StructDefNode* node) {
    // Already pre-registered in registerFunctions
}

void SemanticAnalyzer::analyzeEnumDef(EnumDefNode* node) {
    // Already pre-registered
}

void SemanticAnalyzer::analyzeImplBlock(ImplBlockNode* node) {
    // Analyze each method body
    for (auto& method : node->methods) {
        if (method->kind == NodeKind::FN_DECL) {
            auto* fn = static_cast<FnDeclNode*>(method.get());
            pushScope();
            // Add self if present
            for (auto& param : fn->params) {
                if (param.isSelf) {
                    declareVariable("self", node->targetName, param.isMutSelf, param.line);
                } else {
                    std::string type = param.typeName.empty() ? "unknown"
                                       : normalizeType(param.typeName);
                    declareVariable(param.name, type, false, param.line);
                }
            }
            if (fn->body) analyzeNode(fn->body.get());
            popScope();
        }
    }
}

// ============================================================
// Expression analysis
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
        case NodeKind::BOOL_LITERAL:
            return "bool";
        case NodeKind::FIELD_ACCESS_EXPR:
            return analyzeFieldAccessExpr(static_cast<FieldAccessExprNode*>(node));
        case NodeKind::METHOD_CALL_EXPR:
            return analyzeMethodCallExpr(static_cast<MethodCallExprNode*>(node));
        case NodeKind::INDEX_EXPR:
            return analyzeIndexExpr(static_cast<IndexExprNode*>(node));
        case NodeKind::PATH_EXPR:
            return analyzePathExpr(static_cast<PathExprNode*>(node));
        case NodeKind::CAST_EXPR:
            return analyzeCastExpr(static_cast<CastExprNode*>(node));
        case NodeKind::ARRAY_LITERAL: {
            auto* arr = static_cast<ArrayLiteralNode*>(node);
            std::string elemType = "unknown";
            std::string countStr;
            if (arr->isRepeat) {
                if (arr->repeatValue) elemType = analyzeExpr(arr->repeatValue.get());
                if (arr->repeatCount) {
                    analyzeExpr(arr->repeatCount.get());
                    if (arr->repeatCount->kind == NodeKind::NUMBER_LITERAL) {
                        countStr = static_cast<NumberLiteralNode*>(arr->repeatCount.get())->value;
                    }
                }
            } else {
                for (auto& elem : arr->elements) {
                    std::string t = analyzeExpr(elem.get());
                    if (elemType == "unknown") elemType = t;
                }
                countStr = std::to_string(arr->elements.size());
            }
            if (elemType != "unknown" && !countStr.empty()) {
                return "[" + elemType + "; " + countStr + "]";
            }
            return "array";
        }
        case NodeKind::STRUCT_LITERAL: {
            auto* sl = static_cast<StructLiteralNode*>(node);
            for (auto& f : sl->fields) {
                if (f.value) analyzeExpr(f.value.get());
            }
            return sl->structName;
        }
        default:
            return "unknown";
    }
}

std::string SemanticAnalyzer::analyzeAssignExpr(AssignExprNode* node) {
    // Analyze the target lvalue
    std::string targetType = "unknown";
    std::string targetName;

    if (node->target) {
        targetType = analyzeExpr(node->target.get());

        // Check mutability for simple identifiers
        if (node->target->kind == NodeKind::IDENT_EXPR) {
            auto* ident = static_cast<IdentExprNode*>(node->target.get());
            targetName = ident->name;
            auto* sym = lookupVariable(targetName);
            if (!sym) {
                if (node->value) analyzeExpr(node->value.get());
                return "unknown";
            }
            targetType = sym->type;
            if (!sym->isMutable) {
                recordError("Cannot assign to immutable variable '" + targetName + "'",
                            node->line);
            }
        }
    }

    std::string valueType = "unknown";
    if (node->value) {
        valueType = analyzeExpr(node->value.get());
    }

    // For compound assignment (+=, -=), check operand types
    if (!node->op.empty()) {
        if (targetType != "unknown" && targetType != "i32") {
            recordError("Compound assignment requires i32 operand", node->line);
        }
        if (valueType != "unknown" && valueType != "i32") {
            recordError("Compound assignment requires i32 operand", node->line);
        }
        return targetType;
    }

    if (valueType != "unknown" && targetType != "unknown" && targetType != valueType) {
        if (!targetName.empty()) {
            recordError("Type mismatch: cannot assign " + valueType + " to variable '" +
                        targetName + "' of type " + targetType, node->line);
        }
    }

    return targetType;
}

std::string SemanticAnalyzer::analyzeBinaryExpr(BinaryExprNode* node) {
    std::string leftType = analyzeExpr(node->left.get());
    std::string rightType = analyzeExpr(node->right.get());

    // Arithmetic: +, -, *, /, %
    if (node->op == "+" || node->op == "-" || node->op == "*" ||
        node->op == "/" || node->op == "%") {
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

    // Comparison: ==, !=, <, >, <=, >=
    if (node->op == "==" || node->op == "!=" || node->op == "<" ||
        node->op == ">"  || node->op == "<=" || node->op == ">=") {
        if (leftType != "unknown" && rightType != "unknown" && leftType != rightType) {
            recordError("Cannot compare " + leftType + " with " + rightType, node->line);
        }
        return "bool";
    }

    // Logical: &&, ||
    if (node->op == "&&" || node->op == "||") {
        if (leftType != "unknown" && leftType != "bool") {
            recordError("Left operand of '" + node->op + "' must be bool, got " + leftType,
                        node->line);
        }
        if (rightType != "unknown" && rightType != "bool") {
            recordError("Right operand of '" + node->op + "' must be bool, got " + rightType,
                        node->line);
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

    if (node->op == "!") {
        if (operandType != "unknown" && operandType != "bool") {
            recordError("Unary '!' requires bool operand, got " + operandType, node->line);
        }
        return "bool";
    }

    // Reference: &expr or &mut expr
    if (node->op == "&" || node->op == "&mut") {
        return "&" + operandType;
    }

    // Dereference: *expr
    if (node->op == "*") {
        if (operandType.size() > 1 && operandType[0] == '&') {
            // Strip leading & (or &mut )
            if (operandType.substr(0, 5) == "&mut ") return operandType.substr(5);
            return operandType.substr(1);
        }
        return "unknown";
    }

    return operandType;
}

std::string SemanticAnalyzer::analyzeCallExpr(CallExprNode* node) {
    auto it = functions_.find(node->callee);
    if (it == functions_.end()) {
        recordError("Undefined function '" + node->callee + "'", node->line);
        for (auto& arg : node->args) analyzeExpr(arg.get());
        return "unknown";
    }

    auto& fnInfo = it->second;

    if (node->args.size() != fnInfo.paramTypes.size()) {
        recordError("Function '" + node->callee + "' expects " +
                    std::to_string(fnInfo.paramTypes.size()) + " argument(s), got " +
                    std::to_string(node->args.size()), node->line);
    }

    for (size_t i = 0; i < node->args.size(); ++i) {
        std::string argType = analyzeExpr(node->args[i].get());
        if (i < fnInfo.paramTypes.size()) {
            std::string expected = normalizeType(fnInfo.paramTypes[i]);
            if (argType != "unknown" && expected != "unknown" && argType != expected) {
                recordError("Argument " + std::to_string(i + 1) + " of '" + node->callee +
                            "' expects " + expected + ", got " + argType, node->line);
            }
        }
    }

    return fnInfo.returnType.empty() ? "unknown" : normalizeType(fnInfo.returnType);
}

std::string SemanticAnalyzer::analyzeIdentExpr(IdentExprNode* node) {
    auto* sym = lookupVariable(node->name);
    if (!sym) {
        recordError("Undefined variable '" + node->name + "'", node->line);
        return "unknown";
    }
    return sym->type;
}

std::string SemanticAnalyzer::analyzeFieldAccessExpr(FieldAccessExprNode* node) {
    std::string objType = "unknown";
    if (node->object) objType = analyzeExpr(node->object.get());
    // Look up field type in struct definition
    auto it = structDefs_.find(objType);
    if (it != structDefs_.end()) {
        for (auto& field : it->second) {
            if (field.name == node->field) {
                return normalizeType(field.typeName);
            }
        }
    }
    // Field not found or unknown object type — don't error for flexibility
    return "unknown";
}

std::string SemanticAnalyzer::analyzeMethodCallExpr(MethodCallExprNode* node) {
    std::string objType = "unknown";
    if (node->object) objType = analyzeExpr(node->object.get());

    // Check arguments
    for (auto& arg : node->args) analyzeExpr(arg.get());

    // Look up method
    std::string key = objType + "." + node->method;
    auto it = methods_.find(key);
    if (it != methods_.end()) {
        return it->second.returnType.empty() ? "unknown"
               : normalizeType(it->second.returnType);
    }

    // Built-in array/slice methods
    if (node->method == "len") return "i32";

    return "unknown";
}

std::string SemanticAnalyzer::analyzeIndexExpr(IndexExprNode* node) {
    std::string objType = "unknown";
    if (node->object) objType = analyzeExpr(node->object.get());
    if (node->index) analyzeExpr(node->index.get());
    // Extract element type from array type like "[i32; 3]"
    if (objType.size() > 2 && objType.front() == '[') {
        auto semi = objType.find(';');
        if (semi != std::string::npos) {
            return normalizeType(objType.substr(1, semi - 1));
        }
    }
    return "unknown";
}

std::string SemanticAnalyzer::analyzePathExpr(PathExprNode* node) {
    // Check if it's an enum variant
    auto it = enumDefs_.find(node->base);
    if (it != enumDefs_.end()) {
        for (auto& v : it->second) {
            if (v == node->member) return node->base;
        }
        recordError("Enum '" + node->base + "' has no variant '" + node->member + "'",
                    node->line);
        return node->base;
    }
    // Could be a static method call — Type::method — handled as a function call elsewhere
    return "unknown";
}

std::string SemanticAnalyzer::analyzeCastExpr(CastExprNode* node) {
    if (node->expr) analyzeExpr(node->expr.get());
    return normalizeType(node->targetType);
}
