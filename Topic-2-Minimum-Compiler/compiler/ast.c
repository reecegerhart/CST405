/* =========================================================================
 *  TOPIC 2 · Compiler for a Starter Language
 * FILE: ast.c   —   Phase 2 — Syntax analysis (the tree it builds)
 * -------------------------------------------------------------------------
 * THE PIPELINE, AND WHERE THIS FILE SITS IN IT
 *   scanner -> parser -> ast -> semantic -> tac -> codegen
 *                        ^^^  this file
 *
 * WHAT IS NEW IN TOPIC 2
 *   • One constructor per node kind, plus a tree printer
 *
 * WHAT COMES NEXT
 *   Topic 3 adds functions, arrays and the rest of arithmetic — and with them, real activation records.
 *
 * YOUR TASK
 *   This is Project 2: the first compiler you build end to end.  Sections
 *   marked  TODO (Topic 2)  are yours.  Everything else — the headers, the
 *   scanner, the driver, the register allocator — is given, because the
 *   point of this project is the six PHASES, not the plumbing between them.
 * ========================================================================= */

/* AST IMPLEMENTATION
 * Functions to create and manipulate Abstract Syntax Tree nodes
 * The AST is built during parsing and used for all subsequent phases
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* External line number from scanner */
extern int yylineno;

/* Create a number literal node */
ASTNode* createNum(int value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_NUM;
    node->lineno = yylineno;
    node->data.num = value;  /* Store the integer value */
    return node;
}

/* Create a variable reference node */
ASTNode* createVar(char* name) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_VAR;
    node->lineno = yylineno;
    node->data.name = strdup(name);  /* Copy the variable name */
    return node;
}

/* --------------------------------------------------------------------
 * TODO (Topic 2) — THE REMAINING AST CONSTRUCTORS    Stili
 * createNum and createVar above are the pattern.  Every constructor does
 * the same four things:
 *
 *     ASTNode* node = malloc(sizeof(ASTNode));
 *     node->type   = NODE_XXX;          <- which kind of node this is
 *     node->lineno = yylineno;          <- where it came from, for errors
 *     ... store the children ...
 *     return node;
 *
 * Write: createBinOp, createDecl, createAssign, createPrint, createStmtList.
 *
 * strdup every char* you store.  The scanner's buffer is reused for the
 * next token, so a name you merely point at will have changed by the time
 * the semantic analyzer reads it.  That bug looks like the AST being
 * randomly corrupted, and it is one of the hardest to find in this course.
 *
 * createStmtList is the one worth thinking about: it links two statements
 * into a list, and the grammar is LEFT recursive, so $1 is the list so far
 * and $2 is the new statement.  Draw the tree for a three-statement
 * program before you write it.
 * -------------------------------------------------------------------- */

/* Create a binary operation node */
ASTNode* createBinOp(char op, ASTNode* left, ASTNode* right) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_BINOP;
    node->lineno = yylineno;
    node->data.binop.op = op;
    node->data.binop.left = left;
    node->data.binop.right = right;
    return node;
}

/* Create a variable declaration node */
ASTNode* createDecl(char* type, char* name) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_DECL;
    node->lineno = yylineno;
    node->data.decl.varType = strdup(type);  /* Copy the type name */
    node->data.decl.name = strdup(name);     /* Copy the variable name */
    return node;
}

/* Create an assignment node */
ASTNode* createAssign(char* var, ASTNode* value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ASSIGN;
    node->lineno = yylineno;
    node->data.assign.var = strdup(var);  /* Copy the target name */
    node->data.assign.value = value;
    return node;
}

/* Create a print node */
ASTNode* createPrint(ASTNode* expr) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_PRINT;
    node->lineno = yylineno;
    node->data.expr = expr;
    return node;
}

/* Create a statement list node.
 * The grammar is left recursive, so stmt1 is the list built so far and
 * stmt2 is the statement being added.  For  s1 s2 s3  the tree leans left:
 *
 *            LIST
 *           /    \
 *        LIST     s3
 *       /    \
 *      s1     s2
 *
 * Visiting `stmt` before `next` therefore walks the statements in source
 * order. */
ASTNode* createStmtList(ASTNode* stmt1, ASTNode* stmt2) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_STMT_LIST;
    node->lineno = yylineno;
    node->data.stmtlist.stmt = stmt1;  /* The list so far */
    node->data.stmtlist.next = stmt2;  /* The new statement */
    return node;
}

/* Text form of an operator code, for the tree printer */
const char* opText(char op) {
    switch (op) {
        case '+': return "+";
        case '-': return "-";
        case '*': return "*";
        case '/': return "/";
        default:  return "?";
    }
}

/* Indent two spaces per nesting level */
static void printIndent(int level) {
    for (int i = 0; i < level; i++) printf("  ");
}


/* Display the AST structure (for debugging and education) */
void printAST(ASTNode* node, int level) {
    /* ----------------------------------------------------------------
     * TODO (Topic 2) — THE TREE PRINTER Reece
     * Print the tree, one node per line, indented two spaces per level.
     *
     *     if (!node) return;
     *     for (int i = 0; i < level; i++) printf("  ");
     *     switch (node->type) { ... one case per node kind ... }
     *
     * Recurse into children with level + 1 so the indentation shows the shape.
     * NODE_STMT_LIST is the exception: print its two children at the SAME
     * level, because a list of statements is a sequence, not a nesting.
     *
     * This function is not decoration.  It is the only window you have into
     * Phase 2, and every bug you hit for the rest of the semester gets
     * diagnosed by staring at its output.  Write it early and make it good.
     * ---------------------------------------------------------------- */
    if (!node) return;
 
    switch (node->type) {
        case NODE_NUM:
            printIndent(level);
            printf("Num: %d\n", node->data.num);
            break;
 
        case NODE_VAR:
            printIndent(level);
            printf("Var: %s\n", node->data.name);
            break;
 
        case NODE_BINOP:
            printIndent(level);
            printf("BinOp: %s\n", opText(node->data.binop.op));
            printAST(node->data.binop.left, level + 1);
            printAST(node->data.binop.right, level + 1);
            break;
 
        case NODE_DECL:
            printIndent(level);
            printf("Decl: %s %s\n", node->data.decl.varType, node->data.decl.name);
            break;
 
        case NODE_ASSIGN:
            printIndent(level);
            printf("Assign: %s\n", node->data.assign.var ? node->data.assign.var : "(null)");
            printAST(node->data.assign.value, level + 1);
            break;
 
        case NODE_PRINT:
            printIndent(level);
            printf("Print\n");
            printAST(node->data.expr, level + 1);
            break;
 
        case NODE_STMT_LIST:
            /* A list is a sequence, not a nesting: no line of its own,
             * and both children stay at the same level. */
            printAST(node->data.stmtlist.stmt, level);
            printAST(node->data.stmtlist.next, level);
            break;
    }
    

}
