/* ============================================================================
 *  TOPIC 1 · Compiler Design Phases
 * FILE: main.c   —   the driver for Phase 1
 * ----------------------------------------------------------------------------
 * This program is a compiler with exactly one phase in it.  It reads a source
 * file, asks the scanner for tokens until there are none left, and prints
 * what it got.  That is all a lexical analyzer ever does.
 *
 * The output is deliberately a TABLE rather than a running commentary,
 * because the table is the thing you hand to the parser in Topic 2 and the
 * thing you point at in your video when you say "here is what my scanner
 * produces for this input".
 *
 *   ./lexer program.cm          token table, symbol-table trace, symbol
 *                               table, error list and summary
 *   ./lexer program.cm -c       compact: symbol table, error list and
 *                               summary only (no token table or trace)
 *
 * EXIT STATUS
 *   0   no lexical errors — the source is certified lexically correct
 *   1   at least one lexical error, all of them reported above
 *   2   the file could not be opened
 *
 * A non-zero exit status matters: it is what lets `make` and a test script
 * tell success from failure without reading the output.
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokens.h"
#include "symtab.h"

/* --------------------------------------------------------------------------
 * The printable name of each token kind.  Keeping this next to the enum in
 * tokens.h is a maintenance trap — add a kind there and forget it here and
 * the table silently prints "?".  (A production compiler would generate both
 * from one list with an X-macro.)
 * -------------------------------------------------------------------------*/
const char* tokenName(TokenKind k) {
    switch (k) {
        case TOK_EOF:      return "EOF";  // just a switch that houses all the tokens names.

        case TOK_INT:      return "INT";
        case TOK_PRINT:    return "PRINT";
        case TOK_RETURN:   return "RETURN";
        case TOK_IF:       return "IF";
        case TOK_ELSE:     return "ELSE";
        case TOK_WHILE:    return "WHILE";
        case TOK_FOR:      return "FOR";
        case TOK_SWITCH:   return "SWITCH";
        case TOK_CASE:     return "CASE";
        case TOK_DEFAULT:  return "DEFAULT";
        case TOK_BREAK:    return "BREAK";

        case TOK_ID:       return "ID";
        case TOK_NUM:      return "NUM";

        case TOK_PLUS:     return "PLUS";
        case TOK_MINUS:    return "MINUS";
        case TOK_STAR:     return "STAR";
        case TOK_SLASH:    return "SLASH";
        case TOK_LT:       return "LT";
        case TOK_GT:       return "GT";
        case TOK_LE:       return "LE";
        case TOK_GE:       return "GE";
        case TOK_EQ:       return "EQ";
        case TOK_NE:       return "NE";
        case TOK_AND:      return "AND";
        case TOK_OR:       return "OR";
        case TOK_NOT:      return "NOT";
        case TOK_ASSIGN:   return "ASSIGN";

        case TOK_SEMI:     return "SEMI";
        case TOK_COLON:    return "COLON";
        case TOK_COMMA:    return "COMMA";
        case TOK_LPAREN:   return "LPAREN";
        case TOK_RPAREN:   return "RPAREN";
        case TOK_LBRACE:   return "LBRACE";
        case TOK_RBRACE:   return "RBRACE";
        case TOK_LBRACKET: return "LBRACKET";
        case TOK_RBRACKET: return "RBRACKET";

        case TOK_ERROR:    return "ERROR";

        default:           return "?"; // catch all case 
    }
}

const char* tokenCategory(TokenKind k) {
    switch (k) {
        case TOK_EOF:  // literally just a switch that houses all the types of tokens and returns a string that describes the category of the token
            return "end of file";

        case TOK_INT: case TOK_PRINT: case TOK_RETURN:
        case TOK_IF: case TOK_ELSE: case TOK_WHILE: case TOK_FOR:
        case TOK_SWITCH: case TOK_CASE: case TOK_DEFAULT: case TOK_BREAK:
            return "keyword";

        case TOK_ID:
            return "identifier";

        case TOK_NUM:
            return "integer literal";

        case TOK_PLUS: case TOK_MINUS: case TOK_STAR: case TOK_SLASH:
            return "arithmetic operator";

        case TOK_LT: case TOK_GT: case TOK_LE: case TOK_GE:
        case TOK_EQ: case TOK_NE:
            return "relational operator";

        case TOK_AND: case TOK_OR: case TOK_NOT:
            return "logical operator";

        case TOK_ASSIGN:
            return "assignment";

        case TOK_SEMI: case TOK_COLON: case TOK_COMMA:
        case TOK_LPAREN: case TOK_RPAREN:
        case TOK_LBRACE: case TOK_RBRACE:
        case TOK_LBRACKET: case TOK_RBRACKET:
            return "delimiter";

        case TOK_ERROR:
            return "lexical error";

        default:
            return "?"; // catch case
    }
}

