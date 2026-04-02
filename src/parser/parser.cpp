#include "parser.h"
#include <cctype>

// ============================================================
// Constructor
// ============================================================
Parser::Parser(const std::string& source)
    : lexer_(source),
      current_(Token{TokenType::EOF_TOKEN, "", 0}),
      peek_(Token{TokenType::EOF_TOKEN, "", 0}) {
    advance();
    advance();
}

// ============================================================
// Token navigation
// ============================================================

void Parser::advance() {
    current_ = peek_;
    peek_ = lexer_.nextToken();
}

bool Parser::check(TokenType type) const {
    return current_.type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::expect(TokenType type, const std::string& errorMsg) {
    if (check(type)) {
        Token tok = current_;
        advance();
        return tok;
    }
    recordError(errorMsg, current_.line);
    return current_;
}

// ============================================================
// Error handling
// ============================================================

void Parser::recordError(const std::string& msg, int line) {
    errors_.push_back(ParseError{msg, line});
}

void Parser::synchronize() {
    while (!check(TokenType::EOF_TOKEN)) {
        if (check(TokenType::SEMICOLON)) {
            advance();
            return;
        }
        if (check(TokenType::RBRACE)) {
            return;
        }
        if (check(TokenType::FN) || check(TokenType::LET) ||
            check(TokenType::CONST) || check(TokenType::RETURN) ||
            check(TokenType::WHILE) || check(TokenType::IF) ||
            check(TokenType::STRUCT) || check(TokenType::ENUM) ||
            check(TokenType::IMPL) || check(TokenType::LOOP) ||
            check(TokenType::BREAK) || check(TokenType::CONTINUE)) {
            return;
        }
        advance();
    }
}

// ============================================================
// Type parsing
// ============================================================

std::string Parser::parseType() {
    // () → unit type
    if (check(TokenType::LPAREN)) {
        advance();
        expect(TokenType::RPAREN, "Expected ')' in unit type");
        return "()";
    }
    // &Type or &mut Type or &[Type; N]
    if (check(TokenType::AMP)) {
        advance();
        bool isMut = match(TokenType::MUT);
        std::string inner = parseType();
        if (isMut) return "&mut " + inner;
        return "&" + inner;
    }
    // [Type; N] → array type
    if (check(TokenType::LBRACKET)) {
        advance();
        std::string elemType = parseType();
        expect(TokenType::SEMICOLON, "Expected ';' in array type");
        // Allow number or identifier as array size
        std::string sizeStr;
        if (check(TokenType::NUMBER)) {
            sizeStr = current_.lexeme;
            advance();
        } else if (check(TokenType::IDENT)) {
            sizeStr = current_.lexeme;
            advance();
        } else {
            recordError("Expected array size", current_.line);
        }
        expect(TokenType::RBRACKET, "Expected ']' in array type");
        return "[" + elemType + "; " + sizeStr + "]";
    }
    // IDENT → type name (i32, u32, usize, bool, String, Self, user types)
    Token tok = expect(TokenType::IDENT, "Expected type name");
    return tok.lexeme;
}

// ============================================================
// Public API
// ============================================================

bool Parser::hasErrors() const {
    return !errors_.empty();
}

const std::vector<ParseError>& Parser::errors() const {
    return errors_;
}

// ============================================================
// Top-level
// ============================================================

std::unique_ptr<ProgramNode> Parser::parseProgram() {
    auto program = std::make_unique<ProgramNode>();
    while (!check(TokenType::EOF_TOKEN)) {
        auto stmt = parseStatement();
        if (stmt) {
            program->statements.push_back(std::move(stmt));
        }
    }
    return program;
}

// ============================================================
// Statements
// ============================================================

AstNodePtr Parser::parseStatement() {
    if (check(TokenType::FN))       return parseFnDecl();
    if (check(TokenType::LET) || check(TokenType::CONST)) return parseLetStmt();
    if (check(TokenType::RETURN))   return parseReturnStmt();
    if (check(TokenType::WHILE))    return parseWhileStmt();
    if (check(TokenType::IF))       return parseIfStmt();
    if (check(TokenType::LBRACE))   return parseBlock();
    if (check(TokenType::LOOP))     return parseLoopStmt();
    if (check(TokenType::BREAK))    return parseBreakStmt();
    if (check(TokenType::CONTINUE)) return parseContinueStmt();
    if (check(TokenType::STRUCT))   return parseStructDef();
    if (check(TokenType::ENUM))     return parseEnumDef();
    if (check(TokenType::IMPL))     return parseImplBlock();
    return parseExprStmt();
}

AstNodePtr Parser::parseFnDecl() {
    int line = current_.line;
    advance();  // consume FN

    Token nameTok = expect(TokenType::IDENT, "Expected function name after 'fn'");
    auto node = std::make_unique<FnDeclNode>(nameTok.lexeme, line);

    expect(TokenType::LPAREN, "Expected '(' after function name");

    // Parse parameter list
    if (!check(TokenType::RPAREN)) {
        do {
            // Handle &self, &mut self, self
            if (check(TokenType::AMP)) {
                advance();  // consume &
                bool isMutSelf = match(TokenType::MUT);
                if (check(TokenType::SELF_LOWER)) {
                    advance();
                    ParamNode p;
                    p.name = "self";
                    p.typeName = "Self";
                    p.isSelf = true;
                    p.isMutSelf = isMutSelf;
                    p.line = line;
                    node->params.push_back(p);
                    continue;
                }
                // Not self — this is a reference parameter, put back
                // Actually we can't put back. Handle as &Type param.
                Token paramName = expect(TokenType::IDENT, "Expected parameter name");
                expect(TokenType::COLON, "Expected ':' after parameter name");
                std::string paramType = parseType();
                node->params.push_back(ParamNode{paramName.lexeme, "&" + paramType, false, false, paramName.line});
                continue;
            }
            if (check(TokenType::SELF_LOWER)) {
                advance();
                ParamNode p;
                p.name = "self";
                p.typeName = "Self";
                p.isSelf = true;
                p.line = line;
                node->params.push_back(p);
                continue;
            }
            Token paramName = expect(TokenType::IDENT, "Expected parameter name");
            expect(TokenType::COLON, "Expected ':' after parameter name");
            std::string paramType = parseType();
            node->params.push_back(ParamNode{paramName.lexeme, paramType, false, false, paramName.line});
        } while (match(TokenType::COMMA));
    }

    expect(TokenType::RPAREN, "Expected ')' after parameters");

    // Optional return type: -> Type
    if (match(TokenType::ARROW)) {
        node->returnType = parseType();
    }

    auto body = parseBlock();
    if (body) {
        node->body = std::move(body);
    }
    return node;
}

AstNodePtr Parser::parseBlock() {
    int line = current_.line;
    expect(TokenType::LBRACE, "Expected '{'");

    auto block = std::make_unique<BlockNode>(line);
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        auto stmt = parseStatement();
        if (stmt) {
            block->statements.push_back(std::move(stmt));
        }
    }

    expect(TokenType::RBRACE, "Expected '}'");
    return block;
}

