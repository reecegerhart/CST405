# Contributions

Topic 4: Compiling Loops

The work is split by pipeline stage, the same way as Topic 3. Reece owns the front end (scanner, parser, AST constructors, semantic analysis). Stylianos owns the back end (three address code, the optimizer and MIPS generation). The AST is the handoff between the two halves.

The assignment asks for at least one loop and for order of operations. We are doing both `while` and `for`, plus `break`, and the six relational operators placed in the precedence table below the arithmetic operators.

---

## Interface (already in `ast.h`, do not rename)

So that both halves can be written at the same time, the loop interface was added to `ast.h` first. The back end is already written against these names, so **please do not rename them.** If one of them needs to change, tell Stylianos so `tac.c` can change too.

| Node kind | Union member | Fields | Constructor |
|---|---|---|---|
| `NODE_WHILE` | `data.while_stmt` | `condition`, `body` | `createWhile(condition, body)` |
| `NODE_FOR` | `data.for_stmt` | `init`, `condition`, `update`, `body` | `createFor(init, condition, update, body)` |
| `NODE_BREAK` | (none) | (none) | `createBreak(void)` |

Relational operators reuse `NODE_BINOP` with these one-character codes in `binop.op` (they already match `opText()` in `ast.c`):

| Source | `<` | `>` | `<=` | `>=` | `==` | `!=` |
|---|---|---|---|---|---|---|
| `op` code | `'<'` | `'>'` | `'l'` | `'g'` | `'e'` | `'n'` |

Language design decisions agreed for this topic:

* **Any part of a `for` header may be empty.** The parser passes `NULL` for a missing `init`, `condition` or `update`.
* **A missing `for` condition means an infinite loop**, exactly as in C. `for (;;) { ... }` only ends through `break` or `return`.
* **`init` and `update` are assignments without their own semicolon.** They are ordinary `NODE_ASSIGN` nodes, so `tac.c` generates them like any other assignment.
* **A comparison yields `1` or `0`.** There is no separate boolean type. Any non-zero value counts as true in a loop condition.
* **`break` leaves the innermost enclosing loop only.**

---

## Reece Gerhart

