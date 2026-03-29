# Architecture

A compiler for basic Rust syntax. Reads a `.rs` source file and processes it through
a pipeline of phases, each transforming the program into a richer representation.

## Pipeline

```
source file (.rs)
       |
       v
   [ Lexer ]  ──produces──>  Tokens
       |
       v
   [ Parser ]  ──produces──>  AST
       |
       v
   [ Semantic Analyzer ]  ──validates──>  AST (checked)
       |
       v
   [ Code Generator ]  ──emits──>  target code (planned)
```

## Data structure vs. builder

Each phase follows the same pattern: a **builder** (the logic) produces or consumes
a **data structure** (the output). They live together in the same folder because
the data structure only exists to serve that phase.

- **Data structure**: named after what it represents (e.g. `token.h`, `ast.h`)
- **Builder**: named after the phase that produces it (e.g. `lexer.h`, `parser.h`, `semantic.h`)
