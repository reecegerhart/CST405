/* =========================================================================
 * CST-405  ·  TOPIC 4  ·  Compiling Loops
 * FILE: parser.y   —   Phase 2 — Syntax analysis
 * -------------------------------------------------------------------------
 * THE PIPELINE, AND WHERE THIS FILE SITS IN IT
 *   scanner -> parser -> ast -> semantic -> tac -> codegen
 *              ^^^^^^  this file
 *
 * WHAT IS NEW IN TOPIC 4
 *   • Relational operators, slotted BELOW + - * / in the precedence table
 *   • `while (cond) stmt` and `for (init; cond; update) stmt`
 *   • `break;` to leave a loop early
 *
 * WHAT COMES NEXT
 *   Topic 5 adds decisions (if, if-else, switch) and the logical operators.
 *
 * YOUR TASK
 *   Everything below marked  TODO (Topic 4)  is yours to write.
 *   Everything else already works — it is the Topic 3 compiler you
 *   have already built and tested.  Do not rewrite it; extend it.
 * ========================================================================= */

%{
/* PHASE 2 — SYNTAX ANALYSIS
 *
 * The parser answers one question: do these tokens form a legal program?
 * Bison builds an LALR(1) bottom-up parser from the grammar below.  It reads
 * tokens left to right, shifts them onto a stack, and REDUCES whenever the
 * top of the stack matches the right-hand side of a rule.
 *
 * Answering "yes" is not enough, though — the rest of the compiler needs the
 * program's STRUCTURE.  So each rule carries a semantic action in { } that
 * builds one node of the abstract syntax tree.  $1, $2, ... are the values of
 * the symbols on the right-hand side; $$ is the value this rule hands back.
 *
 *      expr '+' expr   { $$ = createBinOp('+', $1, $3); }
 *       │        │                              │   └── right operand
 *       │        └── $3                         └────── left operand
 *       └── $1
 *
 * By the time yyparse() returns, the tree has been built bottom-up beneath us
 * and `root` points at the whole program.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* External declarations for lexer interface */
extern int yylex();
extern int yyparse();
extern FILE* yyin;
extern int yylineno;  /* Line number from scanner */

void yyerror(const char* s);
ASTNode* root = NULL;
%}

/* SEMANTIC VALUES UNION */
%union {
    int num;
    char* str;
    struct ASTNode* node;
}

/* TOKEN DECLARATIONS */
%token <num> NUM
%token <str> ID
%token INT PRINT
%token RETURN
/* Loop keyword (Topic 4) */
%token FOR
/* --------------------------------------------------------------
 * TODO (Topic 4)
 * Declare the remaining loop keywords: WHILE BREAK.
 * -------------------------------------------------------------- */
/* Two-character relational operators (Topic 4).  The single-character
 * ones, < and >, arrive as character literals and need no %token. */
%token LE GE EQ NE

/* NON-TERMINAL TYPES */
%type <node> program stmt_list stmt decl assign expr print_stmt
%type <node> decl_or_func_list decl_or_func func_def params param_list param
%type <node> return_stmt block func_call arg_list args
/* For-loop non-terminals (Topic 4) */
%type <node> for_stmt for_init for_cond for_update
/* --------------------------------------------------------------
 * TODO (Topic 4)
 * Every new non-terminal needs its type declared here, or bison will
 * not know that $$ is an ASTNode*.  Still to add: while_stmt break_stmt
 * -------------------------------------------------------------- */

/* OPERATOR PRECEDENCE AND ASSOCIATIVITY
 * Listed from lowest to highest precedence.
 *
 * DANGLING-ELSE RESOLUTION:
 *   The grammar has an ambiguity: in  if (c) if (c) s  else s
 *   the "else" can bind to either "if".  The standard resolution
 *   is that else binds to the nearest (innermost) if.
 *
 *   We express this by giving the if-without-else rule a lower
 *   precedence (%prec LOWER_THAN_ELSE) than the ELSE token, so
 *   when bison sees ELSE it shifts (attaches it to the inner if)
 *   rather than reducing (closing the outer if first).
 */
