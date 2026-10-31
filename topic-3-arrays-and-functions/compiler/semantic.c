/* =========================================================================
 *  TOPIC 3 · Compiling Complex Variables and Functions
 * FILE: semantic.c   —   Phase 3 — Semantic analysis
 * -------------------------------------------------------------------------
 * THE PIPELINE, AND WHERE THIS FILE SITS IN IT
 *   scanner -> parser -> ast -> semantic -> tac -> codegen
 *                               ^^^^^^^^  this file
 *
 * WHAT IS NEW IN TOPIC 3
 *   • A scope STACK replaces the single flat scope
 *   • Function signatures are collected first, so calls may appear before definitions
 *   • Argument count is checked against the declaration
 *
 * WHAT COMES NEXT
 *   Topic 4 adds loops (while, for, break) and the label/jump machinery they need.
 *
 * YOUR TASK
 *   Everything below marked  TODO (Topic 3)  is yours to write.
 *   Everything else already works — it is the Topic 2 compiler you
 *   have already built and tested.  Do not rewrite it; extend it.
 * ========================================================================= */

/* SEMANTIC ANALYSIS IMPLEMENTATION - WITH FUNCTION SUPPORT
 * Performs semantic checks on the Abstract Syntax Tree
 * Now supports functions, scopes, parameters, control flow
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"
#include "symtab.h"
#include "trace.h"

#define MAX_FUNCTIONS 100
#define MAX_PARAMS 20
#define MAX_SCOPE_DEPTH 10

/* --------------------------------------------------------------
 * TODO (Topic 3)
 * A second table, for FUNCTIONS.  Variables and functions live in separate
 * namespaces, which is why a program may declare both `int total;` and
 * `int total(...)`.  Record just enough to check a call: the name, the
 * parameter count, and which parameters are arrays.
 * -------------------------------------------------------------- */
typedef struct {
    char name[64];
    int paramCount;
    int isArray[MAX_PARAMS];   /* 1 if that parameter is an array (int a[]) */
} FuncInfo;

static FuncInfo funcTable[MAX_FUNCTIONS];
static int funcCount = 0;
static int inFunction = 0;     /* 1 while a function body is being checked */

static FuncInfo* findFunction(const char* name);
static int addFunction(const char* name, int paramCount, ASTNode* params);

/* Scope for variables */
typedef struct {
    char* names[MAX_VARS];
    int count;
} Scope;

/* Global semantic information */
static SemanticInfo semInfo;
static Scope scopes[MAX_SCOPE_DEPTH];
static int scopeDepth = 0;

/* Initialize semantic analyzer */
void initSemantic() {
    semInfo.errorCount = 0;
    semInfo.warningCount = 0;
    scopeDepth = 0;
/* --------------------------------------------------------------
 * TODO (Topic 3)
 * Reset the function table too, and seed it with the one function the
 * language provides for free: `print`, which takes one argument.
 * Registering it here is what makes `print(x)` type-check like any
 * other call instead of needing a special case.
 * -------------------------------------------------------------- */
    funcCount = 0;
    inFunction = 0;
    addFunction("print", 1, NULL);

    trace("SEMANTIC ANALYZER: initialized\n\n");
}

/* Scope management */
static void enterScope() {
    if (scopeDepth >= MAX_SCOPE_DEPTH) {
        fprintf(stderr, "SEMANTIC ERROR: Maximum scope depth exceeded\n");
        semInfo.errorCount++;
        return;
    }
    scopes[scopeDepth].count = 0;
    scopeDepth++;
}

static void exitScope() {
    if (scopeDepth > 0) {
        /* Free variable names in this scope */
        for (int i = 0; i < scopes[scopeDepth - 1].count; i++) {
            free(scopes[scopeDepth - 1].names[i]);
        }
        scopeDepth--;
    }
}

