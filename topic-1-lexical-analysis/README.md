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
| `main.c` | Driver — reads a file, calls `yylex()` until EOF, prints a token table and summary |
| `Makefile` | Builds `lexer` from `scanner.l` + `main.c` |
| `tests/*.cm` | Sample source files exercising specific behaviors |

## Building

```bash
make          # builds ./lexer
make test     # runs ./lexer over every tests/*.cm
make clean    # removes build artifacts
```

## Running

```bash
./lexer <source-file>        # full token table + summary
./lexer <source-file> -c     # summary only, no table
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
- **Errors**: any other character is reported with its line/column and counted in `lexErrorCount`, but scanning continues so every bad character in a file is reported in one run
- **Unterminated `/*`**: reported once, at the position the comment opened

## Notes

- Line/column tracking is manual (`lineNo`/`colNo` in `scanner.l`) — Flex tracks lines natively via `%option yylineno`, but columns need to be counted by hand for useful error messages.
- You may see a `"/*" within comment [-Wcomment]` warning from GCC when building. This comes from the instructional prose at the top of `scanner.l` containing the literal characters `/*` inside its own comment block — it's cosmetic and doesn't affect correctness.
