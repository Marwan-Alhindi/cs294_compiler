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
    methodNeedsMut_.clear();
    constValues_.clear();

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
    // Allow variable shadowing in same scope (Rust's `let` re-binding)
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
    if (type == "usize") return "usize";
    if (type == "u32" || type == "i64" || type == "u64" ||
        type == "u8" || type == "i8" || type == "u16" || type == "i16" ||
        type == "char" || type == "f32" || type == "f64") {
        return "i32";
    }
    if (type == "String") return "string";
    if (type == "()" || type == "void") {
        return "void";
    }
    // Normalize reference types: &mut T -> normalize inner
    if (type.size() > 5 && type.substr(0, 5) == "&mut ") {
        return "&mut " + normalizeType(type.substr(5));
    }
    if (type.size() > 1 && type[0] == '&' && type[1] != 'm') {
        return "&" + normalizeType(type.substr(1));
    }
    // Normalize array element types
    if (type.size() > 2 && type.front() == '[') {
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
            std::string rest = type.substr(semi);
            return "[" + normalizeType(elemType) + rest;
        }
    }
    return type;
}

std::string SemanticAnalyzer::stripRef(const std::string& type) const {
    if (type.size() > 5 && type.substr(0, 5) == "&mut ") return type.substr(5);
    if (type.size() > 1 && type[0] == '&') return type.substr(1);
    return type;
}

bool SemanticAnalyzer::typesCompatible(const std::string& expected, const std::string& actual) const {
    if (expected == actual) return true;
    if (expected == "unknown" || actual == "unknown") return true;

    // Strip references and compare base types
    std::string baseExp = stripRef(normalizeType(expected));
    std::string baseAct = stripRef(normalizeType(actual));
    if (baseExp == baseAct) return true;

    // Both array types — compare element types and sizes (resolving const names)
    if (!baseExp.empty() && baseExp.front() == '[' && !baseAct.empty() && baseAct.front() == '[') {
        if (baseExp == baseAct) return true;
        // Extract element types and sizes, compare recursively
        auto extractParts = [](const std::string& arr) -> std::pair<std::string, std::string> {
            int depth = 0;
            size_t semi = std::string::npos;
            for (size_t i = 1; i < arr.size(); ++i) {
                if (arr[i] == '[') ++depth;
                else if (arr[i] == ']') --depth;
                else if (arr[i] == ';' && depth == 0) { semi = i; break; }
            }
            if (semi == std::string::npos) return {"", ""};
            std::string elem = arr.substr(1, semi - 1);
            while (!elem.empty() && elem.back() == ' ') elem.pop_back();
            std::string size = arr.substr(semi + 1);
            while (!size.empty() && size.front() == ' ') size = size.substr(1);
            if (!size.empty() && size.back() == ']') size.pop_back();
            return {elem, size};
        };
        auto [elemExp, sizeExp] = extractParts(baseExp);
        auto [elemAct, sizeAct] = extractParts(baseAct);
        // Resolve const names in sizes
        auto resolveSize = [this](const std::string& s) -> std::string {
            for (auto& kv : constValues_) {
                if (s == kv.first) return kv.second;
            }
            return s;
        };
        if (resolveSize(sizeExp) == resolveSize(sizeAct) &&
            typesCompatible(elemExp, elemAct)) {
            return true;
        }
        return false;
    }

    // i32 and usize are compatible for assignments and function args
    auto isIntLike = [](const std::string& t) {
        return t == "i32" || t == "usize";
    };
    if (isIntLike(baseExp) && isIntLike(baseAct)) return true;

    // "array" is compatible with any typed array
    if (baseExp == "array" || baseAct == "array") return true;

    // &str ≈ string ≈ String
    auto isStr = [](const std::string& t) {
        return t == "string" || t == "String" || t == "&str" || t == "str";
    };
    if (isStr(baseExp) && isStr(baseAct)) return true;

    return false;
}