/* Print current semantic scopes for debugging */
static void printSemanticScopes() {
    trace("\n┌─────────────────────────────────────────────────────────┐\n");
    trace("│ SEMANTIC SCOPE STACK (Depth: %d)                        \n", scopeDepth);
    trace("├─────────────────────────────────────────────────────────┤\n");

    if (scopeDepth == 0) {
        trace("│ (no active scopes)                                      │\n");
    } else {
        for (int depth = 0; depth < scopeDepth; depth++) {
            if (depth == 0) {
                trace("│ Scope[%d] GLOBAL (%d variables)                        \n", depth, scopes[depth].count);
            } else {
                trace("│ Scope[%d] LOCAL (%d variables)                         \n", depth, scopes[depth].count);
/* --------------------------------------------------------------
 * TODO (Topic 3)
 * Implement this part of the milestone.
 * -------------------------------------------------------------- */
            }

            if (scopes[depth].count > 0) {
                trace("│   Variables: ");
                for (int i = 0; i < scopes[depth].count; i++) {
                    trace("%s", scopes[depth].names[i]);
                    if (i < scopes[depth].count - 1) trace(", ");
                }
                trace("\n");
            } else {
                trace("│   (empty)\n");
            }
        }
    }
    trace("└─────────────────────────────────────────────────────────┘\n\n");
}

/* Add variable to current scope */
/* RESERVED IDENTIFIERS
 * The intermediate-code generator invents names of its own: temporaries
 * t0, t1, ... and labels L0, L1, ...  If a user variable had one of those
 * shapes the back end could not tell them apart, so the language reserves
 * that namespace.  Reporting it here — in the semantic analyzer, with a line
 * number — is far kinder than a mysterious wrong answer at run time. */
static int isReservedName(const char* name) {
    if (!name || !name[0]) return 0;
    if (strncmp(name, "__sw", 4) == 0) return 1;          /* switch temporaries */
    if (name[0] != 't' && name[0] != 'L') return 0;
    if (!name[1]) return 0;
    for (int i = 1; name[i]; i++)
        if (name[i] < '0' || name[i] > '9') return 0;
    return 1;
}

static void reportReserved(const char* name, int lineno) {
    fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
    fprintf(stderr, "║ SEMANTIC ERROR - Reserved Identifier                      ║\n");
    fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
    fprintf(stderr, "  📍 Location: Line %d\n", lineno);
    fprintf(stderr, "  ❌ Error: '%s' is reserved for the compiler's own use\n", name);
    fprintf(stderr, "  📖 Note: names of the form t0, t1, ... are three-address-code\n");
    fprintf(stderr, "           temporaries and L0, L1, ... are generated labels.\n");
    fprintf(stderr, "  💡 Suggestion: rename the variable, for example '%s_' or 'total'\n\n", name);
}

static int addVarToScope(char* name) {
    if (scopeDepth == 0) {
        fprintf(stderr, "SEMANTIC ERROR: No scope to add variable to\n");
        return -1;
    }

    Scope* currentScope = &scopes[scopeDepth - 1];

    /* Check if already declared in current scope */
    for (int i = 0; i < currentScope->count; i++) {
        if (strcmp(currentScope->names[i], name) == 0) {
            return -1;  /* Already declared in this scope */
        }
    }

    /* Add to current scope */
    if (currentScope->count >= MAX_VARS) {
        fprintf(stderr, "SEMANTIC ERROR: Too many variables in scope\n");
        return -1;
    }

    currentScope->names[currentScope->count] = strdup(name);
    currentScope->count++;
    return 0;
}

/* --------------------------------------------------------------
 * TODO (Topic 3)
 * Arrays need their own scope-insertion helper — not because the storage
 * question is different (symtab.c settles that later) but because the
 * error messages are.
 * -------------------------------------------------------------- */
/* Report one semantic error in the same boxed format as the rest of the file,
 * and count it.  Checking continues afterwards so several errors can be
 * reported in one run. */
static void semError(int lineno, const char* title, const char* error, const char* suggestion) {
    fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
    fprintf(stderr, "║ SEMANTIC ERROR - %s\n", title);
    fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
    fprintf(stderr, "  📍 Location: Line %d\n", lineno);
    fprintf(stderr, "  ❌ Error: %s\n", error);
    fprintf(stderr, "  💡 Suggestion: %s\n\n", suggestion);
    semInfo.errorCount++;
}

/* Add an array (declared or parameter) to the current scope.
 * Returns 0 on success, -1 after reporting an error. */
