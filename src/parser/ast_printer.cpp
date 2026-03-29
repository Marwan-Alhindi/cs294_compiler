#include "ast_printer.h"

static void printIndent(std::ostream& out, int indent) {
    for (int i = 0; i < indent; ++i) out << "  ";
}

static void printNode(const AstNode* node, std::ostream& out, int indent);

static void printNode(const AstNode* node, std::ostream& out, int indent) {
    if (!node) return;
    printIndent(out, indent);

    switch (node->kind) {
        case NodeKind::PROGRAM: {
            auto* n = static_cast<const ProgramNode*>(node);
            out << "ProgramNode\n";
            for (auto& stmt : n->statements)
                printNode(stmt.get(), out, indent + 1);
            break;
        }
        case NodeKind::FN_DECL: {
            auto* n = static_cast<const FnDeclNode*>(node);
            out << "FnDeclNode(\"" << n->name << "\", params=[";
            for (size_t i = 0; i < n->params.size(); ++i) {
                if (i > 0) out << ", ";
                out << n->params[i].name << ": " << n->params[i].typeName;
            }
            out << "])\n";
            printNode(n->body.get(), out, indent + 1);
            break;
        }
        case NodeKind::BLOCK: {
            auto* n = static_cast<const BlockNode*>(node);
            out << "BlockNode\n";
            for (auto& stmt : n->statements)
                printNode(stmt.get(), out, indent + 1);
            break;
        }
        case NodeKind::LET_STMT: {
            auto* n = static_cast<const LetStmtNode*>(node);
            out << "LetStmtNode(mut=" << (n->isMut ? "true" : "false")
                << ", name=\"" << n->name << "\"";
            if (!n->typeName.empty()) out << ", type=\"" << n->typeName << "\"";
            out << ")\n";
            if (n->init) {
                printIndent(out, indent + 1);
                out << "init:\n";
                printNode(n->init.get(), out, indent + 2);
            }
            break;
        }
        case NodeKind::RETURN_STMT: {
            auto* n = static_cast<const ReturnStmtNode*>(node);
            out << "ReturnStmtNode\n";
            if (n->value) {
                printIndent(out, indent + 1);
                out << "value:\n";
                printNode(n->value.get(), out, indent + 2);
            }
            break;
        }
        case NodeKind::WHILE_STMT: {
            auto* n = static_cast<const WhileStmtNode*>(node);
            out << "WhileStmtNode\n";
            printIndent(out, indent + 1);
            out << "condition:\n";
            printNode(n->condition.get(), out, indent + 2);
            printIndent(out, indent + 1);
            out << "body:\n";
            printNode(n->body.get(), out, indent + 2);
            break;
        }
        case NodeKind::IF_STMT: {
            auto* n = static_cast<const IfStmtNode*>(node);
            out << "IfStmtNode\n";
            printIndent(out, indent + 1);
            out << "condition:\n";
            printNode(n->condition.get(), out, indent + 2);
            printIndent(out, indent + 1);
            out << "thenBranch:\n";
            printNode(n->thenBranch.get(), out, indent + 2);
            if (n->elseBranch) {
                printIndent(out, indent + 1);
                out << "elseBranch:\n";
                printNode(n->elseBranch.get(), out, indent + 2);
            }
            break;
        }
        case NodeKind::EXPR_STMT: {
            auto* n = static_cast<const ExprStmtNode*>(node);
            out << "ExprStmtNode\n";
            printNode(n->expr.get(), out, indent + 1);
            break;
        }
        case NodeKind::ASSIGN_EXPR: {
            auto* n = static_cast<const AssignExprNode*>(node);
            out << "AssignExpr(target=\"" << n->target << "\")\n";
            printIndent(out, indent + 1);
            out << "value:\n";
            printNode(n->value.get(), out, indent + 2);
            break;
        }
        case NodeKind::BINARY_EXPR: {
            auto* n = static_cast<const BinaryExprNode*>(node);
            out << "BinaryExpr(\"" << n->op << "\")\n";
            printIndent(out, indent + 1);
            out << "left:\n";
            printNode(n->left.get(), out, indent + 2);
            printIndent(out, indent + 1);
            out << "right:\n";
            printNode(n->right.get(), out, indent + 2);
            break;
        }
        case NodeKind::UNARY_EXPR: {
            auto* n = static_cast<const UnaryExprNode*>(node);
            out << "UnaryExpr(\"" << n->op << "\")\n";
            printNode(n->operand.get(), out, indent + 1);
            break;
        }
        case NodeKind::CALL_EXPR: {
            auto* n = static_cast<const CallExprNode*>(node);
            out << "CallExpr(\"" << n->callee << "\")\n";
            for (auto& arg : n->args)
                printNode(arg.get(), out, indent + 1);
            break;
        }
        case NodeKind::IDENT_EXPR: {
            auto* n = static_cast<const IdentExprNode*>(node);
            out << "IdentExpr(\"" << n->name << "\")\n";
            break;
        }
        case NodeKind::NUMBER_LITERAL: {
            auto* n = static_cast<const NumberLiteralNode*>(node);
            out << "NumberLiteral(" << n->value << ")\n";
            break;
        }
        case NodeKind::STRING_LITERAL: {
            auto* n = static_cast<const StringLiteralNode*>(node);
            out << "StringLiteral(\"" << n->value << "\")\n";
            break;
        }
    }
}

void printAst(const AstNode* node, std::ostream& out, int indent) {
    printNode(node, out, indent);
}
