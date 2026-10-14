# Topic 2 — Mini Compiler for a Starter Language

**CST-405 Compiler Design · Grand Canyon University · Project 2**
**Team:** Reece Gerhart and Stylianos Regas

## 1. What this project is

This project is a complete compiler, built end to end, for the smallest language worth compiling. It reads a source file written in a tiny C-style language, checks that the program is valid, and produces MIPS assembly that runs in the SPIM simulator. The language supports exactly four things: declaring integer variables, assigning to them, adding with `+`, and printing an integer.

```c
int x;
int y;
int total;
x = 5;
y = 10;
total = x + y + 3;
print(total);      // prints 18
```

The point of the project is the pipeline rather than the language. Every real compiler moves a program through the same stages, and this one implements all six of them, with each stage handing a more structured representation to the next.

```
source (.cm) → tokens → AST → checked AST → TAC → optimized TAC → MIPS (.s)
               Phase 1   2        3            4        5            6
```

The scanner, driver, header files, temporary allocator and register cache were provided. Our work is the grammar and its actions, the AST, the symbol table, the semantic checks, TAC generation, the optimizer, and MIPS instruction selection. Section 9 lists who built which piece.

## 2. Requirements and how to build

You need `gcc`, `make`, `flex`, `bison`, and SPIM (or QtSPIM) to run the generated assembly. Ubuntu or any similar Linux works, and Windows users can use WSL with Ubuntu.

```bash
cd compiler
make            # builds ./minicompiler
make test       # compiles every tests/*.cm and runs the result in SPIM
make clean      # removes everything make produced
```

`make` runs flex on `scanner.l` and bison on `parser.y` to generate `lex.yy.c` and `parser.tab.c/.h`, compiles every source file, and links the `minicompiler` executable. The two generated files should never be edited by hand, since changes belong in the `.l` and `.y` files.

## 3. How to run it

```bash
./minicompiler tests/t2_01_basics.cm out.s        # full trace of all six phases
./minicompiler tests/t2_01_basics.cm out.s -q     # quiet: only errors and a summary
spim -file out.s                                  # run the generated MIPS
```

Each successful compile also writes `out.tac` (the unoptimized intermediate code) and `out.optimized.tac`. Reading those two side by side is the quickest way to see what the optimizer did. The full trace prints a banner for each phase, the AST, the scope and symbol information, both TAC listings, optimizer statistics, register allocation statistics, and per-phase timing.

## 4. The six phases in detail

### Phase 1 — Lexical and syntax analysis (`scanner.l`, `parser.y`)

Flex turns the character stream into tokens: the keywords `int` and `print`, identifiers, integer literals, the symbols `= + ( ) ;`, and so on. Comments (`// line` and `/* block */`) and whitespace are discarded here, so no later phase ever sees them. Bison then checks the token stream against this grammar:

```
program     → stmt_list
stmt_list   → stmt | stmt_list stmt
stmt        → decl | assign | print_stmt
decl        → 'int' ID ';'
assign      → ID '=' expr ';'
expr        → NUM | ID | expr '+' expr
print_stmt  → 'print' '(' expr ')' ';'
```

Each rule carries a semantic action that builds one AST node. The `program` rule is the only place that assigns the global `root`, which is how the rest of the compiler finds the finished tree. The scanner duplicates every identifier it returns, and the AST constructors duplicate it again, so the grammar actions `free()` the scanner's copy after constructing the node. Addition is declared `%left`, which makes `a + b + c` parse as `(a + b) + c`. A syntax error is reported through `yyerror` with the line number, and the compiler stops before any later phase runs.

### Phase 2 — AST construction (`ast.c`, `ast.h`)

The AST records the structure of the program without the punctuation. There are seven node kinds: `NODE_NUM`, `NODE_VAR`, `NODE_BINOP`, `NODE_DECL`, `NODE_ASSIGN`, `NODE_PRINT`, and `NODE_STMT_LIST`. Each node stores its kind, the source line it came from (taken from `yylineno`, so later phases can report errors precisely), and a union holding only the fields that kind needs. Strings are copied with `strdup` because the scanner reuses its buffer.

Because the grammar is left recursive, `createStmtList(list, stmt)` receives the existing list first and the new statement second, which preserves source order. `printAST` walks the tree recursively and prints one node per line, indented two spaces per level. A statement list prints both children at the same indentation, because a list is a sequence rather than nesting.

For `total = x + y + 3;` the tree is:

```
ASSIGN total
  BINOP +
    BINOP +
      VAR x
      VAR y
    NUM 3
```

