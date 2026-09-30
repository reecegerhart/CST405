%{
/* ============================================================================
 *  TOPIC 1 · Compiler Design Phases
 * FILE: scanner.l   —   Phase 1: Lexical analysis
 * ----------------------------------------------------------------------------
 * Flex reads the rules below and writes a finite automaton (a DFA) that
 * recognizes all of them at once.  We never write the automaton by hand; we
 * write the vocabulary as regular expressions, and two rules decide
 * everything about how the generated scanner behaves:
 *
 *   LONGEST MATCH   "<=" beats "<" because it consumes more characters.
 *                   Position in the file is irrelevant for this.
 *   FIRST MATCH     when two rules match the SAME number of characters, the
 *                   one written EARLIER wins.  This is the only reason the
 *                   keyword rules must appear above the identifier rule:
 *                   "int" matches both, both match three characters, and
 *                   whichever is written first decides the outcome.
 *
 * Both rules are also what make the ERROR rules below work: an error pattern
 * only fires when no legal pattern matches as much (or more) of the input.
 *
 * WHAT COMES NEXT
 *   In Topic 2 this same file feeds bison instead of main.c.  The actions
 *   keep returning token numbers; bison's parser.tab.h replaces tokens.h,
 *   and the patterns do not change at all.
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "tokens.h"
#include "symtab.h"

/* ---- Counters and "last token" state shared with main.c (see tokens.h) -- */
int lexErrorCount = 0;
int tokenCount    = 0;
int lineNo        = 1;
int colNo         = 1;

char lastLexeme[256];
int  lastLine;
int  lastCol;

/* Every lexical error is also kept in a list, so main.c can repeat them all
 * in one place at the end of the run (they are otherwise scattered through
 * a long token table). */
LexError lexErrors[MAX_LEX_ERRORS];

/* Record where the token that is being returned started, then advance the
 * column past it.  Flex does not track columns, so we keep the column by
 * hand — and a column number is half of what makes an error findable.
 * Used for every VALID token. */
static int tok(TokenKind k) {
    lastLine = lineNo;
    lastCol  = colNo;
    snprintf(lastLexeme, sizeof lastLexeme, "%s", yytext);
    colNo += (int)yyleng;
    tokenCount++;
    return (int)k;
}

/* Advance the position counters over text that produces no token at all:
 * whitespace, newlines, and the insides of comments.  A tab advances to the
 * next tab stop (every 4 columns), which is how most editors display it. */
static void skip(const char* s, int n) {
    for (int i = 0; i < n; i++) {
        if (s[i] == '\n')      { lineNo++; colNo = 1; }
        else if (s[i] == '\t') colNo += 4 - ((colNo - 1) % 4);
        else                   colNo++;
    }
}

/* Report one lexical error and return it to the driver as a TOK_ERROR token.
 *
 *   shown   what to print as the lexeme (NULL = the matched text itself).
 *           Used for the unterminated comment, whose matched text is the
 *           entire rest of the file.
 *   msg     a human-readable explanation of what is wrong.
 *
 * The error is printed immediately (with its exact line and column), counted,
 * and stored for the summary.  Then the position counters are advanced over
 * the bad text with skip(), so that line numbers stay correct even when the
 * bad text spans several lines.  Scanning then CONTINUES: one run reports
 * every error in the file, not just the first. */
static int lexError(const char* shown, const char* msg) {
    lastLine = lineNo;
    lastCol  = colNo;

    if (shown) {
        snprintf(lastLexeme, sizeof lastLexeme, "%s", shown);
    } else if (yyleng == 1 && (yytext[0] < 32 || yytext[0] == 127)) {
        /* Control characters would garble the output, so show a code. */
        snprintf(lastLexeme, sizeof lastLexeme, "\\x%02X",
                 (unsigned char)yytext[0]);
    } else {
        snprintf(lastLexeme, sizeof lastLexeme, "%s", yytext);
    }

    fflush(stdout);   /* keep the error next to its row in the token table */
    fprintf(stderr, "LEXICAL ERROR  line %d, column %d: %s '%s'\n",
            lastLine, lastCol, msg, lastLexeme);

    if (lexErrorCount < MAX_LEX_ERRORS) {
        LexError* e = &lexErrors[lexErrorCount];
        e->line = lastLine;
        e->col  = lastCol;
        snprintf(e->lexeme,  sizeof e->lexeme,  "%.31s", lastLexeme);
        snprintf(e->message, sizeof e->message, "%s", msg);
    }
    lexErrorCount++;

    skip(yytext, (int)yyleng);
    return (int)TOK_ERROR;
}
%}

%option noyywrap
%option nounput
%option noinput
%option 8bit

/* ---------------------------------------------------------------------------
 * NAMED PATTERNS
 * A definition is pure substitution — {digit}+ literally becomes [0-9]+
 * before flex sees it.  They keep the rules below readable.
 * -------------------------------------------------------------------------*/
digit     [0-9]
letter    [a-zA-Z_]
ident     {letter}({letter}|{digit})*
number    {digit}+

/* A number immediately followed by letters, e.g. 123abc.  This is longer
 * than {number}, so longest-match picks it and we can reject the whole
 * thing as ONE error instead of silently splitting it into NUM(123) ID(abc). */
badident  {digit}+{letter}({letter}|{digit})*

/* A multi-byte UTF-8 character (é, “, …).  Matched as a unit so that one
 * foreign character produces one error, not one error per byte. */
utf8char  [\xC0-\xF7][\x80-\xBF]+