AstNodePtr Parser::parseLetStmt() {
    int line = current_.line;
    bool isConst = check(TokenType::CONST);
    advance();  // consume LET or CONST

    bool isMut = !isConst && match(TokenType::MUT);
    Token nameTok = expect(TokenType::IDENT, "Expected variable name after 'let'");

    auto node = std::make_unique<LetStmtNode>(isMut, nameTok.lexeme, line);

    // Optional type annotation
    if (match(TokenType::COLON)) {
        node->typeName = parseType();
    }

    expect(TokenType::ASSIGN, "Expected '=' in let statement");
    node->init = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';' after let statement");
    return node;
}

AstNodePtr Parser::parseReturnStmt() {
    int line = current_.line;
    advance();  // consume RETURN

    auto node = std::make_unique<ReturnStmtNode>(line);

    if (!check(TokenType::SEMICOLON) && !check(TokenType::EOF_TOKEN) &&
        !check(TokenType::RBRACE)) {
        node->value = parseExpression();
    }

    // Allow return without semicolon at end of block
    if (!check(TokenType::RBRACE)) {
        expect(TokenType::SEMICOLON, "Expected ';' after return statement");
    }
    return node;
}

AstNodePtr Parser::parseWhileStmt() {
    int line = current_.line;
    advance();  // consume WHILE

    auto node = std::make_unique<WhileStmtNode>(line);
    node->condition = parseExpression();
    node->body = parseBlock();
    return node;
}

AstNodePtr Parser::parseIfStmt() {
    int line = current_.line;
    advance();  // consume IF

    auto node = std::make_unique<IfStmtNode>(line);
    node->condition = parseExpression();
    node->thenBranch = parseBlock();

    if (match(TokenType::ELSE)) {
        if (check(TokenType::IF)) {
            node->elseBranch = parseIfStmt();
        } else {
            node->elseBranch = parseBlock();
        }
    }

    return node;
}

