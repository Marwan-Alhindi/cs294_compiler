# Semantic Analyzer

Walks the AST and checks for: undefined variables, redeclarations, type mismatches,
mutability violations, and function call arity/type errors. Uses a scoped symbol table
and pre-registers top-level functions for forward calls.