%%

 /* ══════════════════════════════════════════════════════════════════════
  * 1. WHITESPACE AND COMMENTS — produce no token.
  *    skip() is called so lineNo / colNo stay correct, including across a
  *    multi-line block comment.
  * ══════════════════════════════════════════════════════════════════════ */

[ \t\r\n]+                      { skip(yytext, (int)yyleng); }

 /* Line comment: "//" up to (not including) the newline. */
"//"[^\n]*                      { skip(yytext, (int)yyleng); }

 /* Block comment.  Read the body as: any non-star, OR a run of stars that
  * is NOT followed by '/', repeated; then one or more stars and the '/'.
  * This correctly handles stars inside the body (e.g. "a * b" or a
  * closing "**" + "/") and never stops early on a lone '*'. */
"/*"([^*]|\*+[^*/])*\*+"/"      { skip(yytext, (int)yyleng); }

 /* Unterminated block comment: the same body but with no closing "* /".
  * For a well-formed comment the rule above always matches one character
  * MORE (the final '/'), so longest-match means this rule can only win when
  * the comment really does run to end of file.  Reported once, at the
  * position where the comment opened. */
"/*"([^*]|\*+[^*/])*\**         { return lexError("/*", "unterminated block comment (opened here, never closed)"); }

 /* ══════════════════════════════════════════════════════════════════════
  * 2. KEYWORDS — must be written ABOVE {ident}.  "int" matches both rules
  *    with the same length, and the earlier rule wins the tie.  Longer words
  *    such as "integer" still become identifiers, because longest-match is
  *    applied first and {ident} matches more characters.
  * ══════════════════════════════════════════════════════════════════════ */

"int"       { return tok(TOK_INT); }
"print"     { return tok(TOK_PRINT); }
"return"    { return tok(TOK_RETURN); }
"if"        { return tok(TOK_IF); }
"else"      { return tok(TOK_ELSE); }
"while"     { return tok(TOK_WHILE); }
"for"       { return tok(TOK_FOR); }
"switch"    { return tok(TOK_SWITCH); }
"case"      { return tok(TOK_CASE); }
"default"   { return tok(TOK_DEFAULT); }
"break"     { return tok(TOK_BREAK); }

 /* ══════════════════════════════════════════════════════════════════════
  * 3. IDENTIFIERS AND NUMBERS
  * ══════════════════════════════════════════════════════════════════════ */

 /* Identifier: a letter or '_' followed by letters, digits or '_'.
  * Every identifier is entered into the symbol table (symtab.c).  The
  * insert happens BEFORE tok() advances colNo, so the table records where
  * the identifier starts, not where it ends. */
{ident}     {
    symInsert(yytext, lineNo, colNo);
    return tok(TOK_ID);
}

 /* Integer literal.  The language's only numeric type is `int`, so a
  * literal that does not fit in one is rejected here, where its exact
  * position is still known, instead of silently wrapping later. */
{number}    {
    errno = 0;
    long v = strtol(yytext, NULL, 10);
    if (errno == ERANGE || v > INT_MAX)
        return lexError(NULL, "integer literal out of range for int");
    return tok(TOK_NUM);
}

 /* 123abc — see {badident} above. */
{badident}  { return lexError(NULL, "identifier cannot start with a digit"); }

 /* ══════════════════════════════════════════════════════════════════════
  * 4. OPERATORS
  *    The two-character operators are listed first for readability only;
  *    longest-match is what actually makes "<=" win over "<".
  * ══════════════════════════════════════════════════════════════════════ */

"<="        { return tok(TOK_LE); }
">="        { return tok(TOK_GE); }
"=="        { return tok(TOK_EQ); }
"!="        { return tok(TOK_NE); }
"&&"        { return tok(TOK_AND); }
"||"        { return tok(TOK_OR); }

"+"         { return tok(TOK_PLUS); }
"-"         { return tok(TOK_MINUS); }
"*"         { return tok(TOK_STAR); }
"/"         { return tok(TOK_SLASH); }
"<"         { return tok(TOK_LT); }
">"         { return tok(TOK_GT); }
"!"         { return tok(TOK_NOT); }
"="         { return tok(TOK_ASSIGN); }

 /* A single '&' or '|' is NOT an operator in this language (only && and
  * || are).  Give a more helpful message than "unknown character". */
"&"         { return lexError(NULL, "single '&' is not an operator (did you mean '&&'?)"); }
"|"         { return lexError(NULL, "single '|' is not an operator (did you mean '||'?)"); }

 /* ══════════════════════════════════════════════════════════════════════
  * 5. DELIMITERS
  * ══════════════════════════════════════════════════════════════════════ */

";"         { return tok(TOK_SEMI); }
":"         { return tok(TOK_COLON); }
","         { return tok(TOK_COMMA); }
"("         { return tok(TOK_LPAREN); }
")"         { return tok(TOK_RPAREN); }
"{"         { return tok(TOK_LBRACE); }
"}"         { return tok(TOK_RBRACE); }
"["         { return tok(TOK_LBRACKET); }
"]"         { return tok(TOK_RBRACKET); }

 /* ══════════════════════════════════════════════════════════════════════
  * 6. ERRORS — written LAST, so they only fire when nothing legal matches.
  *    `.` matches any single character except newline (newlines are
  *    always consumed by the whitespace rule).  Each bad character is
  *    reported with its exact line and column, then scanning continues.
  * ══════════════════════════════════════════════════════════════════════ */

{utf8char}  { return lexError(NULL, "non-ASCII character not allowed"); }
.           { return lexError(NULL, "invalid character"); }

%%