AstNodePtr Parser::parseExprStmt() {
    int line = current_.line;
    auto node = std::make_unique<ExprStmtNode>(line);
    node->expr = parseExpression();
    // Allow implicit return: no semicolon needed if next token is '}'
    if (!check(TokenType::RBRACE)) {
        expect(TokenType::SEMICOLON, "Expected ';' after expression statement");
    }
    return node;
}

AstNodePtr Parser::parseLoopStmt() {
    int line = current_.line;
    advance();  // consume LOOP

    auto node = std::make_unique<LoopStmtNode>(line);
    node->body = parseBlock();
    return node;
}

AstNodePtr Parser::parseBreakStmt() {
    int line = current_.line;
    advance();  // consume BREAK
    auto node = std::make_unique<BreakStmtNode>(line);
    expect(TokenType::SEMICOLON, "Expected ';' after 'break'");
    return node;
}

AstNodePtr Parser::parseContinueStmt() {
    int line = current_.line;
    advance();  // consume CONTINUE
    auto node = std::make_unique<ContinueStmtNode>(line);
    expect(TokenType::SEMICOLON, "Expected ';' after 'continue'");
    return node;
}

AstNodePtr Parser::parseStructDef() {
    int line = current_.line;
    advance();  // consume STRUCT

    Token nameTok = expect(TokenType::IDENT, "Expected struct name");
    auto node = std::make_unique<StructDefNode>(nameTok.lexeme, line);

    expect(TokenType::LBRACE, "Expected '{' after struct name");
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        Token fieldName = expect(TokenType::IDENT, "Expected field name");
        expect(TokenType::COLON, "Expected ':' after field name");
        std::string fieldType = parseType();
        node->fields.push_back(StructField{fieldName.lexeme, fieldType, fieldName.line});
        if (!check(TokenType::RBRACE)) {
            expect(TokenType::COMMA, "Expected ',' between struct fields");
        }
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return node;
}

AstNodePtr Parser::parseEnumDef() {
    int line = current_.line;
    advance();  // consume ENUM

    Token nameTok = expect(TokenType::IDENT, "Expected enum name");
    auto node = std::make_unique<EnumDefNode>(nameTok.lexeme, line);

    expect(TokenType::LBRACE, "Expected '{' after enum name");
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        Token variant = expect(TokenType::IDENT, "Expected variant name");
        node->variants.push_back(variant.lexeme);
        if (!check(TokenType::RBRACE)) {
            expect(TokenType::COMMA, "Expected ',' between enum variants");
        }
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return node;
}

AstNodePtr Parser::parseImplBlock() {
    int line = current_.line;
    advance();  // consume IMPL

    Token nameTok = expect(TokenType::IDENT, "Expected type name after 'impl'");
    auto node = std::make_unique<ImplBlockNode>(nameTok.lexeme, line);

    expect(TokenType::LBRACE, "Expected '{' after impl type name");
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        if (check(TokenType::FN)) {
            node->methods.push_back(parseFnDecl());
        } else {
            recordError("Expected function declaration in impl block", current_.line);
            synchronize();
        }
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return node;
}

// ============================================================
// Expressions
// Precedence (low to high):
//   assignment < || < && < comparison < additive
//   < multiplicative < cast < unary < postfix < primary
// ============================================================

AstNodePtr Parser::parseExpression() {
    return parseAssignment();
}

AstNodePtr Parser::parseAssignment() {
    auto left = parseLogicalOr();

    if (check(TokenType::ASSIGN)) {
        int line = current_.line;
        advance();  // consume =
        auto node = std::make_unique<AssignExprNode>(line);
        node->target = std::move(left);
        node->value = parseAssignment();
        return node;
    }

    if (check(TokenType::PLUS_ASSIGN) || check(TokenType::MINUS_ASSIGN) ||
        check(TokenType::STAR_ASSIGN) || check(TokenType::SLASH_ASSIGN) ||
        check(TokenType::PERCENT_ASSIGN)) {
        int line = current_.line;
        std::string compoundOp;
        if (current_.type == TokenType::PLUS_ASSIGN) compoundOp = "+";
        else if (current_.type == TokenType::MINUS_ASSIGN) compoundOp = "-";
        else if (current_.type == TokenType::STAR_ASSIGN) compoundOp = "*";
        else if (current_.type == TokenType::SLASH_ASSIGN) compoundOp = "/";
        else compoundOp = "%";
        advance();  // consume compound assign
        auto node = std::make_unique<AssignExprNode>(line);
        node->op = compoundOp;
        node->target = std::move(left);
        node->value = parseAssignment();
        return node;
    }

    return left;
}

