# Lexer

Builder for tokens. Scans source characters and produces a stream of `Token` values.
Skips whitespace and comments. Handles multi-char operators (`==`, `!=`, `<=`, `>=`).
