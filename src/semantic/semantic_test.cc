#include "../semantic/semantic.h"
#include "../parser/parser.h"
#include <gtest/gtest.h>

// ============================================================
// Test helper — parse source, run semantic analysis, return result
// ============================================================

struct TestResult {
    std::unique_ptr<ProgramNode> program;
    std::vector<SemanticError> errors;
    bool passed;
};

static TestResult analyzeSource(const std::string& source) {
    Parser parser(source);
    auto program = parser.parseProgram();
    for (const auto& err : parser.errors()) {
        ADD_FAILURE() << "Unexpected parse error: " << err.message;
    }
    SemanticAnalyzer analyzer(program.get());
    bool passed = analyzer.analyze();
    auto errors = analyzer.errors();   // copy before analyzer destructs
    return TestResult{std::move(program), std::move(errors), passed};
}

// ============================================================
// Name Resolution
// ============================================================

TEST(Semantic, UndefinedVariable) {
    auto r = analyzeSource("fn main() { let x = y; }");
    ASSERT_FALSE(r.passed);
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_NE(r.errors[0].message.find("Undefined variable 'y'"), std::string::npos);
}

TEST(Semantic, DefinedVariable) {
    auto r = analyzeSource("fn main() { let x = 42; let y = x; }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, RedeclarationSameScope) {
    // Rust allows let shadowing in the same scope
    auto r = analyzeSource("fn main() { let x = 1; let x = 2; }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, ShadowingInNestedScope) {
    auto r = analyzeSource("fn main() { let x = 1; { let x = 2; } }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, VariableOutOfScope) {
    auto r = analyzeSource("fn main() { { let x = 1; } let y = x; }");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("Undefined variable 'x'"), std::string::npos);
}

TEST(Semantic, FunctionParamsInScope) {
    auto r = analyzeSource("fn add(a: i32, b: i32) { let c = a + b; }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, DuplicateParamName) {
    auto r = analyzeSource("fn bad(a: i32, a: i32) { let x = a; }");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("Redeclaration"), std::string::npos);
}

// ============================================================
// Mutability
// ============================================================

TEST(Semantic, AssignToImmutable) {
    auto r = analyzeSource("fn main() { let x = 1; x = 2; }");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("immutable"), std::string::npos);
}

TEST(Semantic, AssignToMutable) {
    auto r = analyzeSource("fn main() { let mut x = 1; x = 2; }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, AssignToUndefined) {
    auto r = analyzeSource("fn main() { z = 5; }");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("Undefined variable 'z'"), std::string::npos);
}

// ============================================================
// Type Checking — let declarations
// ============================================================

TEST(Semantic, TypeAnnotationMatch) {
    auto r = analyzeSource("fn main() { let x: i32 = 42; }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, TypeAnnotationMismatch) {
    auto r = analyzeSource("fn main() { let x: string = 42; }");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("Type mismatch"), std::string::npos);
}

// ============================================================
// Type Checking — arithmetic
// ============================================================

TEST(Semantic, ArithmeticOnIntegers) {
    auto r = analyzeSource("fn main() { let x = 1 + 2 * 3 - 4 / 2; }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, ArithmeticOnStrings) {
    auto r = analyzeSource(R"(fn main() { let x = "a" + "b"; })");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("i32"), std::string::npos);
}

TEST(Semantic, ArithmeticOnBool) {
    // (1 == 2) returns bool, which cannot be added to i32
    auto r = analyzeSource("fn main() { let x = (1 == 2) + 3; }");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("bool"), std::string::npos);
}

// ============================================================
// Type Checking — unary
// ============================================================

TEST(Semantic, UnaryMinusOnInteger) {
    auto r = analyzeSource("fn main() { let x = -42; }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, UnaryMinusOnString) {
    auto r = analyzeSource(R"(fn main() { let x = -"hello"; })");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("i32"), std::string::npos);
}

// ============================================================
// Type Checking — comparisons
// ============================================================

TEST(Semantic, ComparisonSameType) {
    auto r = analyzeSource("fn main() { let x = 1 == 2; }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, ComparisonMixedTypes) {
    auto r = analyzeSource(R"(fn main() { let x = 1 == "hello"; })");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("Cannot compare"), std::string::npos);
}

// ============================================================
// Type Checking — assignment type mismatch
// ============================================================

TEST(Semantic, AssignTypeMismatch) {
    auto r = analyzeSource(R"(fn main() { let mut x = 1; x = "hello"; })");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("Type mismatch"), std::string::npos);
}