bool SemanticAnalyzer::isMutableLvalue(AstNode* node) {
    if (!node) return false;
    if (node->kind == NodeKind::IDENT_EXPR) {
        auto* ident = static_cast<IdentExprNode*>(node);
        auto* sym = lookupVariable(ident->name);
        if (!sym) return false;
        // Mutable if binding is mut, or type is &mut T
        return sym->isMutable ||
               (sym->type.size() > 5 && sym->type.substr(0, 5) == "&mut ");
    }
    if (node->kind == NodeKind::FIELD_ACCESS_EXPR) {
        auto* fa = static_cast<FieldAccessExprNode*>(node);
        return isMutableLvalue(fa->object.get());
    }
    if (node->kind == NodeKind::INDEX_EXPR) {
        auto* idx = static_cast<IndexExprNode*>(node);
        return isMutableLvalue(idx->object.get());
    }
    if (node->kind == NodeKind::UNARY_EXPR) {
        auto* un = static_cast<UnaryExprNode*>(node);
        if (un->op == "*") return true;  // dereferenced mut ref
    }
    return false;
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
// Helper: check if an AST subtree contains a return-with-value statement
// ============================================================

bool SemanticAnalyzer::hasReturnValue(AstNode* node) const {
    if (!node) return false;
    if (node->kind == NodeKind::RETURN_STMT) {
        auto* ret = static_cast<ReturnStmtNode*>(node);
        return ret->value != nullptr;
    }
    if (node->kind == NodeKind::BLOCK) {
        auto* block = static_cast<BlockNode*>(node);
        for (auto& s : block->statements) {
            if (hasReturnValue(s.get())) return true;
        }
    }
    if (node->kind == NodeKind::IF_STMT) {
        auto* ifn = static_cast<IfStmtNode*>(node);
        if (hasReturnValue(ifn->thenBranch.get())) return true;
        if (hasReturnValue(ifn->elseBranch.get())) return true;
    }
    if (node->kind == NodeKind::WHILE_STMT) {
        auto* wh = static_cast<WhileStmtNode*>(node);
        if (hasReturnValue(wh->body.get())) return true;
    }
    if (node->kind == NodeKind::LOOP_STMT) {
        auto* lp = static_cast<LoopStmtNode*>(node);
        if (hasReturnValue(lp->body.get())) return true;
    }
    return false;
}

// ============================================================
// Pre-registration
// ============================================================

void SemanticAnalyzer::registerFunctions() {
    for (auto& stmt : root_->statements) {
        // Register const values
        if (stmt->kind == NodeKind::LET_STMT) {
            auto* let = static_cast<LetStmtNode*>(stmt.get());
            if (!let->isMut && let->init && let->init->kind == NodeKind::NUMBER_LITERAL) {
                constValues_[let->name] = static_cast<NumberLiteralNode*>(let->init.get())->value;
            }
        }
        if (stmt->kind == NodeKind::FN_DECL) {
            auto* fn = static_cast<FnDeclNode*>(stmt.get());
            if (functions_.count(fn->name)) {
                recordError("Redefinition of function '" + fn->name + "'", fn->line);
                continue;
            }
            FunctionInfo info;
            info.name = fn->name;
            info.line = fn->line;
            if (!fn->returnType.empty()) {
                info.returnType = normalizeType(fn->returnType);
            } else if (hasReturnValue(fn->body.get())) {
                info.returnType = "unknown";
            } else {
                info.returnType = "void";
            }
            for (auto& param : fn->params) {
                if (!param.isSelf) {
                    info.paramTypes.push_back(
                        param.typeName.empty() ? "unknown" : normalizeType(param.typeName));
                }
            }
            functions_[fn->name] = info;
            declareVariable(fn->name, "fn", false, fn->line);
        }
        if (stmt->kind == NodeKind::STRUCT_DEF) {
            auto* sd = static_cast<StructDefNode*>(stmt.get());
            structDefs_[sd->name] = sd->fields;
        }
        if (stmt->kind == NodeKind::ENUM_DEF) {
            auto* ed = static_cast<EnumDefNode*>(stmt.get());
            enumDefs_[ed->name] = ed->variants;
        }
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
                    if (!retType.empty()) {
                        info.returnType = normalizeType(retType);
                    } else if (hasReturnValue(fn->body.get())) {
                        info.returnType = "unknown";
                    } else {
                        info.returnType = "void";
                    }
                    bool hasSelf = false;
                    bool needsMut = false;
                    for (auto& param : fn->params) {
                        if (param.isSelf) {
                            hasSelf = true;
                            needsMut = param.isMutSelf;
                        } else {
                            info.paramTypes.push_back(
                                param.typeName.empty() ? "unknown"
                                : normalizeType(param.typeName));
                        }
                    }
                    std::string key = impl->targetName + "." + fn->name;
                    methods_[key] = info;
                    methodNeedsMut_[key] = needsMut;

                    if (!hasSelf) {
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
        case NodeKind::BREAK_STMT:
            if (loopDepth_ <= 0) {
                recordError("'break' statement not in loop or while", node->line);
            }
            break;
        case NodeKind::CONTINUE_STMT:
            if (loopDepth_ <= 0) {
                recordError("'continue' statement not in loop or while", node->line);
            }
            break;
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
    // Register nested definitions (functions, structs, enums) in this scope
    if (node->body && node->body->kind == NodeKind::BLOCK) {
        auto* block = static_cast<BlockNode*>(node->body.get());
        for (auto& stmt : block->statements) {
            if (stmt && stmt->kind == NodeKind::STRUCT_DEF) {
                auto* sd = static_cast<StructDefNode*>(stmt.get());
                structDefs_[sd->name] = sd->fields;
            }
            if (stmt && stmt->kind == NodeKind::ENUM_DEF) {
                auto* ed = static_cast<EnumDefNode*>(stmt.get());
                enumDefs_[ed->name] = ed->variants;
            }
            if (stmt && stmt->kind == NodeKind::FN_DECL) {
                auto* fn = static_cast<FnDeclNode*>(stmt.get());
                if (!functions_.count(fn->name)) {
                    FunctionInfo info;
                    info.name = fn->name;
                    info.line = fn->line;
                    if (!fn->returnType.empty()) {
                        info.returnType = normalizeType(fn->returnType);
                    } else if (hasReturnValue(fn->body.get())) {
                        info.returnType = "unknown";
                    } else {
                        info.returnType = "void";
                    }
                    for (auto& param : fn->params) {
                        if (!param.isSelf) {
                            info.paramTypes.push_back(
                                param.typeName.empty() ? "unknown" : normalizeType(param.typeName));
                        }
                    }
                    functions_[fn->name] = info;
                }
            }
        }
    }
    // Check: main function must not declare a return type (or only "()")
    if (node->name == "main" && !node->returnType.empty() &&
        node->returnType != "()" && node->returnType != "void") {
        recordError("'main' function must return '()' or have no return type, got '" +
                    node->returnType + "'", node->line);
    }

    std::string prevFnName = currentFnName_;
    currentFnName_ = node->name;
    std::string prevReturnType = currentReturnType_;
    currentReturnType_ = node->returnType.empty() ? "void" : normalizeType(node->returnType);

    // Check for duplicate param names before declaring
    std::unordered_map<std::string, bool> seenParams;
    for (auto& param : node->params) {
        if (seenParams.count(param.name)) {
            recordError("Redeclaration of '" + param.name + "' in the same scope", param.line);
        }
        seenParams[param.name] = true;
        std::string type = param.typeName.empty() ? "unknown" : normalizeType(param.typeName);
        // Mutable if: mut keyword, &mut self, or &mut reference type
        bool mut = param.isMut || param.isMutSelf ||
                   (type.size() > 5 && type.substr(0, 5) == "&mut ");
        declareVariable(param.name, type, mut, param.line);
    }
    if (node->body) {
        analyzeNode(node->body.get());
    }

    // Check implicit return: if function has a non-void return type,
    // verify the last statement in the body
    if (currentReturnType_ != "void" && currentReturnType_ != "unknown" &&
        node->body && node->body->kind == NodeKind::BLOCK) {
        auto* block = static_cast<BlockNode*>(node->body.get());
        if (!block->statements.empty()) {
            auto* last = block->statements.back().get();
            if (last && last->kind == NodeKind::EXPR_STMT) {
                // Implicit return expression — check type captured during analysis
                if (lastExprType_ != "unknown" && lastExprType_ != "void" &&
                    !typesCompatible(currentReturnType_, lastExprType_)) {
                    recordError("Implicit return type mismatch: expected " +
                                currentReturnType_ + ", got " + lastExprType_, last->line);
                }
            } else if (last && last->kind != NodeKind::RETURN_STMT &&
                       last->kind != NodeKind::LOOP_STMT) {
                // Last statement is not a return or loop — check for missing return
                bool needsReturn = false;
                if (last->kind == NodeKind::IF_STMT) {
                    auto* ifStmt = static_cast<IfStmtNode*>(last);
                    if (!ifStmt->elseBranch) {
                        needsReturn = true;
                    } else {
                        // if-else with else — check last expr type against return type
                        if (lastExprType_ != "unknown" && lastExprType_ != "void" &&
                            !typesCompatible(currentReturnType_, lastExprType_)) {
                            recordError("Implicit return type mismatch: expected " +
                                        currentReturnType_ + ", got " + lastExprType_,
                                        last->line);
                        }
                    }
                } else if (last->kind == NodeKind::WHILE_STMT) {
                    // While loop might contain returns, don't flag
                } else if (!hasReturnValue(node->body.get())) {
                    needsReturn = true;
                }
                if (needsReturn) {
                    recordError("Missing return value in function '" + node->name +
                                "' with return type " + currentReturnType_, node->line);
                }
            }
        } else {
            // Empty body with non-void return type
            recordError("Missing return value in function '" + node->name +
                        "' with return type " + currentReturnType_, node->line);
        }
    }

    currentReturnType_ = prevReturnType;
    currentFnName_ = prevFnName;
    popScope();
}

void SemanticAnalyzer::analyzeBlock(BlockNode* node) {
    pushScope();
    lastExprType_ = "void";
    bool afterExit = false;
    for (auto& stmt : node->statements) {
        if (afterExit) {
            recordError("Unreachable code after exit()", stmt->line);
            break;
        }
        analyzeNode(stmt.get());
        // Mark unreachable only after exit() — not after return
        // (unreachable after return is only a warning in Rust)
        if (stmt->kind == NodeKind::EXPR_STMT) {
            auto* es = static_cast<ExprStmtNode*>(stmt.get());
            if (es->expr && es->expr->kind == NodeKind::CALL_EXPR) {
                auto* call = static_cast<CallExprNode*>(es->expr.get());
                if (call->callee == "exit") afterExit = true;
            }
        }
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
        if (!typesCompatible(varType, normInit)) {
            recordError("Type mismatch: variable '" + node->name + "' declared as " +
                        varType + " but initialized with " + normInit, node->line);
        }
    }

    declareVariable(node->name, varType, node->isMut, node->line);
}

void SemanticAnalyzer::analyzeReturnStmt(ReturnStmtNode* node) {
    if (node->value) {
        std::string valType = analyzeExpr(node->value.get());
        // Only check return type mismatch if function has explicit non-void return type
        if (!currentReturnType_.empty() && currentReturnType_ != "unknown" &&
            currentReturnType_ != "void" && valType != "unknown" &&
            !typesCompatible(currentReturnType_, valType)) {
            recordError("Return type mismatch: expected " + currentReturnType_ +
                        ", got " + valType, node->line);
        }
    }
}

void SemanticAnalyzer::analyzeWhileStmt(WhileStmtNode* node) {
    if (node->condition) analyzeExpr(node->condition.get());
    loopDepth_++;
    if (node->body) analyzeNode(node->body.get());
    loopDepth_--;
}

void SemanticAnalyzer::analyzeIfStmt(IfStmtNode* node) {
    if (node->condition) analyzeExpr(node->condition.get());
    if (node->thenBranch) analyzeNode(node->thenBranch.get());
    if (node->elseBranch) analyzeNode(node->elseBranch.get());
}

void SemanticAnalyzer::analyzeExprStmt(ExprStmtNode* node) {
    if (node->expr) {
        lastExprType_ = analyzeExpr(node->expr.get());
    }
}

void SemanticAnalyzer::analyzeLoopStmt(LoopStmtNode* node) {
    loopDepth_++;
    if (node->body) analyzeNode(node->body.get());
    loopDepth_--;
}

void SemanticAnalyzer::analyzeStructDef(StructDefNode* node) {}

void SemanticAnalyzer::analyzeEnumDef(EnumDefNode* node) {}

void SemanticAnalyzer::analyzeImplBlock(ImplBlockNode* node) {
    std::string prevImplType = currentImplType_;
    bool prevInImpl = inImplBlock_;
    currentImplType_ = node->targetName;
    inImplBlock_ = true;
    for (auto& method : node->methods) {
        if (method->kind == NodeKind::FN_DECL) {
            auto* fn = static_cast<FnDeclNode*>(method.get());
            pushScope();
            std::string prevReturnType = currentReturnType_;
            std::string retType = fn->returnType;
            if (retType == "Self") retType = node->targetName;
            currentReturnType_ = retType.empty() ? "void" : normalizeType(retType);
            for (auto& param : fn->params) {
                if (param.isSelf) {
                    declareVariable("self", node->targetName, param.isMutSelf, param.line);
                } else {
                    std::string type = param.typeName.empty() ? "unknown"
                                       : normalizeType(param.typeName);
                    bool mut = param.isMut ||
                               (type.size() > 5 && type.substr(0, 5) == "&mut ");
                    declareVariable(param.name, type, mut, param.line);
                }
            }
            if (fn->body) analyzeNode(fn->body.get());
            // Check implicit return type for impl methods
            if (currentReturnType_ != "void" && currentReturnType_ != "unknown" &&
                fn->body && fn->body->kind == NodeKind::BLOCK) {
                auto* block = static_cast<BlockNode*>(fn->body.get());
                if (!block->statements.empty()) {
                    auto* last = block->statements.back().get();
                    if (last && last->kind == NodeKind::EXPR_STMT) {
                        if (lastExprType_ != "unknown" &&
                            !typesCompatible(currentReturnType_, lastExprType_)) {
                            recordError("Implicit return type mismatch: expected " +
                                        currentReturnType_ + ", got " + lastExprType_,
                                        last->line);
                        }
                    } else if (last && last->kind != NodeKind::RETURN_STMT &&
                               last->kind != NodeKind::LOOP_STMT) {
                        bool needsReturn = false;
                        if (last->kind == NodeKind::IF_STMT) {
                            auto* ifStmt = static_cast<IfStmtNode*>(last);
                            if (!ifStmt->elseBranch) {
                                needsReturn = true;
                            } else {
                                if (lastExprType_ != "unknown" && lastExprType_ != "void" &&
                                    !typesCompatible(currentReturnType_, lastExprType_)) {
                                    recordError("Implicit return type mismatch: expected " +
                                                currentReturnType_ + ", got " + lastExprType_,
                                                last->line);
                                }
                            }
                        } else if (last->kind == NodeKind::WHILE_STMT) {
                            // While loop might contain returns
                        } else if (!hasReturnValue(fn->body.get())) {
                            needsReturn = true;
                        }
                        if (needsReturn) {
                            recordError("Missing return value in method '" + fn->name +
                                        "' with return type " + currentReturnType_, fn->line);
                        }
                    }
                }
            }
            currentReturnType_ = prevReturnType;
            popScope();
        }
    }
    currentImplType_ = prevImplType;
    inImplBlock_ = prevInImpl;
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
        case NodeKind::NUMBER_LITERAL: {
            auto* numNode = static_cast<NumberLiteralNode*>(node);
            // Check for integer literal overflow
            // Only flag values that exceed u32 max (4294967295)
            // Values between i32 max and u32 max are valid for u32 context
            if (!numNode->value.empty() && numNode->value[0] != '\'') {
                try {
                    unsigned long long val = std::stoull(numNode->value);
                    if (val > 4294967295ULL) {
                        recordError("Integer literal overflow: " + numNode->value +
                                    " is out of range", node->line);
                    }
                } catch (...) {}
            }
            return "i32";
        }
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
                    else if (t != "unknown" && !typesCompatible(elemType, t)) {
                        recordError("Array element type mismatch: expected " + elemType +
                                    ", got " + t, arr->line);
                    }
                }
                countStr = std::to_string(arr->elements.size());
            }
            if (elemType != "unknown" && !countStr.empty()) {
                return "[" + normalizeType(elemType) + "; " + countStr + "]";
            }
            return "array";
        }
        case NodeKind::STRUCT_LITERAL: {
            auto* sl = static_cast<StructLiteralNode*>(node);
            auto defIt = structDefs_.find(sl->structName);
            for (auto& f : sl->fields) {
                std::string valType = "unknown";
                if (f.value) valType = analyzeExpr(f.value.get());
                // Check field type against struct definition
                if (defIt != structDefs_.end()) {
                    for (auto& defField : defIt->second) {
                        if (defField.name == f.name) {
                            std::string expected = normalizeType(defField.typeName);
                            if (valType != "unknown" && !typesCompatible(expected, valType)) {
                                recordError("Field '" + f.name + "' expects " + expected +
                                            ", got " + valType, sl->line);
                            }
                            break;
                        }
                    }
                }
            }
            // Check for missing fields
            if (defIt != structDefs_.end()) {
                for (auto& defField : defIt->second) {
                    bool found = false;
                    for (auto& f : sl->fields) {
                        if (f.name == defField.name) { found = true; break; }
                    }
                    if (!found) {
                        recordError("Missing field '" + defField.name + "' in struct literal '" +
                                    sl->structName + "'", sl->line);
                    }
                }
            }
            return sl->structName;
        }
        case NodeKind::IF_STMT: {
            auto* ifn = static_cast<IfStmtNode*>(node);
            if (ifn->condition) analyzeExpr(ifn->condition.get());
            std::string thenType = "void";
            std::string elseType = "void";
            if (ifn->thenBranch) {
                analyzeNode(ifn->thenBranch.get());
                thenType = lastExprType_;
            }
            if (ifn->elseBranch) {
                analyzeNode(ifn->elseBranch.get());
                elseType = lastExprType_;
            }
            // Check branch type consistency
            if (thenType != "void" && elseType != "void" &&
                thenType != "unknown" && elseType != "unknown" &&
                !typesCompatible(thenType, elseType)) {
                recordError("if-else branches have incompatible types: " +
                            thenType + " vs " + elseType, node->line);
            }
            return thenType != "void" ? thenType : elseType;
        }
        case NodeKind::BLOCK: {
            analyzeBlock(static_cast<BlockNode*>(node));
            return lastExprType_;
        }
        default:
            return "unknown";
    }
}