/* Relational operators (Topic 4) bind LOOSER than + - * /, which is
 * what makes  a + 1 < b * 2  parse as  (a + 1) < (b * 2)  and not
 * a + (1 < b) * 2.  As in C, equality (== !=) is one level looser
 * still than ordering (< > <= >=), so  a < b == c < d  compares the
 * two comparison results.  All are left associative. */
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/'
%right UMINUS              /* unary minus                            */

%%

/* PROGRAM RULE - Entry point */
program:
    decl_or_func_list {
        root = $1;
    }
    ;

/* Declaration or function list */
decl_or_func_list:
    decl_or_func {
        $$ = $1;
    }
    | decl_or_func_list decl_or_func {
        $$ = createStmtList($1, $2);
    }
    ;

/* Can be either a function definition or a variable declaration */
decl_or_func:
    func_def { $$ = $1; }
    | decl { $$ = $1; }
    ;


/* FUNCTION DEFINITION - int name(params) { body } */
func_def:
    INT ID '(' params ')' block {
        $$ = createFuncDef($2, $4, $6);
        free($2);
    }
    | INT ID '(' ')' block {
        $$ = createFuncDef($2, NULL, $5);
        free($2);
    }
    ;

/* PARAMETERS */
params:
    param_list { $$ = $1; }
    ;

param_list:
    param {
        $$ = $1;
    }
    | param_list ',' param {
        $$ = createParamList($1, $3);
    }
    ;

param:
    INT ID {
        $$ = createParam($2);
        free($2);
    }
    | INT ID '[' ']' {
        $$ = createArrayParam($2);
        free($2);
    }
    ;

/* BLOCK STATEMENT */
block:
    '{' stmt_list '}' {
        $$ = createBlock($2);
    }
    | '{' '}' {
        $$ = createBlock(NULL);
    }
    ;


/* STATEMENT LIST */
stmt_list:
    stmt {
        $$ = $1;
    }
    | stmt_list stmt {
        $$ = createStmtList($1, $2);
    }
    ;

/* STATEMENT TYPES */
stmt:
    decl
    | assign
    | print_stmt
    | return_stmt
    | for_stmt              /* Topic 4 */
/* --------------------------------------------------------------
 * TODO (Topic 4)
 * The other loop and `break` are statements too.  Add:
 *   | while_stmt
 *   | break_stmt
 * -------------------------------------------------------------- */
    | block
    | func_call ';' { $$ = $1; }
    ;


/* --------------------------------------------------------------
 * TODO (Topic 4)
 * BREAK STATEMENT.  One rule, one action:
 *   break_stmt: BREAK ';' { $$ = createBreak(); } ;
 * -------------------------------------------------------------- */

/* DECLARATION - int x; or int arr[10]; */
decl:
    INT ID ';' {
        $$ = createDecl("int", $2);
        free($2);
    }
    | INT ID '[' NUM ']' ';' {
        $$ = createArrayDecl($2, $4);
        free($2);
    }
    ;

/* ASSIGNMENT - x = expr; or arr[i] = expr; */
assign:
    ID '=' expr ';' {
        $$ = createAssign($1, $3);
        free($1);
    }
    | ID '[' expr ']' '=' expr ';' {
        ASTNode* lhs = createArrayIndex($1, $3);
        $$ = createAssign(NULL, $6);
        $$->data.assign.arrayLHS = lhs;
        free($1);
    }
    ;

/* RETURN STATEMENT */
return_stmt:
    RETURN expr ';' {
        $$ = createReturn($2);
    }
    | RETURN ';' {
        $$ = createReturn(NULL);
    }
    ;


