# Parser

Builder for the AST. Recursive descent over tokens with one-token lookahead.
Errors collected in a vector; panic-mode recovery via `synchronize()`.
