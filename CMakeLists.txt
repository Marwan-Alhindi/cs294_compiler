cmake_minimum_required(VERSION 3.14)
project(rustc_compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Main binary ---
add_executable(rustc
    src/main/main.cpp
    src/semantic/semantic.cpp
    src/parser/parser.cpp
    src/parser/ast.cpp
    src/parser/ast_printer.cpp    src/lexer/lexer.cpp
    src/lexer/token.cpp
)

# --- GoogleTest ---
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# --- Token tests ---
add_executable(token_test
    src/lexer/token_test.cc
    src/lexer/token.cpp
)
target_link_libraries(token_test GTest::gtest_main)
add_test(NAME TokenTests COMMAND token_test)

# --- Lexer tests ---
add_executable(lexer_test
    src/lexer/lexer_test.cc
    src/lexer/lexer.cpp
    src/lexer/token.cpp
)
target_link_libraries(lexer_test GTest::gtest_main)
add_test(NAME LexerTests COMMAND lexer_test)

# --- AST tests ---
add_executable(ast_test
    src/parser/ast_test.cc
    src/parser/ast.cpp
    src/parser/ast_printer.cpp)
target_link_libraries(ast_test GTest::gtest_main)
add_test(NAME AstTests COMMAND ast_test)

# --- Parser tests ---
add_executable(parser_test
    src/parser/parser_test.cc
    src/parser/parser.cpp
    src/parser/ast.cpp
    src/parser/ast_printer.cpp    src/lexer/lexer.cpp
    src/lexer/token.cpp
)
target_link_libraries(parser_test GTest::gtest_main)
add_test(NAME ParserTests COMMAND parser_test)

# --- Semantic tests ---
add_executable(semantic_test
    src/semantic/semantic_test.cc
    src/semantic/semantic.cpp
    src/parser/parser.cpp
    src/parser/ast.cpp
    src/parser/ast_printer.cpp
    src/lexer/lexer.cpp
    src/lexer/token.cpp
)
target_link_libraries(semantic_test GTest::gtest_main)
add_test(NAME SemanticTests COMMAND semantic_test)