The tree leans left, which is the visible proof that addition is left associative. This shape also determines the evaluation order in the next phases, since the inner `x + y` has to be computed before the outer `+ 3`.

### Phase 3 — Semantic analysis (`semantic.c`, `symtab.c`)

Parsing only proves a program is shaped correctly, not that it means anything. For example, `print(ghost);` is grammatical even if `ghost` was never declared. The semantic analyzer walks the AST and enforces these rules:

- a variable cannot be declared twice in the same scope,
- a variable must be declared before it is assigned or read,
- a declaration is processed before the statements that follow it, so in `int x; x = x + 1;` the `x` on the right-hand side is already in scope.

The right-hand side of an assignment is checked even when the target itself is invalid, so one run reports every error rather than stopping at the first. Each error prints the line number, the offending name, and a suggested fix, then increments `semInfo.errorCount`. If the count is nonzero the compiler stops before generating code. Names that collide with reserved words are also rejected.

There are two symbol tables, and they answer different questions. The scope stack inside `semantic.c` answers "is this name visible here?" and disappears after analysis. The storage map in `symtab.c` answers "where does this name live at run time?" and is used later by code generation. `addVar` gives each integer a four-byte slot, with offsets starting at 0 and increasing in steps of four, so the first variable is at `0($sp)`, the second at `4($sp)`, and so on. It returns `-1` for a redeclaration. `lookupSymbol` searches the local table first and then the global one, so a local can shadow a global.

### Phase 4 — Three-address code (`tac.c`)

Three-address code (TAC) is the intermediate language between the tree and the machine. Every instruction does at most one operation with at most two operands and one result, so a nested expression becomes a short sequence of simple steps. The generator walks the AST recursively. A number node returns its literal as a string, a variable node returns its name, and a binary-operation node allocates a temporary, generates code for both operands, emits `result = left + right`, and returns the temporary. Operand temporaries are released only after the instruction is emitted, so a live value is never reused too early.

For `total = x + y + 3` the generator produces:

```
t1 = x + y
t0 = t1 + 3
total = t0
```

A full program also gets a `FUNC_BEGIN main` and `FUNC_END main` wrapper, `DECL int x` markers so the backend knows which stack slots to reserve, `PRINT` instructions, and a final `RETURN 0`.

### Phase 5 — Optimization (`tac.c`)

The optimizer copies the TAC while applying two transformations, repeating until a full pass changes nothing (with a safety cap of 20 passes). It is a fixed-point loop because one optimization exposes the next.

**Constant propagation** remembers which names currently hold a known constant and substitutes the constant into later instructions. After `x = 5`, a later `y = x + 1` becomes `y = 5 + 1`. **Constant folding** then evaluates any operation whose operands are both literals at compile time, so `5 + 1` becomes `6`. Remembered facts are cleared at labels and function starts, because control flow could reach those points from elsewhere, and a fact is dropped whenever its variable is overwritten with something unknown.

On the first test program this turns `t1 = x + y; t0 = t1 + 3` into `t1 = 15; t0 = 18`, and the three `PRINT` instructions print the literals `5`, `10`, and `18` directly. The counts of each rewrite are recorded in `optStats` and printed in the trace. We apply only these two techniques, so the instruction count stays the same (14 before and after for the first test). The savings show up as cheaper instructions rather than fewer of them, because dead-code elimination is not part of this project.

### Phase 6 — MIPS code generation (`codegen.c`)

The code generator maps each TAC opcode to MIPS instructions, using the provided register cache to decide where values live.

| TAC | MIPS produced |
|---|---|
| `DECL int x` | a comment noting the stack slot (the frame layout already reserved it) |
| `x = 5` or `x = y` | `li` for a literal, `move` for a register |
| `t = a + b` | `add` on the operand registers into the destination register |
| `PRINT v` | `li $v0, 1` and `syscall` to print the integer, then `syscall 4` with the `__nl` string for a newline |
| `RETURN v` | value into `$v0`, write back live registers, then `j main__epilogue` |

Helper functions keep the loads and stores correct. `operandReg` returns the register holding a value, loading it from the stack or loading a literal if needed. `defReg` returns a register for a value about to be written. `flushRegisters` writes modified registers back to their stack slots before a return. The prologue allocates the activation record with `addi $sp, $sp, -N` and saves `$ra`, and the epilogue restores them and exits with syscall 10.

## 5. How the AST drives code generation

The AST is the single structure that every later phase reads, and each phase uses a different traversal of the same tree.

The semantic analyzer walks it in statement order and asks whether each name is declared. The TAC generator walks it recursively and uses the shape of the tree to decide evaluation order: the deepest expressions are generated first, each returns the name of where its result lives, and its parent uses those names as operands. That is why `(x + y) + 3` becomes `t1 = x + y` followed by `t0 = t1 + 3` and never the reverse. The tree makes precedence and associativity explicit, so the code generator never has to re-parse anything.

