/* =========================================================================
 *  TOPIC 3 · Compiling Complex Variables and Functions
 * FILE: ast.c   —   Phase 2 — Syntax analysis (the tree it builds)
 * -------------------------------------------------------------------------
 * THE PIPELINE, AND WHERE THIS FILE SITS IN IT
 *   scanner -> parser -> ast -> semantic -> tac -> codegen
 *                        ^^^  this file
 *
 * WHAT IS NEW IN TOPIC 3
 *   • Constructors and printing for every new node type
 *
 * WHAT COMES NEXT
 *   Topic 4 adds loops (while, for, break) and the label/jump machinery they need.
 *
 * YOUR TASK
 *   Everything below marked  TODO (Topic 3)  is yours to write.
 *   Everything else already works — it is the Topic 2 compiler you
 *   have already built and tested.  Do not rewrite it; extend it.
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

/* Create a binary operation node (for addition) */
ASTNode* createBinOp(char op, ASTNode* left, ASTNode* right) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_BINOP;
    node->lineno = yylineno;
    node->data.binop.op = op;        /* Store operator (+) */
    node->data.binop.left = left;    /* Left subtree */
    node->data.binop.right = right;  /* Right subtree */
    return node;
}

/* Create a variable declaration node */
ASTNode* createDecl(char* type, char* name) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_DECL;
    node->lineno = yylineno;
    node->data.decl.name = strdup(name);
    node->data.decl.varType = strdup(type);
    return node;
}

/* Create an assignment statement node */
ASTNode* createAssign(char* var, ASTNode* value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ASSIGN;
    node->lineno = yylineno;
    node->data.assign.var = var ? strdup(var) : NULL;  /* Variable name (NULL for array assignments) */
    node->data.assign.value = value;      /* Expression tree */
    /* No array subscript on the left unless the parser says so */
    node->data.assign.arrayLHS = NULL;
    return node;
}

/* Create a print statement node */
ASTNode* createPrint(ASTNode* expr) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_PRINT;
    node->lineno = yylineno;
    node->data.expr = expr;  /* Expression to print */
    return node;
}

/* Create a statement list node (links statements together) */
ASTNode* createStmtList(ASTNode* stmt1, ASTNode* stmt2) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_STMT_LIST;
    node->lineno = stmt1 ? stmt1->lineno : (stmt2 ? stmt2->lineno : yylineno);
    node->data.stmtlist.stmt = stmt1;  /* First statement */
    node->data.stmtlist.next = stmt2;  /* Rest of list */
    return node;
}

/* --------------------------------------------------------------
 * TODO (Topic 3)
 * Constructors for the function-related nodes: createFuncDef,
 * createParam, createParamList, createFuncCall, createArgList,
 * createReturn.  They are all the same three lines — malloc, set
 * the type and lineno, store the children — which is the point:
 * the AST is deliberately boring so the phases above it can be
 * interesting.
 * -------------------------------------------------------------- */
ASTNode* createFuncDef(char* name, ASTNode* params, ASTNode* body) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_FUNC_DEF;
    node->lineno = yylineno;
    node->data.func_def.name = strdup(name);
    node->data.func_def.params = params;
    node->data.func_def.body = body;
    return node;
}

ASTNode* createParam(char* name) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_PARAM;
    node->lineno = yylineno;
    node->data.param.name = strdup(name);
    return node;
}

/* Append `param` to the end of `list` (NULL starts a new list); returns the head. */
ASTNode* createParamList(ASTNode* list, ASTNode* param) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_PARAM_LIST;
    node->lineno = param ? param->lineno : yylineno;
    node->data.param_list.param = param;
    node->data.param_list.next = NULL;
    if (!list) return node;
    ASTNode* tail = list;
    while (tail->data.param_list.next) tail = tail->data.param_list.next;
    tail->data.param_list.next = node;
    return list;
}

ASTNode* createFuncCall(char* name, ASTNode* args) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_FUNC_CALL;
    node->lineno = yylineno;
    node->data.func_call.name = strdup(name);
    node->data.func_call.args = args;
    return node;
}

/* Append `expr` to the end of `list` (NULL starts a new list); returns the head. */
ASTNode* createArgList(ASTNode* list, ASTNode* expr) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ARG_LIST;
    node->lineno = expr ? expr->lineno : yylineno;
    node->data.arg_list.expr = expr;
    node->data.arg_list.next = NULL;
    if (!list) return node;
    ASTNode* tail = list;
    while (tail->data.arg_list.next) tail = tail->data.arg_list.next;
    tail->data.arg_list.next = node;
    return list;
}

ASTNode* createReturn(ASTNode* expr) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_RETURN;
    node->lineno = yylineno;
    node->data.ret.expr = expr;
    return node;
}
/* --------------------------------------------------------------
 * TODO (Topic 3)
 * createBlock, createArrayDecl, createArrayParam and
 * createArrayIndex.  Give an array PARAMETER size 0 and isParam 1;
 * give a declared array its real size and isParam 0.  Code
 * generation later depends on telling those two apart.
 * -------------------------------------------------------------- */
ASTNode* createBlock(ASTNode* stmt_list) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_BLOCK;
    node->lineno = yylineno;
    node->data.block.stmt_list = stmt_list;
    return node;
}

ASTNode* createArrayDecl(char* name, int size) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ARRAY_DECL;
    node->lineno = yylineno;
    node->data.array_decl.name = strdup(name);
    node->data.array_decl.size = size;
    node->data.array_decl.isParam = 0;
    return node;
}

