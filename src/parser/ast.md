# AST

Data structure for the parser. Defines 15 node types (8 statements, 7 expressions)
owned via `unique_ptr`. Uses `NodeKind` enum + `static_cast` for downcasting.
