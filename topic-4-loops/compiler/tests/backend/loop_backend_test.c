/* =========================================================================
 * CST-405  ·  TOPIC 4  ·  Compiling Loops
 * FILE: tests/backend/loop_backend_test.c  —  back-end test harness
 * -------------------------------------------------------------------------
 * WHY THIS EXISTS
 *   The back end (tac.c, codegen.c) and the front end (scanner, parser, ast.c,
 *   semantic.c) are written in parallel by different team members.  Until the
 *   parser can build loop nodes, the back end cannot be tested through the
 *   real compiler.  This harness builds the AST for each t4_*.cm program BY
 *   HAND, then runs it through the real back end:
 *
 *       hand-built AST -> generateTAC -> optimizeTAC -> generateMIPSFromTAC
 *
 *   To check the results without SPIM, it also contains a tiny TAC
 *   interpreter.  It runs both the UNOPTIMIZED and the OPTIMIZED TAC and
 *   compares what each prints with the expected output written at the top of
 *   the matching tests/t4_*.cm file.  If the optimizer ever changes what a
 *   program computes, the two runs disagree and the test fails.
 *
 *   The .s files it writes are real MIPS from codegen.c; run them in SPIM to
 *   check the code generator end to end.
 *
 * BUILD AND RUN (from the compiler/ directory)
 *   gcc -Wall -I. -o loop_backend_test tests/backend/loop_backend_test.c \
 *       tac.c codegen.c symtab.c ast.c
 *   ./loop_backend_test [output-dir]        # .s / .tac files go to output-dir
 *   spim -file <output-dir>/t4_01_while.s   # optional: run the MIPS
 *
 *   This file does not need flex or bison and does not use the parser, so
 *   it keeps working while the front end is still being written.  Once the
 *   front end is finished, `make test` is the real test and this harness is
 *   only a second opinion.
 * ========================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "tac.h"
#include "codegen.h"

/* Normally defined by main.c and the flex scanner; this harness has neither. */
int quiet = 1;
int yylineno = 0;

/* ------------------------------------------------------------------------
 * AST BUILDING HELPERS
 * The Topic 3 constructors in ast.c are reused as-is.  The loop nodes are
 * built directly here, using only the fields agreed in ast.h, so this file
 * does not depend on createWhile/createFor/createBreak being written yet.
 * ----------------------------------------------------------------------*/
static ASTNode* mkLoopNode(NodeType t) {
    ASTNode* n = calloc(1, sizeof(ASTNode));
    n->type = t;
    return n;
}
static ASTNode* WHILE(ASTNode* c, ASTNode* body) {
    ASTNode* n = mkLoopNode(NODE_WHILE);
    n->data.while_stmt.condition = c;
    n->data.while_stmt.body = body;
    return n;
}
static ASTNode* FOR(ASTNode* init, ASTNode* c, ASTNode* upd, ASTNode* body) {
    ASTNode* n = mkLoopNode(NODE_FOR);
    n->data.for_stmt.init = init;
    n->data.for_stmt.condition = c;
    n->data.for_stmt.update = upd;
    n->data.for_stmt.body = body;
    return n;
}
static ASTNode* BREAK(void) { return mkLoopNode(NODE_BREAK); }

/* Short names so each test reads like the source program */
#define NUM(v)        createNum(v)
#define VAR(x)        createVar(x)
#define OP(o, a, b)   createBinOp(o, a, b)
#define DECL(x)       createDecl("int", x)
#define SET(x, e)     createAssign(x, e)
#define PRINT(e)      createPrint(e)
#define RET(e)        createReturn(e)

/* Build a left-recursive statement list, the shape the parser produces. */
static ASTNode* seq(int n, ASTNode** s) {
    ASTNode* list = s[0];
    for (int i = 1; i < n; i++) list = createStmtList(list, s[i]);
    return list;
}
#define SEQ(...) seq(sizeof((ASTNode*[]){__VA_ARGS__}) / sizeof(ASTNode*), (ASTNode*[]){__VA_ARGS__})
#define BLOCK(...) createBlock(SEQ(__VA_ARGS__))
#define MAIN(...)  createFuncDef("main", NULL, BLOCK(__VA_ARGS__))

