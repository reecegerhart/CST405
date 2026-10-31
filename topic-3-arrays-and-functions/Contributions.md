# Contributions

Topic 3: Compiling Complex Variables and Functions

The work is split by pipeline stage. Reece owns the front end (scanner, parser, AST, semantic analysis). Stili owns the back end (three address code and MIPS generation). The AST is the handoff between the two halves.

---

## Reece Gerhart

### `scanner.l`

**Tokens for Functions, Arrays, and Arithmetic**

Complete the scanner so it recognizes the new Topic 3 syntax.

* Return `RETURN` for the `return` keyword. Place the rule above the identifier rule so `return` is not scanned as an `ID`.
* Return `'-'`, `'*'`, and `'/'` for the rest of arithmetic. Confirm the `'/'` rule does not swallow the start of a comment. Longest match should settle it, so verify that it does.
* Return tokens for braces (`{`, `}`), brackets (`[`, `]`), and the comma used in parameter and argument lists.

### `parser.y`

**Grammar for Functions, Arrays, and Full Arithmetic**

Extend the grammar so a program is a list of declarations and function definitions.

Declarations and precedence:

* Add the `RETURN` token.
* Declare `%type <node>` for the new non-terminals: `decl_or_func_list`, `decl_or_func`, `func_def`, `params`, `param_list`, `param`, `return_stmt`, `block`, `func_call`, `arg_list`, and `args`.
* Add a `%right UMINUS` precedence declaration above the arithmetic operators as needed. Do not write precedence into the expression rules, because the `%left` declarations already handle it.

Program structure:

* `program`: Replace the flat statement list with `decl_or_func_list`.
* `decl_or_func_list`: One or more `decl_or_func`, built left recursively.
* `decl_or_func`: Either a `func_def` or a `decl`.

Functions, parameters, and blocks:

* `func_def`: `INT ID '(' params ')' block` and `INT ID '(' ')' block`.
* `param_list`: A single `param`, or `param_list ',' param`.
* `param`: `INT ID` for a scalar and `INT ID '[' ']'` for an array parameter, which carries no size because arrays are passed by reference.
* `block`: `'{' stmt_list '}'` and `'{' '}'`.

Statements:

* `stmt`: Add `return_stmt`, `block`, and `func_call ';'` as new alternatives.
* `decl`: Add array declaration `int arr[10];`. The size is a `NUM`, not an expression.
* `assign`: Add array element assignment `arr[i] = expr;`. The left side is stored as a tree in `arrayLHS`, with `var` left `NULL`.
* `return_stmt`: Both `return expr;` and `return;`.

Expressions:

* `expr`: Add array element `arr[i]`, a function call used for its value, subtraction, multiplication, division, unary minus with `%prec UMINUS`, and parentheses. Parentheses need no node. `( e )` just yields `e`.
* `func_call`: `ID '(' args ')'` and `ID '(' ')'`.
* `args` and `arg_list`: Build the argument list with `createArgList` so `tac.c` can walk it in source order.

Free identifier strings returned by the scanner after passing them to AST constructors, since the constructors duplicate the strings.

### `ast.h`

**New Node Kinds and Constructor Declarations**

* Add the function node kinds: `NODE_FUNC_DEF`, `NODE_PARAM`, `NODE_PARAM_LIST`, `NODE_FUNC_CALL`, `NODE_ARG_LIST`, `NODE_RETURN`.
* Add the block and array node kinds: `NODE_BLOCK`, `NODE_ARRAY_DECL`, `NODE_ARRAY_INDEX`.
* Add union members for each: `func_def` (name, params, body), `param` (name), `param_list` (param, next), `func_call` (name, args), `arg_list` (expr, next), `ret` (expr, `NULL` for a bare `return;`), `block` (stmt_list), `array_decl` (name, size, isParam), and `array_index` (name, index).
* Add the missing `arrayLHS` field to the assignment structure. The parser and `tac.c` both refer to it, but it is not currently defined.
* Declare all new constructors: the function constructors, `createBlock`, `createArrayDecl`, `createArrayParam`, and `createArrayIndex`.

**Agree on all node and field names with Stili before she starts `tac.c`.** Her half cannot compile without them.

### `ast.c`

**AST Constructors and Tree Printer**

Implement the new constructors: `createFuncDef`, `createParam`, `createParamList`, `createFuncCall`, `createArgList`, `createReturn`, `createBlock`, `createArrayDecl`, `createArrayParam`, and `createArrayIndex`.

Each constructor should allocate an `ASTNode`, set its node type, record the line number using `yylineno`, store its children or associated data, and return the node. Duplicate stored strings with `strdup`.

For arrays, give an array parameter size 0 and `isParam` 1, and give a declared array its real size and `isParam` 0. Code generation depends on telling those two apart.

Extend `printAST`:

* Print the two assignment shapes differently, a variable target and an array element target.
* Print the function-related nodes, indenting children one level deeper.
* Print `NODE_BLOCK`, `NODE_ARRAY_DECL`, and `NODE_ARRAY_INDEX`.

### `semantic.c`

**Function Table, Scopes, and Two-Pass Analysis**

Function table:

* Add a second table for functions, separate from variables. Record the name, the parameter count, and which parameters are arrays.
* Reset the function table in `initSemantic` and seed it with `print`, which takes one argument.
* Implement `findFunction`, `addFunction`, `countParams`, and `countArgs`. Comparing the parameter count to the argument count is the whole of call checking.

