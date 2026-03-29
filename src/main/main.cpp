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

    // --- Parse ---
    Parser parser(source);
    auto program = parser.parseProgram();

    if (parser.hasErrors()) {
        for (const auto& err : parser.errors()) {
            std::cerr << "Parse error [line " << err.line << "]: " << err.message << std::endl;
        }
        return 1;
    }

    std::cout << "Parsed successfully: "
              << program->statements.size() << " top-level statement(s).\n\n";
    printAst(program.get());

    // --- Semantic analysis ---
    SemanticAnalyzer analyzer(program.get());
    if (!analyzer.analyze()) {
        std::cout << "\n";
        for (const auto& err : analyzer.errors()) {
            std::cerr << "Semantic error [line " << err.line << "]: "
                      << err.message << std::endl;
        }
        return 1;
    }

    std::cout << "\nSemantic analysis passed.\n";
    return 0;
}