static int addArrayToScope(char* name, int lineno) {
    char err[256], sug[256];

    if (isReservedName(name)) {
        reportReserved(name, lineno);
        semInfo.errorCount++;
        return -1;
    }
    if (addVarToScope(name) == -1) {
        snprintf(err, sizeof err, "Array '%s' is already declared in the current scope", name);
        snprintf(sug, sizeof sug, "Rename the array (for example '%s2') or remove the duplicate declaration", name);
        semError(lineno, "Duplicate Array Declaration", err, sug);
        return -1;
    }
    trace("  ✓ Array '%s' declared (line %d)\n", name, lineno);
    return 0;
}

/* Check if variable is declared in any visible scope */
static int isVarDeclaredInScope(char* name) {
    /* Search from innermost to outermost scope */
    for (int depth = scopeDepth - 1; depth >= 0; depth--) {
        for (int i = 0; i < scopes[depth].count; i++) {
            if (strcmp(scopes[depth].names[i], name) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

/* --------------------------------------------------------------
 * TODO (Topic 3)
 * findFunction / addFunction / countParams / countArgs.
 * countParams walks the parameter list parser.y built; countArgs walks
 * the argument list at a call site.  Comparing those two numbers is the
 * whole of call checking.
 * -------------------------------------------------------------- */
static FuncInfo* findFunction(const char* name) {
    for (int i = 0; i < funcCount; i++) {
        if (strcmp(funcTable[i].name, name) == 0) return &funcTable[i];
    }
    return NULL;
}

/* Returns 0 on success, -1 if the name is already taken, -2 if a limit is hit. */
static int addFunction(const char* name, int paramCount, ASTNode* params) {
    if (findFunction(name)) return -1;
    if (funcCount >= MAX_FUNCTIONS || paramCount > MAX_PARAMS) return -2;

    FuncInfo* f = &funcTable[funcCount++];
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->name[sizeof(f->name) - 1] = '\0';
    f->paramCount = paramCount;
    for (int i = 0; i < MAX_PARAMS; i++) f->isArray[i] = 0;

    int i = 0;
    for (ASTNode* p = params; p && i < paramCount; p = p->data.param_list.next, i++) {
        ASTNode* prm = p->data.param_list.param;
        f->isArray[i] = (prm && prm->type == NODE_ARRAY_DECL);
    }
    return 0;
}

static int countParams(ASTNode* params) {
    int n = 0;
    for (ASTNode* p = params; p; p = p->data.param_list.next) n++;
    return n;
}

static int countArgs(ASTNode* args) {
    int n = 0;
    for (ASTNode* a = args; a; a = a->data.arg_list.next) n++;
    return n;
}

/* Forward declaration: checkStmt and checkStmtList are mutually recursive,
 * which is exactly what you want when the thing being checked is a tree. */
static void checkStmtList(ASTNode* node);

/* Check expression for semantic correctness */
static void checkExpr(ASTNode* node) {
    if (!node) return;

    switch(node->type) {
        case NODE_NUM:
            /* Literal numbers are always valid */
            break;

        case NODE_VAR:
            if (!isVarDeclaredInScope(node->data.name)) {
                fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                fprintf(stderr, "║ SEMANTIC ERROR - Undeclared Variable                      ║\n");
                fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                fprintf(stderr, "  ❌ Error: Variable '%s' is used before being declared\n", node->data.name);
                fprintf(stderr, "  💡 Suggestion: Add a declaration before using this variable:\n");
                fprintf(stderr, "     → int %s;  (add this before line %d)\n", node->data.name, node->lineno);
                fprintf(stderr, "  📖 Note: In C-minus, all variables must be declared before use\n\n");
                semInfo.errorCount++;
            }
            break;

        case NODE_BINOP:
            checkExpr(node->data.binop.left);
            if (node->data.binop.right) {  /* Unary minus has no right operand */
                checkExpr(node->data.binop.right);
            }
            break;

/* --------------------------------------------------------------
 * TODO (Topic 3)
 * Check a NODE_FUNC_CALL: does the function exist, and does the call
 * supply the right number of arguments?  Then check each argument
 * expression.  A wrong argument count is the single most common mistake
 * a compiler can catch for the programmer here.
 * -------------------------------------------------------------- */
        case NODE_FUNC_CALL: {
            char err[256], sug[256];
            FuncInfo* f = findFunction(node->data.func_call.name);
            if (!f) {
                snprintf(err, sizeof err, "Function '%s' is called but never defined", node->data.func_call.name);
                snprintf(sug, sizeof sug, "Define 'int %s(...) { ... }' somewhere in the file, or check the spelling", node->data.func_call.name);
                semError(node->lineno, "Undefined Function", err, sug);
            } else {
                int given = countArgs(node->data.func_call.args);
                if (given != f->paramCount) {
                    snprintf(err, sizeof err, "Function '%s' expects %d argument(s) but the call supplies %d",
                             f->name, f->paramCount, given);
                    snprintf(sug, sizeof sug, "Call '%s' with exactly %d argument(s)", f->name, f->paramCount);
                    semError(node->lineno, "Wrong Number of Arguments", err, sug);
                }
            }
            for (ASTNode* a = node->data.func_call.args; a; a = a->data.arg_list.next) {
                checkExpr(a->data.arg_list.expr);
            }
            break;
        }
/* --------------------------------------------------------------
 * TODO (Topic 3)
 * Check a NODE_ARRAY_INDEX: the array must be declared, and the index is
 * an expression that must itself check out.  Note what we do NOT check —
 * whether the index is in range.  That is not knowable at compile time in
 * general, and this language has no run-time check either.  Say so in
 * your write-up; it is a real design decision, not an oversight.
 * -------------------------------------------------------------- */
        case NODE_ARRAY_INDEX:
            if (!isVarDeclaredInScope(node->data.array_index.name)) {
                char err[256], sug[256];
                snprintf(err, sizeof err, "Array '%s' is used before being declared", node->data.array_index.name);
                snprintf(sug, sizeof sug, "Declare it first, for example 'int %s[10];'", node->data.array_index.name);
                semError(node->lineno, "Undeclared Array", err, sug);
            }
            checkExpr(node->data.array_index.index);
            break;
        default:
            break;
    }
}

/* Check statement */
static void checkStmt(ASTNode* node) {
    if (!node) return;

    switch(node->type) {
        case NODE_DECL:
            if (isReservedName(node->data.decl.name)) {
                reportReserved(node->data.decl.name, node->lineno);
                semInfo.errorCount++;
                break;
            }
            if (addVarToScope(node->data.decl.name) == -1) {
                fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                fprintf(stderr, "║ SEMANTIC ERROR - Duplicate Variable Declaration           ║\n");
                fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                fprintf(stderr, "  ❌ Error: Variable '%s' is already declared in the current scope\n", node->data.decl.name);
                fprintf(stderr, "  💡 Suggestion: Choose a different variable name or remove the duplicate declaration\n");
                fprintf(stderr, "  📖 Note: Each variable can only be declared once per scope\n");
                fprintf(stderr, "     → Use assignment (=) to change the value of an existing variable\n");
                fprintf(stderr, "     → Or use a different name: %s2, my%s, etc.\n\n", node->data.decl.name, node->data.decl.name);
                semInfo.errorCount++;
            } else {
                trace("  ✓ Variable '%s' declared (line %d)\n", node->data.decl.name, node->lineno);
            }
            break;

        case NODE_ASSIGN:
                if (node->data.assign.var && !isVarDeclaredInScope(node->data.assign.var)) {
                    fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                    fprintf(stderr, "║ SEMANTIC ERROR - Assignment to Undeclared Variable        ║\n");
                    fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                    fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                    fprintf(stderr, "  ❌ Error: Trying to assign to variable '%s' which hasn't been declared\n", node->data.assign.var);
                    fprintf(stderr, "  💡 Suggestion: Declare the variable before assigning to it:\n");
                    fprintf(stderr, "     → Add this before line %d:\n", node->lineno);
                    fprintf(stderr, "       int %s;\n", node->data.assign.var);
                    fprintf(stderr, "  📖 Remember: Variables must be declared before use in C-minus\n\n");
                    semInfo.errorCount++;
                } else if (node->data.assign.var) {
                    trace("  ✓ Assignment to '%s' is valid (line %d)\n", node->data.assign.var, node->lineno);
                }
/* --------------------------------------------------------------
 * TODO (Topic 3)
 * The target of an assignment may now be an array ELEMENT.  Check
 * the index expression in that case, and the variable name in the
 * other.
 * -------------------------------------------------------------- */
            /* Array element target: checkExpr on the NODE_ARRAY_INDEX verifies
             * the array is declared and checks the index expression. */
            if (node->data.assign.arrayLHS) {
                checkExpr(node->data.assign.arrayLHS);
            }
            checkExpr(node->data.assign.value);
            break;

        case NODE_PRINT:
            checkExpr(node->data.expr);
            trace("  ✓ Print statement is valid\n");
            break;

/* --------------------------------------------------------------
 * TODO (Topic 3)
 * Check a NODE_RETURN.  The interesting case is `return` OUTSIDE any
 * function — legal to parse, meaningless to translate.  That gap between
 * "parses" and "means something" is the territory this phase owns.
 * -------------------------------------------------------------- */
        case NODE_RETURN:
            if (!inFunction) {
                semError(node->lineno, "Return Outside Function",
                         "'return' can only appear inside a function body",
                         "Move this statement into a function, or remove it");
            }
            checkExpr(node->data.ret.expr);
            break;

/* --------------------------------------------------------------
 * TODO (Topic 3)
 * NODE_BLOCK opens a scope and closes it again.  Two lines of code, and
 * they are what make this legal:
 *     int x;  { int x; }      inner x shadows outer x
 * and this an error:
 *     { int x; int x; }       same name twice in one scope
 * Handle NODE_FUNC_CALL used as a statement and NODE_STMT_LIST too.
 * -------------------------------------------------------------- */
        case NODE_BLOCK:
            enterScope();
            checkStmtList(node->data.block.stmt_list);
            exitScope();
            break;

        case NODE_FUNC_CALL:
            checkExpr(node);
            break;

        case NODE_STMT_LIST:
            /* The grammar is left-recursive, so a statement list can contain
             * another statement list.  Handling that here (not only in
             * checkStmtList) is what lets the recursion bottom out. */
            checkStmtList(node);
            break;

/* --------------------------------------------------------------
 * TODO (Topic 3)
 * Check a NODE_ARRAY_DECL.  Two forms to tell apart: a real declaration
 * (which needs a positive size) and a parameter (which has no size at
 * all).  Reject `int a[0];` and `int a[-3];` here — nothing downstream
 * will.
 * -------------------------------------------------------------- */
        case NODE_ARRAY_DECL:
            if (node->data.array_decl.isParam) {
                /* Parameters have no size to check; addParamsToScope inserts them. */
                break;
            }
            if (node->data.array_decl.size <= 0) {
                char err[256], sug[256];
                snprintf(err, sizeof err, "Array '%s' is declared with size %d",
                         node->data.array_decl.name, node->data.array_decl.size);
                snprintf(sug, sizeof sug, "Use a positive size, for example 'int %s[10];'", node->data.array_decl.name);
                semError(node->lineno, "Invalid Array Size", err, sug);
            }
            /* Still add it to scope so later uses do not cascade into
             * "undeclared" errors. */
            addArrayToScope(node->data.array_decl.name, node->lineno);
            break;
        default:
            break;
    }
}

/* Check statement list */
static void checkStmtList(ASTNode* node) {
    if (!node) return;

    if (node->type == NODE_STMT_LIST) {
        checkStmt(node->data.stmtlist.stmt);
        checkStmtList(node->data.stmtlist.next);
    } else {
        checkStmt(node);
    }
}

/* --------------------------------------------------------------
 * TODO (Topic 3)
 * addParamsToScope, checkFuncDef, registerFunction, registerFunctions
 * and checkFunctions — the TWO-PASS structure.
 *
 * Pass 1 records every function signature.  Pass 2 checks the bodies.
 * Two passes and not one, so that a function may call another defined
 * LATER in the file.  Collapse them into one pass and see which programs
 * stop compiling; that experiment is worth five minutes.
 * -------------------------------------------------------------- */
/* Put each parameter into the current (function) scope. */
static void addParamsToScope(ASTNode* params) {
    for (ASTNode* p = params; p; p = p->data.param_list.next) {
        ASTNode* prm = p->data.param_list.param;
        if (!prm) continue;

        if (prm->type == NODE_ARRAY_DECL) {
            addArrayToScope(prm->data.array_decl.name, prm->lineno);
        } else if (prm->type == NODE_PARAM) {
            char* name = prm->data.param.name;
            if (isReservedName(name)) {
                reportReserved(name, prm->lineno);
                semInfo.errorCount++;
            } else if (addVarToScope(name) == -1) {
                char err[256], sug[256];
                snprintf(err, sizeof err, "Parameter '%s' appears more than once in the parameter list", name);
                snprintf(sug, sizeof sug, "Give each parameter a different name");
                semError(prm->lineno, "Duplicate Parameter", err, sug);
            }
        }
    }
}

/* Pass 2, one function: parameters and body share ONE scope, so a local that
 * reuses a parameter's name is a duplicate (as in C), not a shadow. */
static void checkFuncDef(ASTNode* node) {
    trace("Checking function '%s' (line %d)\n", node->data.func_def.name, node->lineno);
    enterScope();
    addParamsToScope(node->data.func_def.params);

    inFunction = 1;
    ASTNode* body = node->data.func_def.body;
    if (body && body->type == NODE_BLOCK) {
        checkStmtList(body->data.block.stmt_list);
    }
    inFunction = 0;

    exitScope();
}

/* Pass 1, one function: record its signature. */
static void registerFunction(ASTNode* node) {
    char err[256], sug[256];
    int count = countParams(node->data.func_def.params);
    int rc = addFunction(node->data.func_def.name, count, node->data.func_def.params);

    if (rc == -1) {
        snprintf(err, sizeof err, "Function '%s' is already defined", node->data.func_def.name);
        snprintf(sug, sizeof sug, "Rename one of the functions or remove the duplicate definition");
        semError(node->lineno, "Duplicate Function Definition", err, sug);
    } else if (rc == -2) {
        snprintf(err, sizeof err, "Function '%s' exceeds a compiler limit (at most %d functions and %d parameters)",
                 node->data.func_def.name, MAX_FUNCTIONS, MAX_PARAMS);
        snprintf(sug, sizeof sug, "Reduce the number of functions or parameters");
        semError(node->lineno, "Compiler Limit Exceeded", err, sug);
    } else {
        trace("  ✓ Function '%s' registered with %d parameter(s) (line %d)\n",
              node->data.func_def.name, count, node->lineno);
    }
}

/* Pass 1: walk the top-level list and register every function. */
static void registerFunctions(ASTNode* node) {
    if (!node) return;
    if (node->type == NODE_STMT_LIST) {
        registerFunctions(node->data.stmtlist.stmt);
        registerFunctions(node->data.stmtlist.next);
    } else if (node->type == NODE_FUNC_DEF) {
        registerFunction(node);
    }
}

/* Pass 2: check every function body, and every global declaration in order. */
static void checkFunctions(ASTNode* node) {
    if (!node) return;
    if (node->type == NODE_STMT_LIST) {
        checkFunctions(node->data.stmtlist.stmt);
        checkFunctions(node->data.stmtlist.next);
    } else if (node->type == NODE_FUNC_DEF) {
        checkFuncDef(node);
    } else {
        checkStmt(node);   /* global declaration */
    }
}

int performSemanticAnalysis(ASTNode* root) {
    if (!root) {
        fprintf(stderr, "SEMANTIC ERROR: No AST to analyze\n");
        return -1;
    }

    trace("Running semantic analysis with function support...\n\n");

    /* Enter global scope */
    enterScope();
    trace("Entered global scope\n");
    printSemanticScopes();

    /* The single walk over the statement list was replaced by the two passes below. */
/* --------------------------------------------------------------
 * TODO (Topic 3)
 * Replace the single walk with TWO passes:
 *   pass 1: registerFunctions(root) — record every signature
 *   pass 2: checkFunctions(root)    — check every body
 * -------------------------------------------------------------- */
    registerFunctions(root);
    checkFunctions(root);

    /* Exit global scope */
    exitScope();

    return semInfo.errorCount > 0 ? -1 : 0;
}

/* Print semantic analysis summary */
void printSemanticSummary() {
    trace("═══════════════════════════════════════════\n");
    trace("SEMANTIC ANALYSIS SUMMARY\n");
    trace("═══════════════════════════════════════════\n");
    trace("Errors found:       %d\n", semInfo.errorCount);
    trace("Warnings found:     %d\n", semInfo.warningCount);
    trace("\n");

    if (semInfo.errorCount == 0) {
        trace("✓ Semantic analysis passed - program is semantically correct!\n");
    } else {
        trace("✗ Semantic analysis failed - fix errors before proceeding\n");
    }
    trace("═══════════════════════════════════════════\n\n");
}