AstNodePtr Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();

    while (check(TokenType::OR_OR)) {
        std::string op = current_.lexeme;
        int line = current_.line;
        advance();
        auto node = std::make_unique<BinaryExprNode>(op, line);
        node->left = std::move(left);
        node->right = parseLogicalAnd();
        left = std::move(node);
    }

    return left;
}

AstNodePtr Parser::parseLogicalAnd() {
    auto left = parseComparison();

    while (check(TokenType::AND_AND)) {
        std::string op = current_.lexeme;
        int line = current_.line;
        advance();
        auto node = std::make_unique<BinaryExprNode>(op, line);
        node->left = std::move(left);
        node->right = parseComparison();
        left = std::move(node);
    }

    return left;
}

AstNodePtr Parser::parseComparison() {
    auto left = parseAdditive();

    while (check(TokenType::EQ) || check(TokenType::NEQ) ||
           check(TokenType::LT) || check(TokenType::GT) ||
           check(TokenType::LTE) || check(TokenType::GTE)) {
        std::string op = current_.lexeme;
        int line = current_.line;
        advance();
        auto node = std::make_unique<BinaryExprNode>(op, line);
        node->left = std::move(left);
        node->right = parseAdditive();
        left = std::move(node);
    }

    return left;
}

AstNodePtr Parser::parseAdditive() {
    auto left = parseMultiplicative();

    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        std::string op = current_.lexeme;
        int line = current_.line;
        advance();
        auto node = std::make_unique<BinaryExprNode>(op, line);
        node->left = std::move(left);
        node->right = parseMultiplicative();
        left = std::move(node);
    }

    return left;
}

AstNodePtr Parser::parseMultiplicative() {
    auto left = parseCast();

    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
        std::string op = current_.lexeme;
        int line = current_.line;
        advance();
        auto node = std::make_unique<BinaryExprNode>(op, line);
        node->left = std::move(left);
        node->right = parseCast();
        left = std::move(node);
    }

    return left;
}

AstNodePtr Parser::parseCast() {
    auto left = parseUnary();

    while (check(TokenType::AS)) {
        int line = current_.line;
        advance();  // consume 'as'
        std::string targetType = parseType();
        auto node = std::make_unique<CastExprNode>(targetType, line);
        node->expr = std::move(left);
        left = std::move(node);
    }

    return left;
}

AstNodePtr Parser::parseUnary() {
    if (check(TokenType::MINUS) || check(TokenType::NOT)) {
        std::string op = current_.lexeme;
        int line = current_.line;
        advance();
        auto node = std::make_unique<UnaryExprNode>(op, line);
        node->operand = parseUnary();
        return node;
    }
    // Reference: &expr or &mut expr
    if (check(TokenType::AMP)) {
        int line = current_.line;
        advance();  // consume &
        std::string op = "&";
        if (match(TokenType::MUT)) {
            op = "&mut";
        }
        auto node = std::make_unique<UnaryExprNode>(op, line);
        node->operand = parseUnary();
        return node;
    }
    // Dereference: *expr
    if (check(TokenType::STAR)) {
        int line = current_.line;
        advance();
        auto node = std::make_unique<UnaryExprNode>("*", line);
        node->operand = parseUnary();
        return node;
    }
    auto primary = parsePrimary();
    return parsePostfix(std::move(primary));
}

