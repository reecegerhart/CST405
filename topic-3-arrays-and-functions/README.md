# Topic 3 — Compiling Complex Variables and Functions

**CST-405 Compiler Design · Grand Canyon University · Project 3**
**Team:** Reece Gerhart and Stylianos Regas

## 1. What this project is

This project extends the Topic 2 compiler from a flat list of statements to a real program structure: functions with parameters and return values, global and local variables, arrays passed by reference, nested scopes, and full integer arithmetic. It still reads a small C-style language and produces MIPS assembly that runs in SPIM.

```c
int shared[4];
int sum3(int a[]) { return a[0] + a[1] + a[2]; }
int main() {
    shared[0] = 10; shared[1] = 20; shared[2] = 30;
    print(sum3(shared) + 40);      // prints 100
    return 0;
}
```

The main new idea is the activation record. Each call builds its own frame on the stack, which holds the saved return address, the parameters, the locals and the compiler's temporaries. That frame is what lets calls nest and lets the same name mean different things in different functions.

```
source (.cm) → tokens → AST → checked AST → TAC → optimized TAC → MIPS (.s)
               Phase 1   2        3            4        5            6
```

The work is split at the AST. Reece built the front end (phases 1 to 3) and Stylianos built the back end (phases 4 to 6). Section 9 lists who built which piece.

## 2. Requirements and how to build