The TAC then becomes a flat list that the optimizer and code generator process one instruction at a time, which is far easier than working with a tree. Following one line all the way through:

```
source:      total = x + y + 3;
AST:         ASSIGN(total, BINOP(+, BINOP(+, x, y), 3))
TAC:         t1 = x + y ; t0 = t1 + 3 ; total = t0
optimized:   t1 = 15 ; t0 = 18 ; total = 18      (x=5, y=10 known)
MIPS:        li $t2, 15 ; li $t3, 18 ; li $t4, 18
```

## 6. Project structure

| File | Purpose |
|---|---|
| `scanner.l` | Flex rules that produce tokens (provided) |
| `parser.y` | Bison grammar and the actions that build the AST |
| `ast.c`, `ast.h` | AST node types, constructors, and `printAST` |
| `symtab.c`, `symtab.h` | Storage map: variable name to stack offset |
| `semantic.c`, `semantic.h` | Scope tracking and semantic error reporting |
| `tac.c`, `tac.h` | TAC generation, temporary allocation, and the optimizer |
| `codegen.c`, `codegen.h` | Register cache, frame layout, and MIPS instruction selection |
| `main.c` | Driver that runs the six phases and prints timing |
| `trace.h` | Controls verbose versus quiet (`-q`) output |
| `Makefile` | `make`, `make test`, `make clean` |
| `tests/` | Test programs `t2_01` to `t2_05` |
| `contributions.md` | Per-member task breakdown |

## 7. Testing

The provided test suite is thorough for a language this small, so we did not write additional tests. It covers a full program, left associativity, both comment styles, a semantic error, and a syntax error. Run all of it with `make test`.

| Test | What it checks | Expected result |
|---|---|---|
| `t2_01_basics.cm` | Declare, assign, add, print | prints `5`, `10`, `18` |
| `t2_02_chained.cm` | Left-associative chained addition | prints `6`, `21` |
| `t2_03_comments.cm` | Line and block comments are ignored | prints `42` |
| `t2_04_errors_undeclared.cm` | Semantic analysis | rejected, error names `ghost` and its line |
| `t2_05_errors_syntax.cm` | Syntax analysis | rejected with a syntax error |

The last two programs are meant to fail, so `make test` prints `COMPILE FAILED` for them. That is the correct outcome, because the compiler exits with a nonzero status when it rejects a program.

In `t2_05`, the semicolon is missing at the end of line 7 (`x = 5`). The parser cannot know a statement has ended until it reads the next token, which is `y` on line 8, so the error is reported at line 8. This is standard behavior for a missing-semicolon error and is why the message points at the line after the mistake.

## 8. Updates since the last submission

> **[Fill in before submitting.]** The rubric grades how fully prior instructor feedback was incorporated. List each piece of feedback from the last submission and the change that addressed it, for example: "Feedback: *comment the code more*. Change: added file and function header comments in `semantic.c` and `tac.c`."

## 9. Team contributions

| Member | Files and responsibilities |
|---|---|
| **Reece Gerhart** | `parser.y` (all grammar rules and AST-building actions); `semantic.c` (expression and statement checking, duplicate and undeclared errors); `symtab.c` (declaration with stack offsets, name lookup with local-over-global shadowing) |
| **Stylianos Regas** | `ast.c` (constructors and `printAST`); `tac.c` (TAC generation, constant folding and propagation); `codegen.c` (TAC-to-MIPS instruction selection) |

Each member also recorded an individual video that explains the code, outlines their assigned tasks, describes how the AST is used to produce TAC and MIPS, and demonstrates the compiler running.

| Member | Video |
|---|---|
| Reece Gerhart | https://www.youtube.com/watch?v=i-XEn4M_GGU |
| Stylianos Regas | https://mygcuedu6961-my.sharepoint.com/:v:/r/personal/sregas_my_gcu_edu/Documents/Videos/Clipchamp/Video%20Project%2010/Exports/Video%20Project%2010.mp4?d=w868d91257a00412cbf62f4f2e2c0fa3e&csf=1&web=1&e=p3isCD |

## 10. Known limitations

The language is intentionally minimal. It has a single type (`int`), one operator (`+`), no control flow, no functions, and no input. Syntax errors use bison's generic message with a line number, with no custom messages such as "missing `;`". Only constant folding and constant propagation are applied, so unused temporaries remain in the TAC. The later topics extend this foundation with more operators, control flow, functions, and arrays.