/* ------------------------------------------------------------------------
 * A TINY TAC INTERPRETER (main only, no calls, no arrays)
 * Enough to execute every Topic 4 test.  Prints go into `outBuf`.
 * ----------------------------------------------------------------------*/
#define MAX_ENV 64
static char  envName[MAX_ENV][32];
static long  envVal[MAX_ENV];
static int   envCount;

static long* slot(const char* name) {
    for (int i = 0; i < envCount; i++) if (strcmp(envName[i], name) == 0) return &envVal[i];
    strncpy(envName[envCount], name, 31);
    envVal[envCount] = 0;
    return &envVal[envCount++];
}
static int isNum(const char* s) {
    if (!s || !*s) return 0;
    int i = (s[0] == '-') ? 1 : 0;
    if (!s[i]) return 0;
    for (; s[i]; i++) if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}
static long val(const char* s) { return isNum(s) ? atol(s) : *slot(s); }

static TACInstr* findLabel(TACInstr* head, const char* l) {
    for (TACInstr* c = head; c; c = c->next)
        if (c->op == TAC_LABEL && strcmp(c->result, l) == 0) return c;
    fprintf(stderr, "interp: missing label %s\n", l);
    exit(1);
}

/* Returns the number of TAC instructions EXECUTED — a simple dynamic cost
 * metric, used as the optimized-vs-unoptimized benchmark — or -1 if the
 * step limit was hit (runaway loop).  Labels and declarations cost nothing
 * at run time, so they are not counted. */
static long interpret(TACList* list, char* outBuf, size_t n) {
    envCount = 0; outBuf[0] = '\0';
    long steps = 0;
    for (TACInstr* c = list->head; c; c = c->next) {
        if (c->op != TAC_LABEL && c->op != TAC_DECL && c->op != TAC_FUNC_BEGIN) steps++;
        if (steps > 1000000) return -1;
        long a, b;
        switch (c->op) {
            case TAC_ADD: case TAC_SUB: case TAC_MUL: case TAC_DIV:
            case TAC_LT: case TAC_GT: case TAC_LE: case TAC_GE: case TAC_EQ: case TAC_NE:
                a = val(c->arg1); b = val(c->arg2);
                switch (c->op) {
                    case TAC_ADD: *slot(c->result) = a + b; break;
                    case TAC_SUB: *slot(c->result) = a - b; break;
                    case TAC_MUL: *slot(c->result) = a * b; break;
                    case TAC_DIV: *slot(c->result) = a / b; break;
                    case TAC_LT:  *slot(c->result) = a <  b; break;
                    case TAC_GT:  *slot(c->result) = a >  b; break;
                    case TAC_LE:  *slot(c->result) = a <= b; break;
                    case TAC_GE:  *slot(c->result) = a >= b; break;
                    case TAC_EQ:  *slot(c->result) = a == b; break;
                    case TAC_NE:  *slot(c->result) = a != b; break;
                    default: break;
                }
                break;
            case TAC_NEG:    *slot(c->result) = -val(c->arg1); break;
            case TAC_ASSIGN: *slot(c->result) = val(c->arg1); break;
            case TAC_PRINT: {
                char tmp[32]; snprintf(tmp, sizeof tmp, "%s%ld", outBuf[0] ? " " : "", val(c->arg1));
                strncat(outBuf, tmp, n - strlen(outBuf) - 1);
                break;
            }
            case TAC_GOTO:     c = findLabel(list->head, c->arg1); break;
            case TAC_IF_FALSE: if (val(c->arg1) == 0) c = findLabel(list->head, c->arg2); break;
            case TAC_IF_TRUE:  if (val(c->arg1) != 0) c = findLabel(list->head, c->arg2); break;
            case TAC_RETURN:   return steps;
            default: break;     /* LABEL, DECL, FUNC_BEGIN/END: nothing to do */
        }
    }
    return steps;
}

/* ------------------------------------------------------------------------
 * RUN ONE TEST
 * ----------------------------------------------------------------------*/
static const char* outDir = ".";
static int failures = 0;

