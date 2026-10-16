# Changes since the first submission

This lists every change to the lexer code since the first graded submission (commit `278a37c`). Each entry says what was wrong, what changed, and which file(s) changed.

## Bug fixes

| # | Problem in the first submission | Fix | File(s) |
|---|---|---|---|
| 1 | **`-c` flag was inverted.** `if (countOnly)` printed the token table *only* when `-c` was given, and printed nothing by default. | Condition corrected to `if (!countOnly)`. The default run shows the full token table; `-c` gives the compact view. | `main.c` |
| 2 | **`"` was treated as whitespace.** The whitespace class was `[\t\r\n"]+`, so a double quote was silently skipped instead of being reported as a lexical error. | Whitespace is now exactly `[ \t\r\n]+`. `"` is reported as an invalid character. | `scanner.l` |
| 3 | **Error messages did not say what was wrong.** They printed `unknown symbol` with no offending text. | Every error now prints `LEXICAL ERROR  line L, column C: <problem> '<text>'` and shows the bad text. Unprintable bytes appear as `\xNN`. | `scanner.l` |
| 4 | **An unterminated comment broke line counting.** The rule advanced the column by the comment's full length but never counted its newlines, so every later line number was wrong. The error's lexeme was the whole rest of the file, which garbled the token table. | New `lexError()` helper advances position with `skip()`, which counts newlines. The lexeme is shown as `/*`. | `scanner.l` |
| 5 | **An unterminated comment ending in `**` leaked a token.** The pattern ended in `\*?` (at most one star), so `/* text **` at end of file left a stray `STAR` token. | Pattern now ends in `\**`, so any number of trailing stars is part of the error. Covered by `tests/11_unterminated_stars.cm`. | `scanner.l` |
| 6 | **"Lines read" was off by one.** It printed the *current* line counter, which is one past the last line when the file ends in a newline (and short when a comment was unterminated, see #4). | The summary computes the real number of lines read. | `main.c` |
| 7 | **Error tokens counted as "recognized".** Every error incremented `tokenCount`, so the summary overstated the number of valid tokens. | Errors go through `lexError()`, which does not increment `tokenCount`. They are counted only under "Lexical errors". | `scanner.l`, `tokens.h` |
| 8 | **Error output could appear out of order.** Errors go to `stderr` and the table to `stdout`. When output was redirected, stdout buffering could print errors far from their rows. | `stdout` is flushed before each error, so the error prints next to its `ERROR` row. | `scanner.l` |
| 9 | **The title box was misaligned.** The right border did not line up. | Padding fixed. Long file names are cut to fit. | `main.c` |
| 10 | **Repository did not build from a clean clone.** `symtab.c`, `symtab.h` and `tests/07_symbol_table.cm` were never committed, but `main.c` and `scanner.l` include `symtab.h`. | These files must be committed with this resubmission (see "Before resubmitting"). | — |

## New lexical-error detection

Requirement 3 of the assignment is: *generates lexical errors, including their exact location*. The scanner now catches these errors too, each reported with its exact line and column:

| Input | Before | Now |
|---|---|---|
| `9lives` | Accepted silently as `NUM(9)` `ID(lives)` | One error: *identifier cannot start with a digit* |
| `99999999999` | Accepted as `NUM` (overflows `int`) | One error: *integer literal out of range for int* |
| `a & b`, `a \| b` | Generic unknown-symbol error | Error with a hint: *did you mean '&&'?* / *'\|\|'?* |
| `é` (UTF-8) | Two errors (one per byte), with garbled output | One error: *non-ASCII character not allowed* |

## Improvements

- **Error summary.** After the token and symbol tables, a **LEXICAL ERRORS** table lists every error again (line, column, text, problem), so none are lost in a long token listing.
- **Clearer verdict.** A failing run ends with `✗ N LEXICAL ERROR(S) — SOURCE IS NOT LEXICALLY CORRECT`. A passing run still ends with `✓ CERTIFIED LEXICALLY CORRECT` and exit status `0`.
- **Empty or comment-only files** now report "no tokens found" with exit status `0`, because there are no lexical errors. Before, they were treated as a failure (exit `1`) with a message meant for an unfinished scanner.
- **Tabs** advance to the next 4-column tab stop instead of always adding 4 columns, so reported columns match an editor.
- **Unknown command-line options** now print a warning instead of being silently ignored.
- **Symbol table** (`symtab.c` / `symtab.h`). Every identifier is entered into a hash table (djb2 hash, 211 buckets, separate chaining). The run prints each INSERT/LOOKUP and then the final table. See the README.

## Comments and documentation in the code

- Removed the leftover assignment-template text ("YOUR TASK STARTS HERE", "TODO") from `scanner.l` and `main.c`.
- Every group of rules in `scanner.l` now has a comment explaining **why** it is written that way:
  - longest match vs. first match
  - why keywords must come before `{ident}`
  - how the block-comment regular expression works
  - why the unterminated-comment rule can only win at end of file
  - why the error rules come last
- New helpers (`lexError`, the error log in `tokens.h`) and the scanning loop and summary logic in `main.c` are commented.

## Testing

- **`make check`** (new) runs every test file and compares the number of lexical errors and the exit status to the expected values. It prints `PASS`/`FAIL` per file and fails if any test fails.
- **`make test`** now also prints each file's exit status.
- New test files:
  - `08_error_kinds.cm`: one example of every kind of lexical error (7 errors)
  - `09_positions.cm`: exact columns after tabs, and one error for a multi-byte character (2 errors)
  - `10_valid_program.cm`: a realistic program using every construct of the language (0 errors, certified)
  - `11_unterminated_stars.cm`: the trailing-`**` unterminated-comment case (1 error)

Current result: all 11 tests pass, and the build has no warnings under `gcc -Wall` (flex 2.6.4, Ubuntu/WSL).

## Before resubmitting

1. Commit the new source and test files: `symtab.c`, `symtab.h`, `tests/07`–`tests/11`, and `CHANGES.md`.
2. Recommended: stop committing build outputs (`lexer`, `*.o`, `lex.yy.c`). They are generated by `make`, and the committed binary will not run on the grader's machine anyway. A `.gitignore` has been added for this. To remove the copies already tracked:
   `git rm --cached topic-1-lexical-analysis/lexer/lexer topic-1-lexical-analysis/lexer/*.o topic-1-lexical-analysis/lexer/lex.yy.c`
