# Lexer — Phase 1: Lexical Analysis

A hand-built scanner (Flex) for a small C-like language, generating a stream of typed tokens from source text. This is the first stage of a larger compiler pipeline:

```
scanner -> parser -> ast -> semantic -> tac -> codegen
^^^^^^^  you are here
```

The scanner's job is vocabulary only — recognizing that `count`, `=`, `1`, `;` are valid pieces of the language. It says nothing about whether those pieces form a legal sentence; that's the parser's job in the next phase.

## Files

| File | Purpose |
|---|---|
| `tokens.h` | Token kind enum (`TokenKind`), shared globals (`lineNo`, `colNo`, `tokenCount`, `lexErrorCount`), scanner interface |
| `scanner.l` | Flex source — the rules that turn characters into tokens |
| `symtab.h` / `symtab.c` | Symbol table — a hash table of every distinct identifier, plus a trace of each insert/lookup |
| `main.c` | Driver — reads a file, calls `yylex()` until EOF, prints a token table, the symbol table, and a summary |
| `Makefile` | Builds `lexer` from `scanner.l` + `main.c` |
| `tests/*.cm` | Sample source files exercising specific behaviors |

## Building

```bash
make          # builds ./lexer
make test     # runs ./lexer over every tests/*.cm (full output + exit status)
make check    # PASS/FAIL: verifies each test's expected error count and exit status
make clean    # removes build artifacts
```

## Running

```bash
./lexer <source-file>        # full token table + summary
./lexer <source-file> -c     # symbol table + summary, no token table or trace
```

Each table row shows: token number, line, column, token name, lexeme, and category (keyword, identifier, operator, delimiter, etc.).

## Exit status

| Code | Meaning |
|---|---|
| `0` | No lexical errors — source is lexically valid |
| `1` | At least one lexical error (all reported with line/column) |
| `2` | Source file could not be opened |

## What the scanner recognizes

- **Keywords**: `int print return if else while for switch case default break`
- **Identifiers / numbers**: `{letter}({letter}|{digit})*`, `{digit}+`
- **Operators**: arithmetic (`+ - * /`), relational (`< > <= >= == !=`), logical (`&& || !`), assignment (`=`)
- **Delimiters**: `; : , ( ) { } [ ]`
- **Comments**: `//` to end of line, `/* ... */` spanning multiple lines (contents are skipped, not tokenized)
- **Errors** — each is reported immediately as `LEXICAL ERROR  line L, column C: <problem> '<text>'`, shown as an `ERROR` row in the token table, and listed again in a **LEXICAL ERRORS** summary at the end. Scanning always continues, so one run reports every error:
  - any character not in the language (`@ # $ ' "` ...), shown as `\xNN` if unprintable
  - a non-ASCII (UTF-8) character — one error per character, not one per byte
  - a number running into letters, e.g. `9lives` — one error, not `NUM` + `ID`
  - an integer literal too large for `int`
  - a lone `&` or `|` (only `&&` / `||` exist), with a hint
  - an unterminated `/*` — reported once, at the position the comment opened
- **Tabs** advance to the next 4-column tab stop, so reported columns match an editor set to 4-space tabs

See [CHANGES.md](CHANGES.md) for everything changed since the first submission.

## Symbol table

Every identifier the scanner matches is entered into a symbol table (`symtab.c`) from the `{ident}` rule in `scanner.l`:

```c
{ident}     {
    symInsert(yytext, lineNo, colNo);
    return tok(TOK_ID);
}
```

`symInsert()` hashes the name (djb2, 211 buckets, separate chaining) and looks it up. The first time a name is seen it is an **INSERT**: a new entry records the name, where it was first seen, and a reference count of 1. Every later occurrence is a **LOOKUP**: the entry is found and its reference count and line list are updated. So the table ends up with one entry per *distinct* name, however many times it appears. Keywords never reach the table because their flex rules sit above `{ident}`.

The run prints two things after the token table:

- **Symbol table interactions** — one row per identifier scanned: INSERT or LOOKUP, the name, its hash bucket, and the result.
- **Symbol table** — the final contents: index, name, bucket, first line:col, total references, and the lines it appears on.

At this phase the table only knows *which names exist*. Types, scopes and memory addresses come from later phases (the semantic analyzer's scope stack and the code generator's storage map).

`tests/07_symbol_table.cm` exercises it: 5 distinct names, 20 interactions.

## Notes

- Line/column tracking is manual (`lineNo`/`colNo` in `scanner.l`) — Flex tracks lines natively via `%option yylineno`, but columns need to be counted by hand for useful error messages.
- The build is warning-free under `gcc -Wall`.
- "Tokens recognized" in the summary counts **valid** tokens only; error tokens are counted separately under "Lexical errors".