ASTNode* createArrayParam(char* name) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ARRAY_DECL;
    node->lineno = yylineno;
    node->data.array_decl.name = strdup(name);
    node->data.array_decl.size = 0;
    node->data.array_decl.isParam = 1;
    return node;
}
/* --------------------------------------------------------------
 * TODO (Topic 3)
 * createArrayIndex: build the node for  arr[i].  It stores the array
 * NAME and the index EXPRESSION — note the name is not itself an
 * expression node, which is a simplification worth mentioning in your
 * write-up (it is what stops this language having pointers).
 * -------------------------------------------------------------- */
ASTNode* createArrayIndex(char* name, ASTNode* index) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ARRAY_INDEX;
    node->lineno = yylineno;
    node->data.array_index.name = strdup(name);
    node->data.array_index.index = index;
    return node;
}

/* Render an operator code as the text the programmer actually wrote.
 * The parser packs multi-character operators into single chars so that a
 * BINOP node stays small; this is where that encoding is undone. */
const char* opText(char op) {
    switch (op) {
        case '+': return "+";   case '-': return "-";
        case '*': return "*";   case '/': return "/";
        case '<': return "<";   case '>': return ">";
        case 'l': return "<=";  case 'g': return ">=";
        case 'e': return "==";  case 'n': return "!=";
        case 'u': return "unary -";
        case '&': return "&&";  case '|': return "||";  case '!': return "!";
        default:  return "?";
    }
}

/* Display the AST structure (for debugging and education) */
void printAST(ASTNode* node, int level) {
    if (!node) return;

    /* List nodes are not drawn themselves: hand their items straight on at the
     * same level so no stray indentation is printed before each item. */
    if (node->type == NODE_STMT_LIST) {
        printAST(node->data.stmtlist.stmt, level);
        printAST(node->data.stmtlist.next, level);
        return;
    }
    if (node->type == NODE_PARAM_LIST) {
        printAST(node->data.param_list.param, level);
        printAST(node->data.param_list.next, level);
        return;
    }
    if (node->type == NODE_ARG_LIST) {
        printAST(node->data.arg_list.expr, level);
        printAST(node->data.arg_list.next, level);
        return;
    }

    /* Indent based on tree depth */
    for (int i = 0; i < level; i++) printf("  ");

    /* Print node based on its type */
    switch(node->type) {
        case NODE_NUM:
            printf("NUM: %d\n", node->data.num);
            break;
        case NODE_VAR:
            printf("VAR: %s\n", node->data.name);
            break;
        case NODE_BINOP:
            printf("BINOP: %s\n", opText(node->data.binop.op));
            printAST(node->data.binop.left, level + 1);
            printAST(node->data.binop.right, level + 1);
            break;
        case NODE_DECL:
            printf("DECL: %s %s\n", node->data.decl.varType, node->data.decl.name);
            break;
        case NODE_ASSIGN:
            if (node->data.assign.var) printf("ASSIGN: %s\n", node->data.assign.var);
/* --------------------------------------------------------------
 * TODO (Topic 3)
 * An assignment now has two shapes: to a variable, or to an array
 * element.  Print them differently — a tree dump that hides the
 * difference is a tree dump you cannot debug with.
 * -------------------------------------------------------------- */
            else {
                printf("ASSIGN (array element):\n");
                printAST(node->data.assign.arrayLHS, level + 1);
            }
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("VALUE:\n");
            printAST(node->data.assign.value, level + 2);
            break;
        case NODE_PRINT:
            printf("PRINT\n");
            printAST(node->data.expr, level + 1);
            break;
        case NODE_STMT_LIST:
            /* Print statements in sequence at same level */
            printAST(node->data.stmtlist.stmt, level);
            printAST(node->data.stmtlist.next, level);
            break;
/* --------------------------------------------------------------
 * TODO (Topic 3)
 * Print the function-related nodes.  Indent children one level
 * deeper so the printed tree really looks like a tree.
 * -------------------------------------------------------------- */
        case NODE_FUNC_DEF:
            printf("FUNC_DEF: %s\n", node->data.func_def.name);
            printAST(node->data.func_def.params, level + 1);
            printAST(node->data.func_def.body, level + 1);
            break;
        case NODE_PARAM:
            printf("PARAM: %s\n", node->data.param.name);
            break;
        case NODE_PARAM_LIST:
            /* handled before the indent above */
            break;
        case NODE_FUNC_CALL:
            printf("FUNC_CALL: %s\n", node->data.func_call.name);
            printAST(node->data.func_call.args, level + 1);
            break;
        case NODE_ARG_LIST:
            /* handled before the indent above */
            break;
        case NODE_RETURN:
            printf("RETURN\n");
            printAST(node->data.ret.expr, level + 1);
            break;
/* --------------------------------------------------------------
 * TODO (Topic 3)
 * Print NODE_BLOCK, NODE_ARRAY_DECL and NODE_ARRAY_INDEX.
 * -------------------------------------------------------------- */
        case NODE_BLOCK:
            printf("BLOCK\n");
            printAST(node->data.block.stmt_list, level + 1);
            break;
        case NODE_ARRAY_DECL:
            if (node->data.array_decl.isParam)
                printf("ARRAY_PARAM: %s[]\n", node->data.array_decl.name);
            else
                printf("ARRAY_DECL: %s[%d]\n", node->data.array_decl.name, node->data.array_decl.size);
            break;
        case NODE_ARRAY_INDEX:
            printf("ARRAY_INDEX: %s\n", node->data.array_index.name);
            printAST(node->data.array_index.index, level + 1);
            break;
    }
}