int main(int argc, char** argv) {
    int countOnly = 0;
    atexit(symFree);

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source-file> [-c]\n", argv[0]);
        fprintf(stderr, "  -c   compact: symbol table and summary, no token table\n");
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) countOnly = 1;
        else fprintf(stderr, "Warning: ignoring unknown option '%s'\n", argv[i]);
    }

    yyin = fopen(argv[1], "r");
    if (!yyin) {
        fprintf(stderr, "Error: cannot open '%s'\n", argv[1]);
        return 2;
    }

    /* Banner.  The box is 66 columns inside; the file name is padded (or
     * cut) to fit so the right-hand border always lines up. */
    printf("\n╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  PHASE 1 · LEXICAL ANALYSIS                                      ║\n");
    printf("║  source: %-56.56s║\n", argv[1]);
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");

    symInit();

    if (!countOnly) {
        printf("   #  LINE  COL  TOKEN        LEXEME            KIND\n");
        printf(" ───  ────  ───  ───────────  ────────────────  ─────────────────────\n");
    }

    /* The scanning loop.  yylex() returns one token kind per call and 0 at
     * end of file; the lexeme and position of that token are left in
     * lastLexeme / lastLine / lastCol.  Error tokens (TOK_ERROR) appear in
     * the table too, so every problem shows up in context. */
    int n = 0;
    int kind;
    while ((kind = yylex()) != 0) {
        n++;
        if (!countOnly)
            printf(" %3d  %4d  %3d  %-11s  %-16.16s  %s\n",
                   n, lastLine, lastCol,
                   tokenName((TokenKind)kind), lastLexeme,
                   tokenCategory((TokenKind)kind));
    }

    fclose(yyin);

    /* The symbol table: every interaction the scanner had with it, then
     * the table as it stands at end of file. */
    if (!countOnly) symPrintTrace();
    symPrintTable();

    /* Every lexical error again, in one list, so they are easy to find
     * after a long token table. */
    if (lexErrorCount > 0) {
        printf("\n  LEXICAL ERRORS  (%d)\n", lexErrorCount);
        printf("  LINE  COL  TEXT              PROBLEM\n");
        printf("  ────  ───  ────────────────  ──────────────────────────────────\n");
        int shown = lexErrorCount < MAX_LEX_ERRORS ? lexErrorCount : MAX_LEX_ERRORS;
        for (int i = 0; i < shown; i++)
            printf("  %4d  %3d  %-16.16s  %s\n", lexErrors[i].line, lexErrors[i].col,
                   lexErrors[i].lexeme, lexErrors[i].message);
        if (lexErrorCount > shown)
            printf("  ... %d more not listed\n", lexErrorCount - shown);
    }

    /* lineNo is the line the scanner is CURRENTLY on.  If the file ends
     * with a newline, that is one past the last real line, so step back. */
    int linesRead = (colNo == 1) ? lineNo - 1 : lineNo;

    printf("\n───────────────────────────────────────────────────────────────────\n");
    printf("  Tokens recognized : %d  (valid tokens; errors counted below)\n", tokenCount);
    printf("  Lines read        : %d\n", linesRead);
    printf("  Lexical errors    : %d\n", lexErrorCount);
    printf("  Distinct names    : %d  (entries in the symbol table)\n", symSize());
    printf("───────────────────────────────────────────────────────────────────\n");

    /* Errors take priority: a file of nothing but bad characters has zero
     * valid tokens, and must be reported as errors, not as "empty". */
    if (tokenCount == 0 && lexErrorCount == 0) {
        /* An empty file, or one with only whitespace and comments, has no
         * lexical errors — but say so plainly rather than "certifying" a
         * program that has nothing in it. */
        printf("\n  ⚠  NO TOKENS FOUND\n");
        printf("     The file is empty or contains only whitespace and comments.\n");
        printf("     There are no lexical errors, but there is also no program.\n\n");
        return 0;
    }

    if (lexErrorCount == 0) {
        printf("\n  ✓ CERTIFIED LEXICALLY CORRECT\n");
        printf("    Every character in this file belongs to the language.\n");
        printf("    That is a claim about VOCABULARY only — this program says\n");
        printf("    nothing about whether the tokens form a legal sentence.\n");
        printf("    Answering that is the parser's job, in Topic 2.\n\n");
        return 0;
    }

    printf("\n  ✗ %d LEXICAL ERROR(S) — SOURCE IS NOT LEXICALLY CORRECT\n", lexErrorCount);
    printf("    Each one is listed above with its exact line and column.\n");
    printf("    Scanning continued past every error on purpose, so that one\n");
    printf("    run reports every problem rather than only the first.\n\n");
    return 1;
}
