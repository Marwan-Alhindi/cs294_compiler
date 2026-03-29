#include "../lexer/lexer.h"
#include "../lexer/token.h"
#include "../parser/parser.h"
#include "../semantic/semantic.h"
#include "../parser/ast_printer.h"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: rustc <source_file>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file '" << argv[1] << "'" << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // === Phase 1: Lexer ===
    std::cout << "===== LEXER OUTPUT =====\n\n";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::EOF_TOKEN) break;
        std::cout << tokenTypeToString(tok.type) << "  " << tok.lexeme
                  << "  [line " << tok.line << "]\n";
    }

    // === Phase 2: Parser ===
    std::cout << "\n===== PARSER OUTPUT =====\n\n";
    Parser parser(source);
    auto program = parser.parseProgram();

    if (parser.hasErrors()) {
        for (const auto& err : parser.errors()) {
            std::cerr << "Parse error [line " << err.line << "]: " << err.message << std::endl;
        }
        return 1;
    }

    printAst(program.get());

    // === Phase 3: Semantic Analysis ===
    std::cout << "\n===== SEMANTIC OUTPUT =====\n\n";
    SemanticAnalyzer analyzer(program.get());
    if (!analyzer.analyze()) {
        for (const auto& err : analyzer.errors()) {
            std::cout << "Error [line " << err.line << "]: " << err.message << "\n";
        }
        return 1;
    }

    std::cout << "No errors.\n";
    return 0;
}