static void runTest(const char* name, ASTNode* program, const char* expected) {
    char path[512], buf[512];

    initTAC();
    generateTAC(program);
    snprintf(path, sizeof path, "%s/%s.tac", outDir, name);
    saveTACToFile(path);

    char gotUnopt[256];
    long r1 = interpret(getUnoptimizedTAC(), gotUnopt, sizeof gotUnopt);

    optimizeTAC();
    snprintf(path, sizeof path, "%s/%s.optimized.tac", outDir, name);
    saveOptimizedTACToFile(path);

    char gotOpt[256];
    long r2 = interpret(getOptimizedTAC(), gotOpt, sizeof gotOpt);

    snprintf(path, sizeof path, "%s/%s.s", outDir, name);
    generateMIPSFromTAC(path);

    OptStats os = getOptStats();
    int ok = (r1 >= 0 && r2 >= 0 && strcmp(gotUnopt, expected) == 0 && strcmp(gotOpt, expected) == 0);
    if (!ok) failures++;

    printf("%-18s %s   expected [%s]  unoptimized [%s]  optimized [%s]\n",
           name, ok ? "PASS" : "FAIL", expected,
           r1 < 0 ? "RUNAWAY" : gotUnopt, r2 < 0 ? "RUNAWAY" : gotOpt);
    snprintf(buf, sizeof buf, "TAC %d -> %d in %d passes (fold %d, cprop %d, copy %d, "
             "dead %d, unreach %d, branch %d)",
             os.instructionsBefore, os.instructionsAfter, os.passes, os.constFold,
             os.constProp, os.copyProp, os.deadCode, os.unreachable, os.branch);
    printf("                   %s\n", buf);
    printf("                   executed TAC instructions: %ld unoptimized -> %ld optimized\n", r1, r2);
}