AstNodePtr Parser::parsePostfix(AstNodePtr left) {
    while (true) {
        if (check(TokenType::DOT)) {
            int line = current_.line;
            advance();  // consume .
            Token memberTok = expect(TokenType::IDENT, "Expected member name after '.'");

            if (check(TokenType::LPAREN)) {
                // Method call: obj.method(args)
                advance();  // consume (
                auto node = std::make_unique<MethodCallExprNode>(memberTok.lexeme, line);
                node->object = std::move(left);
                if (!check(TokenType::RPAREN)) {
                    do {
                        if (check(TokenType::RPAREN)) break;
                        node->args.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                }
                expect(TokenType::RPAREN, "Expected ')' after method arguments");
                left = std::move(node);
            } else {
                // Field access: obj.field
                auto node = std::make_unique<FieldAccessExprNode>(memberTok.lexeme, line);
                node->object = std::move(left);
                left = std::move(node);
            }
        } else if (check(TokenType::LBRACKET)) {
            // Index: obj[index]
            int line = current_.line;
            advance();  // consume [
            auto node = std::make_unique<IndexExprNode>(line);
            node->object = std::move(left);
            node->index = parseExpression();
            expect(TokenType::RBRACKET, "Expected ']' after index");
            left = std::move(node);
        } else {
            break;
        }
    }
    return left;
}

AstNodePtr Parser::parsePrimary() {
    // Number literal
    if (check(TokenType::NUMBER)) {
        auto node = std::make_unique<NumberLiteralNode>(current_.lexeme, current_.line);
        advance();
        return node;
    }

    // String literal
    if (check(TokenType::STRING)) {
        auto node = std::make_unique<StringLiteralNode>(current_.lexeme, current_.line);
        advance();
        return node;
    }

    // Bool literals
    if (check(TokenType::TRUE)) {
        auto node = std::make_unique<BoolLiteralNode>(true, current_.line);
        advance();
        return node;
    }
    if (check(TokenType::FALSE)) {
        auto node = std::make_unique<BoolLiteralNode>(false, current_.line);
        advance();
        return node;
    }

    // Array literal: [...]
    if (check(TokenType::LBRACKET)) {
        return parseArrayLiteral();
    }

    // Identifier, function call, path expression, or struct literal
    if (check(TokenType::IDENT)) {
        std::string name = current_.lexeme;
        int line = current_.line;

        // Path expression: Name::member or Name::method(args)
        if (peek_.type == TokenType::COLON_COLON) {
            advance();  // consume IDENT
            advance();  // consume ::
            Token member = expect(TokenType::IDENT, "Expected identifier after '::'");

            // Check for static method call: Name::method(args)
            if (check(TokenType::LPAREN)) {
                advance();  // consume (
                std::string callee = name + "::" + member.lexeme;
                auto callNode = std::make_unique<CallExprNode>(callee, line);
                if (!check(TokenType::RPAREN)) {
                    do {
                        if (check(TokenType::RPAREN)) break;
                        callNode->args.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                }
                expect(TokenType::RPAREN, "Expected ')' after call arguments");
                return callNode;
            }

            return std::make_unique<PathExprNode>(name, member.lexeme, line);
        }

        // Function call: ident(...)
        if (peek_.type == TokenType::LPAREN) {
            advance();  // consume IDENT
            advance();  // consume (
            auto node = std::make_unique<CallExprNode>(name, line);
            if (!check(TokenType::RPAREN)) {
                do {
                    if (check(TokenType::RPAREN)) break;  // trailing comma
                    node->args.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RPAREN, "Expected ')' after call arguments");
            return node;
        }

        // Struct literal: Name { field: value, ... }
        // Heuristic: name starts with uppercase letter
        if (peek_.type == TokenType::LBRACE && !name.empty() && std::isupper(name[0])) {
            advance();  // consume IDENT
            advance();  // consume {
            auto node = std::make_unique<StructLiteralNode>(name, line);
            if (!check(TokenType::RBRACE)) {
                do {
                    Token fieldName = expect(TokenType::IDENT, "Expected field name");
                    expect(TokenType::COLON, "Expected ':' after field name");
                    auto value = parseExpression();
                    node->fields.push_back(StructLiteralField{fieldName.lexeme, std::move(value)});
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RBRACE, "Expected '}' after struct literal");
            return node;
        }

        // Bare identifier
        advance();
        return std::make_unique<IdentExprNode>(name, line);
    }

    // self keyword as expression
    if (check(TokenType::SELF_LOWER)) {
        auto node = std::make_unique<IdentExprNode>("self", current_.line);
        advance();
        return node;
    }

    // If expression: if cond { } else { }
    if (check(TokenType::IF)) {
        return parseIfStmt();
    }

    // Loop expression: loop { }
    if (check(TokenType::LOOP)) {
        return parseLoopStmt();
    }

    // Grouped expression: (expr)
    if (check(TokenType::LPAREN)) {
        advance();
        auto expr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')' after grouped expression");
        return expr;
    }

    // Unexpected token
    recordError("Unexpected token '" + current_.lexeme + "' in expression", current_.line);
    synchronize();
    return nullptr;
}

AstNodePtr Parser::parseArrayLiteral() {
    int line = current_.line;
    advance();  // consume [

    auto node = std::make_unique<ArrayLiteralNode>(line);

    if (check(TokenType::RBRACKET)) {
        advance();
        return node;
    }

    auto first = parseExpression();

    // [value; count] form
    if (check(TokenType::SEMICOLON)) {
        advance();
        node->isRepeat = true;
        node->repeatValue = std::move(first);
        node->repeatCount = parseExpression();
        expect(TokenType::RBRACKET, "Expected ']' after array literal");
        return node;
    }

    // [a, b, c, ...] form
    node->elements.push_back(std::move(first));
    while (match(TokenType::COMMA)) {
        if (check(TokenType::RBRACKET)) break;
        node->elements.push_back(parseExpression());
    }
    expect(TokenType::RBRACKET, "Expected ']' after array literal");
    return node;
}