The `for` loop and the relational operators are already working end to end (see Stylianos's section), so they can be used as a worked example. What is left in the front end is `while` and `break`. Each spot is still marked `TODO (Topic 4)` in the source.

### `scanner.l`

**Tokens for `while` and `break`**

* Return `WHILE` and `BREAK` for the keywords. Place them next to the existing `"for"` rule, above the identifier rule, so they are not scanned as an `ID`.

### `parser.y`

**Grammar for `while` and `break`**

* Declare the tokens `WHILE BREAK`, next to the existing `%token FOR`.
* Declare `%type <node>` for `while_stmt` and `break_stmt`.
* `stmt`: Add `while_stmt` and `break_stmt` as new alternatives, next to the existing `for_stmt`.
* `while_stmt`: `WHILE '(' expr ')' stmt` builds `createWhile($3, $5)`.
* `break_stmt`: `BREAK ';'` builds `createBreak()`.

### `ast.c`

**`while` and `break` Constructors and Tree Printer**

* Implement `createWhile` and `createBreak`, using the field names in `ast.h`. `createFor` shows the pattern. Set `node->lineno = yylineno` in each one so later phases report errors against the right line.
* `createBreak` stores no data. What `break` means depends only on where it appears.
* Extend `printAST` for `NODE_WHILE` (label the condition and the body, like the existing `NODE_FOR` case) and for `NODE_BREAK` (one line, no children).

### `semantic.c`

**Break Depth and `while` Checking**

* Add a `breakDepth` counter: how many loops enclose the statement being checked. Reset it in `initSemantic`.
* `NODE_WHILE`: Check the condition, then check the body with `breakDepth++` before and `breakDepth--` after.
* `NODE_FOR`: This case already checks all four children. Add `breakDepth++` / `breakDepth--` around the body check, which is marked with a TODO.
* `NODE_BREAK`: Report an error when `breakDepth == 0` (a `break` outside any loop). No grammar can express this rule, which is why semantic analysis exists.

When an error occurs, report the line number using `node->lineno`, say what is wrong and how to fix it, increment `semInfo.errorCount`, and keep checking.

**Done when** `make test` passes `t4_01_while` (`55 10`) and `t4_03_break` (`21 4`). The other `t4_*` and all `t3_*` tests already pass.

---

## Stylianos Regas

### Front end for the `for` loop

**`for` and the Relational Operators, End to End**

So that a loop works through the whole compiler, the `for` loop and the comparison operators it needs were added to the front end. `while` and `break` are left for Reece.

* `scanner.l`: Returns `FOR`, plus `LE GE EQ NE` and `'<' '>'`.
* `parser.y`:
  * Adds the `for_stmt`, `for_init`, `for_cond`, and `for_update` rules. Each part of the header can be empty and yields `NULL`.
  * `for_init` and `for_update` are `ID '=' expr` without a semicolon.
  * Adds the six relational operators to `expr`.
  * Relational precedence follows C: `%left EQ NE`, then `%left '<' '>' LE GE`, both looser than `+ -`. So `a + 1 < b * 2` parses as `(a + 1) < (b * 2)`, which covers the assignment's order of operations requirement.
* `ast.c`: Adds `createFor` and the `NODE_FOR` case of `printAST`, which labels `INIT`, `COND`, `UPDATE`, and `BODY` and prints `(none)` for a missing part.
* `semantic.c`: Adds the `NODE_FOR` case, which checks `init`, `condition`, `body`, and `update`. An undeclared loop variable gets the normal undeclared-variable error.

Verified with `make test` under SPIM:

* `t4_02_for` prints `30 120`, `t4_04_nested` prints `225 9`, `t4_05_relational` prints `1 0 1 0 1 0`, and `t4_06_optimize` prints `10`.
* All Topic 3 tests still pass, and bison reports no grammar conflicts.

### `ast.h`

**Interface for the Loop Nodes**

* Added `NODE_WHILE`, `NODE_FOR`, and `NODE_BREAK` to the node enum.
* Added the `while_stmt` and `for_stmt` union members, with comments explaining what a `NULL` child means.
* Declared `createWhile`, `createFor`, and `createBreak` (implemented by Reece in `ast.c`).

### `tac.c`

**Labels, Loop Lowering, Break, and Branch Simplification**

* `newLabel()`: Hands out `L0`, `L1`, ... from a counter that is never reset, so labels never repeat anywhere in the program.
* **Break-label stack:** `pushBreakLabel`, `popBreakLabel`, and `currentBreakLabel`. A loop pushes its exit label around its body, and `break` emits `GOTO` to the label on top. This handles nested loops with no extra code.
* **Relational operators:** Maps the op codes `< > l g e n` to `TAC_LT`, `TAC_GT`, `TAC_LE`, `TAC_GE`, `TAC_EQ`, and `TAC_NE`.
* **Loop lowering:** Both loops go through one routine, `generateLoop(init, cond, update, body)`. A `while` is treated as a `for` with no init and no update (the desugaring approach). Loops are tested at the top:

  ```
      <init>
  Lstart:
      t = cond
      IF_FALSE t GOTO Lend
      <body>
      <update>
      GOTO Lstart
  Lend:
  ```

  When the condition is missing, no test is emitted at all, so the loop is infinite.
* **`NODE_BREAK`:** Emits `GOTO` to the current break label. If none exists, it reports an internal error as a safety check, since `semantic.c` should already have rejected the program.
* **Branch simplification (optimizer technique 7):** `IF_FALSE 0` becomes `GOTO`, and `IF_FALSE k` (k ≠ 0) is deleted. `IF_TRUE` is handled the same way in reverse. The next pass then removes the unreachable code after the new `GOTO`, which is why the optimizer runs until a pass changes nothing.

### `codegen.c`

**MIPS for Comparisons and Control Flow**

* Relational opcodes map to `slt`, `sgt`, `sle`, `sge`, `seq`, and `sne`, and share the arithmetic case: two registers in, one out.
* `TAC_LABEL` becomes `L:`, `TAC_GOTO` becomes `j L`, `TAC_IF_FALSE` becomes `beq $t, $zero, L`, and `TAC_IF_TRUE` becomes `bne $t, $zero, L`.
* **Register cache rule:** Dirty registers are written back and the cache is forgotten (`flushRegisters`) at every label and before every jump, because a label can be reached by more than one path. In the conditional branches the condition is read into a register first, then registers are flushed, then the branch tests that register.

### `tests/backend/loop_backend_test.c`

**Back-End Test Harness**

Builds the AST for every `t4_*.cm` program by hand, so the back end can be tested before the parser exists. It also includes two extra cases: an infinite `for (;;)` with nested breaks, and constant `while (0)` / `while (1 < 2)` conditions. Each program runs through the real `generateTAC`, `optimizeTAC`, and `generateMIPSFromTAC`. A small TAC interpreter then runs both the unoptimized and optimized code and checks the printed output against the expected values. It also reports how many TAC instructions each version **executed**, which is the optimized-vs-unoptimized benchmark.

```bash
cd compiler
gcc -Wall -I. -o loop_backend_test tests/backend/loop_backend_test.c tac.c codegen.c symtab.c ast.c
./loop_backend_test /tmp            # writes .tac, .optimized.tac and .s files to /tmp
spim -file /tmp/t4_01_while.s
```

---

## Shared

* **Interface agreement:** The node kinds, field names, and op codes are listed at the top of this file and already in `ast.h`. Reece reads `ast.h` and implements the constructors. Stylianos's code reads from it.
* **Branches:** Stylianos works on `StyliBranchTopic4` and Reece works on `main`. Reece should start from this branch, or merge it in first, because his remaining TODOs sit next to the `for` code in the same four front-end files. Starting from the scaffold on `main` would mean redoing the `for` work and causing merge conflicts.
* **Testing:** Until the front end is finished, the back end is tested with `tests/backend/loop_backend_test.c`. Once both halves are merged, `make test` must pass all six `t4_*` tests plus the Topic 3 `t3_*` tests.
* **Videos:** Each of us records our own video covering how the code works, the tasks we did (from this file), the language design choices above, and a live run.
* **Write-up notes:** Mention that `for` is desugared into the same lowering as `while`, that a missing `for` condition is an infinite loop, that `break` is resolved entirely in TAC generation by the label stack, and that the register cache must be flushed at every label and branch.