int main(int argc, char** argv) {
    if (argc > 1) outDir = argv[1];

    /* t4_01_while.cm */
    runTest("t4_01_while", MAIN(
        DECL("i"), DECL("s"),
        SET("i", NUM(1)), SET("s", NUM(0)),
        WHILE(OP('l', VAR("i"), NUM(10)), BLOCK(
            SET("s", OP('+', VAR("s"), VAR("i"))),
            SET("i", OP('+', VAR("i"), NUM(1))))),
        PRINT(VAR("s")),
        PRINT(OP('-', VAR("i"), NUM(1))),
        RET(NUM(0))), "55 10");

    /* t4_02_for.cm */
    runTest("t4_02_for", MAIN(
        DECL("i"), DECL("s"), DECL("p"),
        SET("s", NUM(0)),
        FOR(SET("i", NUM(0)), OP('<', VAR("i"), NUM(5)), SET("i", OP('+', VAR("i"), NUM(1))),
            BLOCK(SET("s", OP('+', VAR("s"), OP('*', VAR("i"), VAR("i")))))),
        PRINT(VAR("s")),
        SET("p", NUM(1)),
        FOR(SET("i", NUM(1)), OP('l', VAR("i"), NUM(5)), SET("i", OP('+', VAR("i"), NUM(1))),
            BLOCK(SET("p", OP('*', VAR("p"), VAR("i"))))),
        PRINT(VAR("p")),
        RET(NUM(0))), "30 120");

    /* t4_03_break.cm */
    runTest("t4_03_break", MAIN(
        DECL("i"),
        SET("i", NUM(0)),
        WHILE(OP('<', VAR("i"), NUM(100)), BLOCK(
            SET("i", OP('+', VAR("i"), NUM(7))), BREAK())),
        PRINT(OP('+', VAR("i"), NUM(14))),
        SET("i", NUM(0)),
        FOR(SET("i", NUM(0)), OP('<', VAR("i"), NUM(100)), SET("i", OP('+', VAR("i"), NUM(1))),
            BLOCK(BREAK())),
        PRINT(OP('+', VAR("i"), NUM(4))),
        RET(NUM(0))), "21 4");

    /* t4_04_nested.cm */
    runTest("t4_04_nested", MAIN(
        DECL("i"), DECL("j"), DECL("s"), DECL("c"),
        SET("s", NUM(0)), SET("c", NUM(0)),
        FOR(SET("i", NUM(1)), OP('l', VAR("i"), NUM(5)), SET("i", OP('+', VAR("i"), NUM(1))), BLOCK(
            FOR(SET("j", NUM(1)), OP('l', VAR("j"), NUM(5)), SET("j", OP('+', VAR("j"), NUM(1))), BLOCK(
                SET("s", OP('+', VAR("s"), OP('*', VAR("i"), VAR("j")))),
                SET("c", OP('+', VAR("c"), NUM(1))))))),
        PRINT(VAR("s")),
        PRINT(OP('-', VAR("c"), NUM(16))),
        RET(NUM(0))), "225 9");

    /* t4_05_relational.cm */
    runTest("t4_05_relational", MAIN(
        PRINT(OP('<', NUM(3), NUM(5))),
        PRINT(OP('<', NUM(5), NUM(3))),
        PRINT(OP('l', NUM(4), NUM(4))),
        PRINT(OP('>', NUM(4), NUM(9))),
        PRINT(OP('e', NUM(7), NUM(7))),
        PRINT(OP('n', NUM(7), NUM(7))),
        RET(NUM(0))), "1 0 1 0 1 0");

    /* t4_06_optimize.cm */
    runTest("t4_06_optimize", MAIN(
        DECL("a"), DECL("b"), DECL("c"),
        SET("a", OP('+', OP('*', NUM(2), NUM(3)), NUM(4))),
        SET("b", OP('+', VAR("a"), NUM(0))),
        SET("c", OP('*', VAR("b"), NUM(1))),
        PRINT(VAR("c")),
        RET(NUM(0))), "10");

    /* ---- extra cases not covered by the t4_* files ---------------------- */

    /* for (;;) with no condition is infinite; only break leaves it.
     *   i = 0;
     *   for (;;) {
     *       for (;;) { i = i + 1; break; }     inner break leaves inner only
     *       while (i < 100) { i = i * 2; }      i: 1 2 4 ... 128
     *       i = i + 10;
     *       break;                              outer break
     *   }
     *   print(i);                               138 */
    runTest("x_infinite_for", MAIN(
        DECL("i"),
        SET("i", NUM(0)),
        FOR(NULL, NULL, NULL, BLOCK(
            FOR(NULL, NULL, NULL, BLOCK(SET("i", OP('+', VAR("i"), NUM(1))), BREAK())),
            WHILE(OP('<', VAR("i"), NUM(100)), BLOCK(SET("i", OP('*', VAR("i"), NUM(2))))),
            SET("i", OP('+', VAR("i"), NUM(10))),
            BREAK())),
        PRINT(VAR("i")),
        RET(NUM(0))), "138");

    /* while (0) — branch simplification turns the test into GOTO and the
     * body becomes unreachable.  while (1) with a break — the test is
     * deleted outright. */
    runTest("x_const_branch", MAIN(
        DECL("x"),
        SET("x", NUM(5)),
        WHILE(NUM(0), BLOCK(PRINT(NUM(999)), SET("x", NUM(0)))),
        WHILE(OP('<', NUM(1), NUM(2)), BLOCK(PRINT(VAR("x")), BREAK())),
        PRINT(OP('+', VAR("x"), NUM(1))),
        RET(NUM(0))), "5 6");

    /* Regression: a user variable whose name starts with 't' must not be
     * mistaken for a temporary and freed (it used to free t0 while live).
     *   a = 1; total = 2; b = 3; c = 4;
     *   print((a + total) * (b + c));          3 * 7 = 21 */
    runTest("x_t_named_var", MAIN(
        DECL("a"), DECL("total"), DECL("b"), DECL("c"),
        SET("a", NUM(1)), SET("total", NUM(2)), SET("b", NUM(3)), SET("c", NUM(4)),
        WHILE(OP('>', VAR("total"), NUM(0)), BLOCK(
            PRINT(OP('*', OP('+', VAR("a"), VAR("total")), OP('+', VAR("b"), VAR("c")))),
            BREAK())),
        RET(NUM(0))), "21");

    printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}
