# Contributions

## Reece Gerhart

### `semantic.c`

**Expression and Statement Semantic Analysis**

Implement semantic checking for expressions and statements.

For expression checking, handle the following AST node types:

* `NODE_NUM`: Always valid.
* `NODE_VAR`: Verify that the variable is declared and visible in the current scope using `isVarDeclaredInScope(node->data.name)`.
* `NODE_BINOP`: Recursively check both operands.

For statement checking, handle the following AST node types:

* `NODE_DECL`: Verify that the variable has not already been declared in the current scope. If valid, add it using `addVarToScope(name)`.
* `NODE_ASSIGN`: Verify that the assignment target is declared, then check the right hand expression using `checkExpr`.
* `NODE_PRINT`: Check the expression being printed.
* `NODE_STMT_LIST`: Recursively process both parts of the statement list using `checkStmtList`.

When an error occurs, report the line number using `node->lineno`, identify the undeclared variable, and explain how to fix the issue. Increment `semInfo.errorCount` for each error and continue checking so multiple errors can be reported in one run.

Process declarations before statements that use their variables. For example, in `int x; x = x + 1;`, the declaration must be processed before checking the assignment expression.

### `symtab.c`

**Variable Declaration and Name Resolution**

Implement variable declaration by adding the variable to the local symbol table and assigning it a stack frame offset.

Return the assigned byte offset, or `-1` if the variable has already been declared. Each integer occupies four bytes, and offsets increase from zero. The first variable is stored at `0($sp)`, the second at `4($sp)`, and subsequent variables continue in four byte increments.

Use the existing `findIn()` and `appendTo()` functions for searching and allocation.

Implement name resolution by returning the symbol associated with a variable name, or `NULL` if the variable is not declared. Search the local symbol table first, followed by the global symbol table. This ordering allows a local declaration to shadow a global declaration.

---

## Stili

### `ast.c`

**AST Constructors and Tree Printer**

Implement the following AST constructors: `createBinOp`, `createDecl`, `createAssign`, `createPrint`, and `createStmtList`.

Each constructor should allocate memory for an `ASTNode`, set its node type, record the line number using `yylineno`, store its children or associated data, and return the node.

Use `createNum` and `createVar` as examples. Duplicate stored strings using `strdup` because the scanner reuses its token buffer.

For `createStmtList`, preserve the statement order. The grammar is left recursive, so the first argument represents the existing statement list and the second argument represents the new statement.

Implement the AST tree printer so that each node appears on its own line, indented by two spaces for each level of nesting. Use a switch statement to handle each node type and recursively print child nodes at `level + 1`.

For `NODE_STMT_LIST`, print both children at the same indentation level because the list represents a sequence of statements rather than nested statements.

### `tac.c`

**Three Address Code Generation and Optimization**

Implement TAC generation for both statements and expressions.

For statement generation:

* `NODE_DECL`: Emit `TAC_DECL` so the backend knows the variable exists and can reserve a stack slot.
* `NODE_ASSIGN`: Evaluate the right hand expression, then emit `TAC_ASSIGN`.
* `NODE_PRINT`: Evaluate the expression, then emit `TAC_PRINT`.
* `NODE_STMT_LIST`: Recursively generate TAC for the statements.

Use `appendTAC(createTAC(op, arg1, arg2, result))` to emit instructions.

For expression generation:

* `NODE_NUM`: Return a string containing the numeric literal, such as `"42"`.
* `NODE_VAR`: Return a copy of the variable name.
* `NODE_BINOP`: Allocate a temporary using `allocTemp()`, recursively generate TAC for both operands, emit the binary operation, free operand temporaries after emitting the instruction, and return the result temporary.

Free operand temporaries only after emitting the instruction so live values are not accidentally reused.

For optimization, implement one pass that copies the input TAC into the output TAC while applying constant folding and constant propagation.

Constant folding evaluates operations involving literal operands during compilation. For example, `t0 = 2 + 3` becomes `t0 = 5`.

Constant propagation tracks known constant values and substitutes them into later instructions. For example, `x = 5; y = x + 1` becomes `y = 5 + 1`, allowing constant folding to produce `y = 6`.

Clear remembered constant values whenever a label is encountered because control flow may reach that label from another location. Increment `changesThisPass` for each rewrite and update the corresponding field in `optStats`.

### `codegen.c`

**TAC to MIPS Code Generation**

Implement MIPS instruction generation for each TAC opcode.

Use the existing helper functions:

* `operandReg(name)`: Returns the register containing a value, loading it from memory or loading a literal when necessary.
* `defReg(name)`: Returns a register for writing a new value.
* `flushRegisters()`: Writes dirty registers back to memory.

Implement the following operations:

* `TAC_DECL`: Emit a comment identifying the variable's stack location. No instruction is necessary because `layoutFrame()` reserves the slot.
* `TAC_ASSIGN`: Move the source value into the destination register.
* `TAC_ADD`: Generate an `add` instruction using the two operand registers and destination register.
* `TAC_PRINT`: Print the integer using SPIM syscall 1, followed by a newline using syscall 4 and the `__nl` string.
* `TAC_RETURN`: Place the return value in `$v0` and jump to the epilogue label rather than falling through.

Use `operandReg` when reading values and `defReg` when writing values. Using the wrong helper can introduce unnecessary memory loads.