// ============================================================
// Function calls
// ============================================================

TEST(Semantic, CallDefinedFunction) {
    auto r = analyzeSource("fn foo() {} fn main() { foo(); }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, CallUndefinedFunction) {
    auto r = analyzeSource("fn main() { bar(); }");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("Undefined function 'bar'"), std::string::npos);
}

TEST(Semantic, CallCorrectArgCount) {
    auto r = analyzeSource("fn add(a: i32, b: i32) {} fn main() { add(1, 2); }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, CallWrongArgCount) {
    auto r = analyzeSource("fn add(a: i32, b: i32) {} fn main() { add(1); }");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("expects 2"), std::string::npos);
}

TEST(Semantic, CallArgTypeMismatch) {
    auto r = analyzeSource(R"(fn greet(name: string) {} fn main() { greet(42); })");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("expects string"), std::string::npos);
}

TEST(Semantic, ForwardFunctionCall) {
    // main calls foo which is declared after main — should work via pre-registration
    auto r = analyzeSource("fn main() { foo(); } fn foo() {}");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, RecursiveCall) {
    auto r = analyzeSource("fn countdown(n: i32) { countdown(n - 1); }");
    EXPECT_TRUE(r.passed);
}

// ============================================================
// Duplicate function definitions
// ============================================================

TEST(Semantic, DuplicateFunction) {
    auto r = analyzeSource("fn foo() {} fn foo() {}");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("Redefinition"), std::string::npos);
}

// ============================================================
// Scoping: each function has its own scope
// ============================================================

TEST(Semantic, SeparateFunctionScopes) {
    auto r = analyzeSource("fn foo() { let x = 1; } fn bar() { let x = 2; }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, DeeplyNestedScopes) {
    auto r = analyzeSource("fn main() { let a = 1; { let b = a; { let c = b + a; } } }");
    EXPECT_TRUE(r.passed);
}

// ============================================================
// Conditions: while and if
// ============================================================

TEST(Semantic, WhileUndefinedCondition) {
    auto r = analyzeSource("fn main() { while (undef > 0) {} }");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("Undefined variable"), std::string::npos);
}

TEST(Semantic, IfUndefinedCondition) {
    auto r = analyzeSource("fn main() { if (undef == 0) {} }");
    ASSERT_FALSE(r.passed);
}

TEST(Semantic, IfElseAnalyzed) {
    auto r = analyzeSource("fn main() { let x = 1; if (x == 0) { let y = x; } else { let z = x; } }");
    EXPECT_TRUE(r.passed);
}

// ============================================================
// Edge cases
// ============================================================

TEST(Semantic, EmptyProgram) {
    auto r = analyzeSource("");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, EmptyFunction) {
    auto r = analyzeSource("fn main() {}");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, BareReturn) {
    auto r = analyzeSource("fn main() { return; }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, ReturnExpression) {
    auto r = analyzeSource("fn main() { let x = 1; return x + 1; }");
    EXPECT_TRUE(r.passed);
}

TEST(Semantic, MultipleErrors) {
    auto r = analyzeSource("fn main() { let x = undef_a; let y = undef_b; }");
    ASSERT_FALSE(r.passed);
    EXPECT_GE(r.errors.size(), 2u);
}

TEST(Semantic, UndefinedIdentExprStmt) {
    // Bare identifier used as expression statement — should report undefined
    auto r = analyzeSource("fn main() { fn_name; }");
    ASSERT_FALSE(r.passed);
    EXPECT_NE(r.errors[0].message.find("Undefined variable 'fn_name'"), std::string::npos);
}

// ============================================================
// Integration: realistic program (from tests/sample.rs, minus fn_name)
// ============================================================

TEST(Semantic, SampleProgram) {
    auto r = analyzeSource(R"(
        fn main() {
            let x = 42;
            let mut y = 0;
            if (x == y) {
                return;
            } else {
                y = x + 1;
            }
            while (y > 0) {
                y = y - 1;
            }
        }
    )");
    EXPECT_TRUE(r.passed);
    EXPECT_TRUE(r.errors.empty());
}

TEST(Semantic, ComplexProgram) {
    auto r = analyzeSource(R"(
        fn add(a: i32, b: i32) {
            return a + b;
        }
        fn main() {
            let x = 10;
            let mut result = 0;
            result = add(x, 20);
            if (result > 0) {
                let msg = "positive";
            }
        }
    )");
    EXPECT_TRUE(r.passed);
}