Expression checking:

* `NODE_FUNC_CALL`: Verify the function exists and the argument count matches, then check each argument expression.
* `NODE_ARRAY_INDEX`: Verify the array is declared and check the index expression. Do not check whether the index is in range.

Statement checking:

* `NODE_ASSIGN`: When the target is an array element, check the index expression. Otherwise check the variable name.
* `NODE_RETURN`: Report a `return` outside any function.
* `NODE_BLOCK`: Open a scope, check the statement list, and close the scope.
* `NODE_FUNC_CALL` as a statement.
* `NODE_ARRAY_DECL`: Reject a declared array with size 0 or negative. Parameters have no size to check.
* Add an array scope-insertion helper with its own error messages.

Function checking and the two-pass structure:

* Implement `addParamsToScope`, `checkFuncDef`, `registerFunction`, `registerFunctions`, and `checkFunctions`.
* Replace the single walk in `performSemanticAnalysis` with two passes. Pass 1 records every function signature and pass 2 checks every body, so a function may call one defined later in the file.

When an error occurs, report the line number using `node->lineno`, identify the problem, and explain how to fix it. Increment `semInfo.errorCount` and continue checking so multiple errors can be reported in one run.

**Do this part last.** It cannot be tested until the scanner, parser, and AST work.

---

## Stili

### `tac.c`

**Three Address Code Generation for Functions and Arrays**

Expression generation:

* `generateArgTAC`: Walk the argument list, emit code for each argument expression followed by an `ARG` instruction, and return the count. The count goes into the `CALL` instruction so nested calls like `f(1, g(2))` come out right.
* `NODE_FUNC_CALL`: Emit `ARG a1; ARG a2; t = CALL f, 2` and return the name of the temporary holding the result.
* `NODE_ARRAY_INDEX`: Emit a single `ARRAY_LOAD` (`t = arr[i]`) and return the temporary.

Statement generation:

* `NODE_ASSIGN`: If `arrayLHS` is set, evaluate the index and emit `ARRAY_STORE`. Otherwise emit a plain `ASSIGN`. `ARRAY_STORE` keeps the stored value in the `result` field.
* `NODE_RETURN`: Evaluate the expression, if any, and emit `RETURN`.
* `NODE_BLOCK`: Recurse into its statement list.
* `NODE_FUNC_CALL` as a statement: Evaluate and discard the result.
* `NODE_ARRAY_DECL`: Emit `ARRAY_DECL` for a real array and `PARAM` for an array parameter. Mark an array parameter with the type `"int[]"` so the code generator knows the slot holds an address.

Function structure:

* Implement `generateParamTAC`, `generateTACFuncDef`, and `generateTACList`. A function definition becomes `FUNC_BEGIN f; PARAM int a; ...; body; FUNC_END f`.
* Update `generateTAC` to walk the program with `generateTACList` instead of wrapping everything in an implicit `main`. Every function gets its own `FUNC_BEGIN` and `FUNC_END` pair.

Free operand temporaries only after emitting the instruction so live values are not accidentally reused.

### `codegen.c`

**TAC to MIPS: Calls, Returns, and Array Addressing**

Use the existing helper functions: `operandReg`, `defReg`, and `flushRegisters`.

Array addressing (`elementAddrReg`):

* Global array: `la $t, g_name`, the address of a `.data` label.
* Local array: `addi $t, $sp, offset`, since the elements are in the frame.
* Array parameter: `lw $t, offset($sp)`, since the slot holds someone else's base address.
* Element address is base plus index times 4, computed with `sll` by 2 and then `add`.

Instruction cases:

* `TAC_PARAM`: Copy each incoming `$a0-$a3` register into its frame slot, because the argument registers will be needed again as soon as this function calls something else.
* `TAC_ARG` and `TAC_CALL`: Put the arguments in `$a0-$a3` while the `$t` registers are still valid, **then** flush every dirty register to memory, then `jal` to the function label. The result comes back in `$v0`. Doing these two steps in the wrong order is the classic bug and only shows up once a function takes two arguments. An array passed as an argument is passed as its base address. More than four arguments is a reported error.
* `TAC_RETURN`: Place the value in `$v0`, then jump to the epilogue label. Do not fall through, because the epilogue is the only place that restores `$ra` and pops the frame.
* `TAC_ARRAY_LOAD` and `TAC_ARRAY_STORE`: Compute the element address once with `elementAddrReg`, then a single `lw` or `sw`.
* Complete the remaining "implement this part of the milestone" block in the register allocator section.

Use `operandReg` when reading values and `defReg` when writing values. Using the wrong helper can introduce unnecessary memory loads.

---

## Shared

* **Interface agreement:** Agree on node kinds and union field names (for example `NODE_FUNC_CALL`, `data.func_call.args`, `array_index.name` and `.index`) before either person starts. Reece owns `ast.h`, and Stili reads from it.
* **Testing:** Stili cannot run the `t3_*.cm` programs until Reece's front end works. Until then, Stili can hand-build small AST trees to test `tac.c` and `codegen.c`.
* **Write-up notes:** Mention that array names are not expression nodes (which is what stops this language having pointers) and that there is no array bounds checking at compile time or run time.