/* FOR LOOP (Topic 4)
 *   for ( init ; cond ; update ) stmt
 *
 * All three parts of the header are OPTIONAL, so each has an empty
 * alternative that yields NULL.  A NULL condition means "loop forever",
 * as in C — the back end emits no test at all for it.
 *
 * init and update are assignments WITHOUT a semicolon: the two `;` in
 * the header belong to the for-statement, not to the assignments, so
 * the ordinary `assign` rule (which ends in ';') cannot be reused here.
 * They still build ordinary NODE_ASSIGN nodes, so every later phase
 * treats them exactly like any other assignment.
 *
 * The body is any `stmt`, so both  for (...) x = x + 1;  and a braced
 * block work. */
for_stmt:
    FOR '(' for_init ';' for_cond ';' for_update ')' stmt {
        $$ = createFor($3, $5, $7, $9);
    }
    ;

for_init:
    ID '=' expr {
        $$ = createAssign($1, $3);
        free($1);
    }
    | /* empty */ { $$ = NULL; }
    ;

for_cond:
    expr          { $$ = $1; }
    | /* empty */ { $$ = NULL; }
    ;

for_update:
    ID '=' expr {
        $$ = createAssign($1, $3);
        free($1);
    }
    | /* empty */ { $$ = NULL; }
    ;

/* --------------------------------------------------------------
 * TODO (Topic 4)
 * WHILE (FOR is done above).
 *
 *   while_stmt : WHILE '(' expr ')' stmt          -> createWhile(cond, body)
 * -------------------------------------------------------------- */

/* EXPRESSION RULES */
expr:
    NUM {
        $$ = createNum($1);
    }
    | ID {
        $$ = createVar($1);
        free($1);
    }
    | ID '[' expr ']' {
        $$ = createArrayIndex($1, $3);
        free($1);
    }
    | func_call {
        $$ = $1;
    }
    | expr '+' expr {
        $$ = createBinOp('+', $1, $3);
    }
    | expr '-' expr {
        $$ = createBinOp('-', $1, $3);
    }
    | expr '*' expr {
        $$ = createBinOp('*', $1, $3);
    }
    | expr '/' expr {
        $$ = createBinOp('/', $1, $3);
    }
    /* The six relational operators (Topic 4), each producing a BINOP
     * node.  createBinOp packs the operator into ONE char, so the
     * two-character operators get single-letter codes:  <= is 'l',
     * >= is 'g', == is 'e', != is 'n'  (see opText() in ast.c).
     * A comparison yields 1 or 0 — there is no separate boolean type.
     * Their precedence comes from the %left lines above, not from
     * the shape of these rules. */
    | expr '<' expr { $$ = createBinOp('<', $1, $3); }
    | expr '>' expr { $$ = createBinOp('>', $1, $3); }
    | expr LE  expr { $$ = createBinOp('l', $1, $3); }
    | expr GE  expr { $$ = createBinOp('g', $1, $3); }
    | expr EQ  expr { $$ = createBinOp('e', $1, $3); }
    | expr NE  expr { $$ = createBinOp('n', $1, $3); }
    | '-' expr %prec UMINUS {
        $$ = createBinOp('u', $2, NULL);  /* 'u' for unary minus */
    }
    | '(' expr ')' {
        $$ = $2;
    }
    ;

/* FUNCTION CALL */
func_call:
    ID '(' args ')' {
        $$ = createFuncCall($1, $3);
        free($1);
    }
    | ID '(' ')' {
        $$ = createFuncCall($1, NULL);
        free($1);
    }
    ;

/* ARGUMENTS */
args:
    arg_list { $$ = $1; }
    ;

arg_list:
    expr {
        $$ = $1;  /* Single argument becomes the arg node */
    }
    | arg_list ',' expr {
        $$ = createArgList($1, $3);
    }
    ;


/* PRINT STATEMENT */
print_stmt:
    PRINT '(' expr ')' ';' {
        $$ = createPrint($3);
    }
    ;

%%

/* ERROR HANDLING */
void yyerror(const char* s) {
    fprintf(stderr, "Syntax Error at line %d: %s\n", yylineno, s);
}
