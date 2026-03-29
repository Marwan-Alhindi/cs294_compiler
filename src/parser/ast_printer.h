#ifndef AST_PRINTER_H
#define AST_PRINTER_H

#include "ast.h"
#include <iostream>
#include <string>

void printAst(const AstNode* node, std::ostream& out = std::cout, int indent = 0);

#endif // AST_PRINTER_H