std::string SemanticAnalyzer::analyzeAssignExpr(AssignExprNode* node) {
    std::string targetType = "unknown";
    std::string targetName;

    if (node->target) {
        targetType = analyzeExpr(node->target.get());

        // Check mutability for any lvalue
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
        } else if (node->target->kind == NodeKind::INDEX_EXPR ||
                   node->target->kind == NodeKind::FIELD_ACCESS_EXPR) {
            // Check mutability of the root object
            if (!isMutableLvalue(node->target.get())) {
                recordError("Cannot assign to immutable value", node->line);
            }
        }
    }

    std::string valueType = "unknown";
    if (node->value) {
        valueType = analyzeExpr(node->value.get());
    }

    if (!node->op.empty()) {
        return targetType;
    }

    if (!typesCompatible(targetType, valueType)) {
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

    // Arithmetic + bitwise + shift: +, -, *, /, %, &, |, ^, <<, >>
    if (node->op == "+" || node->op == "-" || node->op == "*" ||
        node->op == "/" || node->op == "%" ||
        node->op == "&" || node->op == "|" || node->op == "^" ||
        node->op == "<<" || node->op == ">>") {
        bool leftIsInt = (leftType == "i32" || leftType == "usize");
        bool rightIsInt = (rightType == "i32" || rightType == "usize");
        if (leftType != "unknown" && !leftIsInt) {
            recordError("Left operand of '" + node->op + "' must be i32, got " + leftType,
                        node->line);
        }
        if (rightType != "unknown" && !rightIsInt) {
            recordError("Right operand of '" + node->op + "' must be i32, got " + rightType,
                        node->line);
        }
        // Check for mixed i32/usize (allow if one side is a literal)
        if (leftType != "unknown" && rightType != "unknown" &&
            leftIsInt && rightIsInt && leftType != rightType) {
            bool leftIsLit = node->left && node->left->kind == NodeKind::NUMBER_LITERAL;
            bool rightIsLit = node->right && node->right->kind == NodeKind::NUMBER_LITERAL;
            if (!leftIsLit && !rightIsLit) {
                recordError("Cannot mix " + leftType + " and " + rightType +
                            " in arithmetic", node->line);
            }
        }
        if (leftType == "usize" || rightType == "usize") return "usize";
        return "i32";
    }

    // Comparison: ==, !=, <, >, <=, >=
    if (node->op == "==" || node->op == "!=" || node->op == "<" ||
        node->op == ">"  || node->op == "<=" || node->op == ">=") {
        if (leftType != "unknown" && rightType != "unknown" && leftType != rightType) {
            // Allow i32/usize comparison (implicit coercion)
            bool bothInt = (leftType == "i32" || leftType == "usize") &&
                           (rightType == "i32" || rightType == "usize");
            if (!bothInt) {
                recordError("Cannot compare " + leftType + " with " + rightType, node->line);
            }
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
    bool prevInUnaryMinus = inUnaryMinus_;
    if (node->op == "-") inUnaryMinus_ = true;
    std::string operandType = analyzeExpr(node->operand.get());
    inUnaryMinus_ = prevInUnaryMinus;

    if (node->op == "-") {
        if (operandType != "unknown" && operandType != "i32" && operandType != "usize") {
            recordError("Unary '-' requires i32 operand, got " + operandType, node->line);
        }
        return operandType == "usize" ? "usize" : "i32";
    }
    if (node->op == "!") {
        if (operandType != "unknown" && operandType != "bool" &&
            operandType != "i32" && operandType != "usize") {
            recordError("Unary '!' requires bool or i32 operand, got " + operandType, node->line);
        }
        // ! on bool returns bool, ! on i32/usize returns same type (bitwise NOT)
        if (operandType == "i32" || operandType == "usize") return operandType;
        return "bool";
    }

    // Reference: &expr produces &T, &mut expr produces &mut T
    if (node->op == "&") return "&" + operandType;
    if (node->op == "&mut") return "&mut " + operandType;

    // Dereference: *expr
    if (node->op == "*") {
        if (operandType.size() > 5 && operandType.substr(0, 5) == "&mut ") {
            return operandType.substr(5);
        }
        if (operandType.size() > 1 && operandType[0] == '&') {
            return operandType.substr(1);
        }
        if (operandType != "unknown") {
            recordError("Cannot dereference non-reference type " + operandType, node->line);
        }
        return "unknown";
    }

    return operandType;
}

std::string SemanticAnalyzer::analyzeCallExpr(CallExprNode* node) {
    // Resolve Self:: to current impl type
    std::string callee = node->callee;
    if (callee.substr(0, 6) == "Self::" && !currentImplType_.empty()) {
        callee = currentImplType_ + "::" + callee.substr(6);
    }
    auto it = functions_.find(callee);
    if (it == functions_.end()) {
        recordError("Undefined function '" + node->callee + "'", node->line);
        for (auto& arg : node->args) analyzeExpr(arg.get());
        return "unknown";
    }

    // exit() can only be called from the top-level main function
    if (callee == "exit" && (currentFnName_ != "main" || inImplBlock_)) {
        recordError("'exit()' can only be called from the main function", node->line);
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
            if (!typesCompatible(expected, argType)) {
                recordError("Argument " + std::to_string(i + 1) + " of '" + node->callee +
                            "' expects " + expected + ", got " + argType, node->line);
            }
            // Check integer literal overflow for i32 parameters
            if (expected == "i32" && node->args[i] &&
                node->args[i]->kind == NodeKind::NUMBER_LITERAL) {
                auto* num = static_cast<NumberLiteralNode*>(node->args[i].get());
                try {
                    long long val = std::stoll(num->value);
                    if (val > 2147483647LL || val < -2147483648LL) {
                        recordError("Integer literal " + num->value +
                                    " overflows i32 parameter", node->line);
                    }
                } catch (...) {}
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
    // Strip reference for field lookup
    std::string baseType = stripRef(objType);
    auto it = structDefs_.find(baseType);
    if (it != structDefs_.end()) {
        for (auto& field : it->second) {
            if (field.name == node->field) {
                return normalizeType(field.typeName);
            }
        }
        // Known struct but field not found
        recordError("Struct '" + baseType + "' has no field '" + node->field + "'",
                    node->line);
        return "unknown";
    }
    return "unknown";
}

std::string SemanticAnalyzer::analyzeMethodCallExpr(MethodCallExprNode* node) {
    std::string objType = "unknown";
    if (node->object) objType = analyzeExpr(node->object.get());

    for (auto& arg : node->args) analyzeExpr(arg.get());

    // Strip reference for method lookup
    std::string baseType = stripRef(objType);

    std::string key = baseType + "." + node->method;
    auto it = methods_.find(key);
    if (it != methods_.end()) {
        // Check if method needs &mut self but object is immutable
        auto mutIt = methodNeedsMut_.find(key);
        if (mutIt != methodNeedsMut_.end() && mutIt->second) {
            // Method needs &mut self — check if object is mutable or &mut ref
            bool isMutRef = (objType.size() > 5 && objType.substr(0, 5) == "&mut ");
            if (!isMutRef && node->object && !isMutableLvalue(node->object.get())) {
                recordError("Cannot call method requiring &mut self on immutable value",
                            node->line);
            }
        }
        return it->second.returnType.empty() ? "unknown"
               : normalizeType(it->second.returnType);
    }

    if (node->method == "len") return "i32";
    if (node->method == "to_string") return "string";
    if (node->method == "abs") return "i32";
    if (node->method == "clone") return baseType;
    if (node->method == "is_empty") return "bool";
    if (node->method == "push" || node->method == "pop" ||
        node->method == "clear" || node->method == "insert" ||
        node->method == "remove") return "void";

    return "unknown";
}

std::string SemanticAnalyzer::analyzeIndexExpr(IndexExprNode* node) {
    std::string objType = "unknown";
    if (node->object) objType = analyzeExpr(node->object.get());
    if (node->index) {
        std::string idxType = analyzeExpr(node->index.get());
        if (idxType != "unknown" && idxType != "i32" && idxType != "usize") {
            recordError("Array index must be an integer type, got " + idxType, node->line);
        }
    }
    // Strip reference
    std::string baseType = stripRef(objType);
    // Extract element type from array type like "[i32; 3]"
    if (baseType.size() > 2 && baseType.front() == '[') {
        int depth = 0;
        size_t semi = std::string::npos;
        for (size_t i = 1; i < baseType.size(); ++i) {
            if (baseType[i] == '[') ++depth;
            else if (baseType[i] == ']') --depth;
            else if (baseType[i] == ';' && depth == 0) { semi = i; break; }
        }
        if (semi != std::string::npos) {
            std::string elemType = baseType.substr(1, semi - 1);
            while (!elemType.empty() && elemType.back() == ' ') elemType.pop_back();
            return normalizeType(elemType);
        }
    }
    return "unknown";
}

std::string SemanticAnalyzer::analyzePathExpr(PathExprNode* node) {
    auto it = enumDefs_.find(node->base);
    if (it != enumDefs_.end()) {
        for (auto& v : it->second) {
            if (v == node->member) return node->base;
        }
        recordError("Enum '" + node->base + "' has no variant '" + node->member + "'",
                    node->line);
        return node->base;
    }
    return "unknown";
}

std::string SemanticAnalyzer::analyzeCastExpr(CastExprNode* node) {
    if (node->expr) analyzeExpr(node->expr.get());
    return normalizeType(node->targetType);
}