The compiler needs `flex`, `bison`, `gcc`, `make`, and [SPIM](http://spimsimulator.sourceforge.net/) or QtSPIM. On Windows, run these under WSL.

```bash
cd compiler
make
```

## 3. How to run it

```bash
./minicompiler tests/t3_05_scope.cm out.s          # full trace of all six phases
./minicompiler tests/t3_05_scope.cm out.s -q       # quiet: errors and summary only
spim -file out.s                                   # run the generated MIPS
make test                                          # compile and run every tests/*.cm
make clean                                         # remove everything make produced
```

Compiling also writes `out.tac` and `out.optimized.tac`. Reading those two side by side shows what the optimizer did.

## 4. The six phases in detail

### Phase 1 — Lexical analysis (`scanner.l`)

The scanner adds the `return` keyword, the rest of arithmetic (`-`, `*`, `/`), braces, brackets and the comma. Longest match keeps `/` from swallowing a comment: on `//` or `/*` the comment rule matches more characters than the single-character division rule. Carriage returns count as whitespace, so files with Windows line endings scan cleanly.

### Phase 2 — Syntax analysis and the AST (`parser.y`, `ast.c`, `ast.h`)

A program is now a list of declarations and function definitions, so executable statements appear only inside a function body. Precedence comes from the `%left` and `%right` declarations rather than from extra grammar layers, and `%prec UMINUS` makes unary minus bind tightest. Parentheses produce no node, because the grouping they asked for is already recorded in the shape of the tree.

### Phase 3 — Semantic analysis (`semantic.c`)

Semantic analysis uses a scope stack and two passes. Pass 1 records every function signature, and pass 2 checks every body, so a function may call one defined later in the file. It reports undeclared names, undefined functions, wrong argument counts, invalid array sizes and duplicate declarations, and it continues after an error so one run can report several.

### Phase 4 — Three-address code (`tac.c`)

| Source | TAC |
|---|---|
| `int f(int a, int v[]) { ... }` | `FUNC_BEGIN f`, `PARAM int a`, `PARAM int[] v`, body, `FUNC_END f` |
| `x = f(1, g(2));` | `ARG 1`, `ARG 2`, `t0 = CALL g, 1`, `ARG t0`, `t1 = CALL f, 2`, `x = t1` |
| `y = a[i];` | `t0 = a[i]` (`ARRAY_LOAD`), `y = t0` |
| `a[i] = y;` | `a[i] = y` (`ARRAY_STORE`, with the value kept in the `result` field) |
| `return e;` | code for `e`, `RETURN t0` |

The count on `CALL` tells the back end how many buffered `ARG`s belong to that call, which is what makes nested calls work. For the same reason, the temporaries an `ARG` names stay allocated until after its `CALL`.

Because a function's frame is one flat table of names, a local declared while its name is already visible gets a unique TAC name such as `x.1`. The dot cannot appear in a source identifier, so the name cannot collide with anything.

### Phase 5 — Optimization (`tac.c`)

The optimizer runs algebraic simplification, constant folding, constant and copy propagation, dead code elimination, and unreachable code removal until a pass changes nothing. Three rules matter here. A call forgets everything known about non-temporaries, because the callee may change them. Dead code elimination only touches real temporaries. Copies are never propagated into an `ARG`, because an `ARG` is not read until its `CALL`, and a nested call may have reused the copy's source by then.

### Phase 6 — MIPS code generation (`codegen.c`)

```
high address
┌──────────────────────────────┐
│ saved $ra                    │  frameSize-4($sp)
├──────────────────────────────┤
│ (reserved / alignment)       │  frameSize-8($sp)
├──────────────────────────────┤
│ parameters, locals, arrays,  │  0($sp) .. localBytes-1($sp)
│ and compiler temporaries     │
└──────────────────────────────┘  <- $sp
low address
```

Arrays are the clearest example of how a name turns into an address:

| Array kind | Base address | Why |
|---|---|---|
| Global | `la $t, g_name` | It is a `.data` label |
| Local | `addi $t, $sp, offset` | The elements are in the frame |
| Parameter | `lw $t, offset($sp)` | The slot holds the caller's base address |

The element address is then the base plus the index times 4, computed with `sll` and `add`. A call moves its arguments into `$a0`-`$a3`, flushes every dirty register to memory because `$t0`-`$t9` are caller-saved, and then runs `jal`. A return puts the value in `$v0`, flushes so that a global still in a register reaches memory, and jumps to the epilogue, which is the only place that restores `$ra` and pops the frame. Each function copies its incoming `$a` registers into its frame immediately, because it will need them again as soon as it makes a call.

## 5. How the AST drives code generation

The AST is the handoff between the two halves of the compiler. `generateTAC` walks the top-level list, and each function definition becomes a `FUNC_BEGIN`/`FUNC_END` pair. Global declarations stay outside those pairs, which is how `emitDataSection` knows to put them in `.data`. Inside a function, each node kind maps to the instructions shown in Phase 4, and `codegen.c` never sees the tree: it works only from the optimized TAC.

Two design points follow from how arrays appear in the tree:

- **Array names are not expression nodes when indexed.** `NODE_ARRAY_INDEX` stores the array's name as a plain string next to its index expression, so nothing can compute with an array's address. This is what stops the language from having pointers. A bare array name can appear only as a call argument, where the back end passes its base address.
- **There is no array bounds checking**, either at compile time or at run time. The index is an arbitrary expression whose value generally cannot be known at compile time, and the language has no run-time check. An out-of-range index reads or writes whatever memory is at that address.

## 6. Project structure

```
topic-3-arrays-and-functions/
├── Contributions.md
├── README.md
└── compiler/
    ├── Makefile
    ├── scanner.l  parser.y         Phases 1-2
    ├── ast.c  ast.h                Phase 2: the tree
    ├── semantic.c  semantic.h      Phase 3
    ├── symtab.c  symtab.h          Storage map used by phase 6
    ├── tac.c  tac.h                Phases 4-5
    ├── codegen.c  codegen.h        Phase 6
    ├── main.c  trace.h             Driver and tracing
    └── tests/
        ├── t2_*.cm  t3_*.cm        Programs that must compile and run
        └── errors/                 Programs that must be rejected
```

## 7. Testing

Run the programs with `make test`. Each one states its expected output in its first line.

| Test | What it checks | Expected output |
|---|---|---|
| `t2_01_basics` | Topic 2 program in `main()` | `5 10 18` |
| `t2_02_chained` | Left-associative addition | `6 21` |
| `t2_03_comments` | Line and block comments | `42` |
| `t3_01_arithmetic` | Precedence, parentheses, unary minus | `14 20 9 3 -20 7` |
| `t3_02_globals` | Globals across a call | `5 12 17` |
| `t3_03_arrays` | Global and local arrays by reference | `30 100 60` |
| `t3_04_functions` | Multiple parameters, nested calls | `6 5 7 8` |
| `t3_05_scope` | Parameter shadows a global | `1 2 1` |
| `t3_06_fact_chain` | `fact(5)` is 120, through a chain of functions | `120 30` |
| `t3_07_shadowing` | Nested block scopes, including arrays | `2 20 3 2 1 10 4 8 7 5 6` |
| `t3_08_temp_names` | Variables named like temporaries (`total`, `t1x`) | `51 2` |
| `t3_09_register_pressure` | More live variables than registers | `102 203 90 13` |
| `t3_10_returns` | Early return, bare return, global written before return | `6 5 8 40` |
| `t3_11_calling_convention` | Argument order, four arguments, forwarded arrays | `7 1234 2468 8 3 4 10 9 8 77 66 2 14 -28 75` |

The programs in `tests/errors/` must be rejected, so they are kept out of `make test`. Each one states the error it expects in its first line. To try one:

```bash
./minicompiler tests/errors/t3_e2_wrong_arg_count.cm out.s -q
```

| Test | Expected rejection |
|---|---|
| `t2_04_errors_undeclared` | `ghost` is undeclared |
| `t2_05_errors_syntax` | Syntax error at line 8 (the missing semicolon is on line 7) |
| `t3_e1_undefined_function` | Function is never defined |
| `t3_e2_wrong_arg_count` | Argument count does not match |
| `t3_e3_array_size_zero` | Array size must be positive |
| `t3_e4_undeclared_array` | Array is undeclared |
| `t3_e5_duplicates` | Duplicate function, parameter, variable and array |
| `t3_e6_too_many_args` | Call passes more than four arguments |
| `t3_e7_too_many_params` | Function declares more than four parameters |

**Topic 2 tests.** The original Topic 2 programs have statements at file scope, which the Topic 3 grammar correctly rejects as syntax errors. The versions here make the smallest change that lets them run, which is wrapping the body in `main()`.

**`fact(5)`.** The language has no `if` yet, so a recursive factorial could never stop. `t3_06_fact_chain` computes `fact(5)` through a chain of five functions, which exercises the same saved `$ra` and per-call `n`.

**Other checks.** Beyond these programs, we confirmed the tests can actually fail by reverting 15 individual pieces of the back end and the scanner fix, one at a time; the suite caught all 15. We also generated 800 random programs covering functions, arrays and shadowing, then compared the compiler's output with `gcc`'s. An early run of that comparison found the copy propagation bug described in Phase 5. After the fix, all 800 programs matched, across 4,977 printed values.

## 8. Updates since the last submission

> **[Fill in before submitting.]** List each piece of feedback from the last submission and the change that addressed it.

## 9. Team contributions

| Member | Files and responsibilities |
|---|---|
| **Reece Gerhart** | `scanner.l` (new tokens); `parser.y` (program structure, functions, arrays, full arithmetic); `ast.h`, `ast.c` (node kinds, constructors, `printAST`); `semantic.c` (function table, scope stack, two-pass analysis) |
| **Stylianos Regas** | `tac.c` (TAC for calls, returns, arrays, blocks and function structure; unique names for shadowing locals); `codegen.c` (array addressing, parameters, calling convention, returns, array load and store); regression tests in `tests/` |

Stylianos also corrected code in the provided template and one line of the scanner:

| File | Change | What it prevents |
|---|---|---|
| `tac.c` | Temporaries are recognized by their exact form | `total` being treated as a temporary and freed while a live value depended on it |
| `tac.c` | Copies are not propagated into `ARG` | A nested call overwriting an argument |
| `codegen.c` | Registers in use by the current instruction are never evicted | An instruction overwriting its own operand when all ten registers are full |
| `scanner.l` | `\r` counts as whitespace | False lexical errors on every line of a Windows checkout |

Each member also recorded an individual video that explains the code, outlines their assigned tasks, and demonstrates the compiler running.

| Member | Video |
|---|---|
| Reece Gerhart | *[paste link]* |
| Stylianos Regas | *[paste link]* |

## 10. Known limitations

- **No control flow.** There is no `if` or loop yet, so recursion cannot terminate. Topic 4 adds them.
- **At most four parameters or arguments.** Only `$a0`-`$a3` are used, and the code generator reports anything more as an error.
- **No bounds checking.** See section 5.
- **Array and scalar misuse is not fully checked.** Semantic analysis compares argument counts but not kinds, which is all the project's guidance asks of call checking. As a result:
  - `s = a;`, where `a` is an array, compiles and reads the first element.
  - Passing an array to a scalar parameter compiles and passes its address.
  - Passing a scalar to an `int v[]` parameter compiles and fails at run time with a bad address.
  - Indexing a scalar is rejected only by the code generator.
  - Assigning to a bare array name (`a = 1;`) is not rejected by semantic analysis. If the array is indexed afterward, the code generator stops with a confusing message that names the assigned value rather than the array.
- **A missing `main` is not reported by the compiler.** SPIM reports the undefined symbol instead.
- **A function that ends without `return`** gives its caller whatever is left in `$v0`.
- **Run-time arithmetic errors are only reported by SPIM.** Signed overflow on `add` and `sub`, and division by zero, print an exception notice, and then the program continues.
