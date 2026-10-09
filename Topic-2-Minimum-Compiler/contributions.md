# Contributions

## Stili

### `ast.c`

**THE REMAINING AST CONSTRUCTORS**

Implement the following constructors: `createBinOp`, `createDecl`, `createAssign`, `createPrint`, and `createStmtList`.

Each constructor should allocate memory for an `ASTNode`, set its node type, record the line number using `yylineno`, store its children or associated data, and return the node.

Use `createNum` and `createVar` as examples. Any stored `char*` values must be duplicated using `strdup` because the scanner reuses its token buffer.

The `createStmtList` constructor requires special attention because the grammar is left recursive. The first argument represents the existing statement list, and the second argument represents the new statement. The resulting tree should preserve the order of statements.

### `codegen.c`

**TAC -> MIPS**

Implement MIPS instruction generation for each TAC opcode.

The existing helper functions are:

* `operandReg(name)`: Returns the register containing a value, loading it from memory or loading a literal when necessary.
* `defReg(name)`: Returns a register for writing a new value.
* `flushRegisters()`: Writes dirty registers back to memory.

Implement the following operations:

* `TAC_DECL`: Emit a comment identifying the variable's stack location. No instruction is necessary because `layoutFrame()` reserves the slot.
* `TAC_ASSIGN`: Move the value from the source register into the destination register.
* `TAC_ADD`: Generate an `add` instruction using the two operand registers and the destination register.
* `TAC_PRINT`: Print the integer stored in the operand register using SPIM syscall 1, followed by a newline using syscall 4 and the `__nl` string.
* `TAC_RETURN`: Place the return value in `$v0` and jump to the epilogue label rather than falling through.

Use `operandReg` when reading values and `defReg` when writing values. Using the wrong helper can introduce unnecessary memory loads.

### `semantic.c`

**CHECK A STATEMENT**

Implement statement checking for the following AST node types:

* `NODE_DECL`: Verify that the variable has not already been declared in the current scope. If valid, add it using `addVarToScope(name)`.
* `NODE_ASSIGN`: Verify that the assignment target is declared, then check the right hand expression using `checkExpr`.
* `NODE_PRINT`: Check the expression being printed.
* `NODE_STMT_LIST`: Recursively process both parts of the statement list using `checkStmtList`.

The order of checking matters. For example, in `int x; x = x + 1;`, the declaration must be processed before checking the assignment expression so that `x` is visible.

### `symtab.c`

**RESOLVE A NAME**

Implement name resolution by returning the symbol associated with a variable name, or `NULL` if the variable is not declared.

Search the local symbol table first, followed by the global symbol table. This ordering allows a local declaration to shadow a global declaration.

### `tac.c`

**STATEMENT -> THREE-ADDRESS CODE**

Implement TAC generation for statement nodes.

* `NODE_DECL`: Emit `TAC_DECL` so the backend knows the variable exists and can reserve a stack slot.
* `NODE_ASSIGN`: Evaluate the right hand expression, then emit `TAC_ASSIGN`.
* `NODE_PRINT`: Evaluate the expression, then emit `TAC_PRINT`.
* `NODE_STMT_LIST`: Recursively generate TAC for the statements.

Use `appendTAC(createTAC(op, arg1, arg2, result))` to emit instructions.

---

## Reece Gerhart

### `ast.c`

**AST Tree Printer**

Implement the AST tree printer so that each node appears on its own line, indented by two spaces for each level of nesting.

The function should return immediately if the node is `NULL`, print the appropriate indentation, and use a switch statement to handle each node type. Recursively print child nodes at `level + 1` to show the tree structure.

`NODE_STMT_LIST` is an exception. Its two children should be printed at the same indentation level because a statement list represents a sequence rather than nested statements.

### `semantic.c`

**Expression Semantic Analysis**

Implement expression checking for the following AST node types:

* `NODE_NUM`: Always valid.
* `NODE_VAR`: Verify that the variable is declared and visible in the current scope using `isVarDeclaredInScope(node->data.name)`.
* `NODE_BINOP`: Recursively check both operands.

When an error occurs, report the line number using `node->lineno`, identify the undeclared variable, and explain how to fix the issue. Increment `semInfo.errorCount` for each error and continue checking so multiple errors can be reported in one run.

### `symtab.c`

**Variable Declaration**

Implement variable declaration by adding the variable to the local symbol table and assigning it a stack frame offset.

Return the assigned byte offset, or `-1` if the variable has already been declared. The caller uses `-1` to report duplicate declarations.

Each integer occupies four bytes. Offsets increase from zero, so the first variable is stored at `0($sp)`, the second at `4($sp)`, and subsequent variables continue in four byte increments.

Use the existing `findIn()` and `appendTo()` functions for searching and allocation.

### `tac.c`

**Expression to Three Address Code Generation**

Implement recursive TAC generation for expressions. The function must return the name of the location containing the expression's value, whether that location is a literal, variable, or temporary.

Handle each AST node as follows:

* `NODE_NUM`: Return a string containing the numeric literal, such as `"42"`.
* `NODE_VAR`: Return a copy of the variable name.
* `NODE_BINOP`: Allocate a temporary using `allocTemp()`, recursively generate TAC for the left and right operands, emit the binary operation, free operand temporaries after the instruction has been emitted, and return the result temporary.

Free temporary operands only after emitting the instruction so that live values are not accidentally reused.

For an expression such as `a + b + c`, verify that the generated TAC contains the expected three address code instructions and that temporary numbers are not reused while their values are still needed.

### `tac.c`

**TAC Optimization Pass**

Implement one optimization pass that copies the input TAC into the output TAC while applying constant folding and constant propagation.

**Constant Folding**

Evaluate operations involving literal operands during compilation. For example:

`t0 = 2 + 3` becomes `t0 = 5`.

The existing `foldConstants()` function handles the arithmetic.

**Constant Propagation**

Track known constant values and substitute them into later instructions. For example:

`x = 5; y = x + 1` becomes `y = 5 + 1`, allowing constant folding to produce `y = 6`.

**Labels and Optimization Statistics**

Clear all remembered constant values when encountering a label because control flow may reach that label from another location. Although Topic 2 does not yet include labels, this behavior is needed for later topics.

Increment `changesThisPass` for every rewrite and update the corresponding field in `optStats` so `main.c` can report the optimization results.
