** Stili

ast.c 
* --------------------------------------------------------------------
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

codegen.c
 /* --------------------------------------------------------
             * TODO (Topic 2) — TAC -> MIPS Stili
             * One case per TAC opcode.  Everything you need is already written above:
             *
             *     operandReg(name)   register holding that value (loads it, or does
             *                        `li` if it is a literal)
             *     defReg(name)       register to WRITE a new value of `name` into
             *     flushRegisters()   write every dirty register back to memory
             *
             *   TAC_DECL     no instruction — the slot was reserved by layoutFrame().
             *                Emit a comment saying where it lives; you will be glad of
             *                it the first time you read your own assembly.
             *
             *   TAC_ASSIGN   result = arg1
             *                    int a = operandReg(i->arg1);
             *                    int d = defReg(i->result);
             *                    move $td, $ta
             *
             *   TAC_ADD      result = arg1 + arg2   ->   add $td, $ta, $tb
             *
             *   TAC_PRINT    print arg1, using the SPIM syscalls:
             *                    move $a0, $t<arg>
             *                    li   $v0, 1        # 1 = print integer
             *                    syscall
             *                    la   $a0, __nl     # then a newline
             *                    li   $v0, 4        # 4 = print string
             *                    syscall
             *
             *   TAC_RETURN   put the value in $v0, then jump to the epilogue label.
             *                Do NOT just fall through.
             *
             * WHY defReg AND operandReg ARE DIFFERENT
             *   operandReg must LOAD the value from memory if it is not already in a
             *   register.  defReg must not: the register is about to be overwritten,
             *   so loading first is a wasted instruction.  Use the wrong one and your
             *   code still works — just with an extra `lw` everywhere.  Reading your
             *   own output and spotting that is a genuinely good exercise.
             * -------------------------------------------------------- */


Semantic.c 
/* ----------------------------------------------------------------
     * TODO (Topic 2) — CHECK A STATEMENT  Stili
     *     NODE_DECL    the name must NOT already be declared in this scope.
     *                  On success, add it: addVarToScope(name).
     *     NODE_ASSIGN  the target must already be declared; then check the
     *                  expression on the right with checkExpr.
     *     NODE_PRINT   check the expression.
     *     NODE_STMT_LIST  recurse into both halves via checkStmtList.
     *
     * Order matters in NODE_ASSIGN and it is easy to get backwards.  For
     *     int x;  x = x + 1;
     * checking the right-hand side must happen with x already in scope.  For
     *     int x = x + 1;
     * (a form this language does not have — but think about it) it should not.
     * Languages differ here, and this is where that decision gets made.
     * ---------------------------------------------------------------- */

symtab.c
/* ----------------------------------------------------------------
     * TODO (Topic 2) — RESOLVE A NAME  Stili
     * Return the symbol for `name`, or NULL if it is not declared.
     * Search the LOCAL table first and the GLOBAL table second: that order is
     * what makes an inner declaration shadow an outer one, and it is the
     * entire implementation of scoping at this milestone.
     * ---------------------------------------------------------------- */

tac.c
/* ----------------------------------------------------------------
     * TODO (Topic 2) — STATEMENT -> THREE-ADDRESS CODE Stili
     *     NODE_DECL    emit TAC_DECL — no code runs, but the back end needs to
     *                  know the variable exists so it can reserve a slot
     *     NODE_ASSIGN  evaluate the expression, then emit TAC_ASSIGN
     *     NODE_PRINT   evaluate the expression, then emit TAC_PRINT
     *     NODE_STMT_LIST  recurse
     *
     * Use appendTAC(createTAC(op, arg1, arg2, result)) to emit.
     * ---------------------------------------------------------------- */




**Reece Gerhart 

ast.c
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

semantic.c
 /* ----------------------------------------------------------------
     * TODO (Topic 2) — CHECK AN EXPRESSION Reece 
     * Walk the expression and report anything that cannot mean what it says.
     *
     *     NODE_NUM    always fine
     *     NODE_VAR    the name must be declared and VISIBLE here
     *                 -> isVarDeclaredInScope(node->data.name)
     *     NODE_BINOP  nothing to check about the operator itself; recurse into
     *                 both operands
     *
     * When you report an error: give the LINE NUMBER (node->lineno), name the
     * identifier, and say what would fix it.  Compare these two messages and
     * decide which one you would rather receive:
     *
     *     error: undeclared identifier
     *     line 7: 'totl' is not declared — did you mean 'total'?
     *
     * Increment semInfo.errorCount for each error.  Do NOT stop at the first
     * one: report everything you can find in a single run.
     * ---------------------------------------------------------------- */

symtab.c
/* ----------------------------------------------------------------
     * TODO (Topic 2) — DECLARE A VARIABLE Reece 
     * Add `name` to the local table and give it a home in the frame.
     * Return the byte offset you assigned, or -1 if the name is already
     * declared — the caller uses -1 to report a duplicate declaration.
     *
     * Each int takes 4 bytes, and offsets run upward from 0.  So the first
     * variable lives at 0($sp), the second at 4($sp), and nextOffset is
     * simply the running total.
     *
     * findIn() and appendTo() above do the searching and the allocation.
     * ---------------------------------------------------------------- */

tac.c
/* ----------------------------------------------------------------
     * TODO (Topic 2) — EXPRESSION -> THREE-ADDRESS CODE Reece
     * Return the NAME of the location holding this expression's value.  That
     * return value is the whole contract, and it is what makes the recursion
     * work: a caller does not care whether it gets back a literal, a variable
     * or a temporary, only that it can name the value.
     *
     *     NODE_NUM    return a string holding the literal, e.g. "42"
     *     NODE_VAR    return a copy of the variable's name
     *     NODE_BINOP  t = allocTemp();
     *                 left  = generateTACExpr(left child)
     *                 right = generateTACExpr(right child)
     *                 emit  t = left + right
     *                 freeTemp(left); freeTemp(right);
     *                 return t
     *
     * Free the operand temporaries AFTER emitting, never before: freeing t1
     * and then using it in the instruction you are about to emit is how you
     * end up with two live values in the same temporary.
     *
     * For  a + b + c  you should get exactly three instructions.  If you get
     * four, or if a temporary number is reused while still live, print the
     * TAC and walk it by hand — that listing is the point of this phase.
     * ---------------------------------------------------------------- */

tac.c 
/* ----------------------------------------------------------------
     * TODO (Topic 2) — ONE OPTIMIZATION PASS Reece
     * Copy `in` to `out`, rewriting what you can along the way.  Start with
     * the two transformations that pay off immediately on this language:
     *
     *   CONSTANT FOLDING     t0 = 2 + 3      ->   t0 = 5
     *                        Both operands are literals, so do the arithmetic
     *                        now instead of at run time.  foldConstants() is
     *                        already written for you.
     *
     *   CONSTANT PROPAGATION x = 5 ; y = x + 1   ->   y = 5 + 1
     *                        Remember that x holds 5, and substitute it into
     *                        later operands.  Then folding turns that into 6,
     *                        which is why these two techniques belong together.
     *
     *   THE RULE YOU MUST NOT BREAK: forget every remembered value at a LABEL.
     *   Control can arrive at a label from anywhere, so nothing you learned
     *   before it is still guaranteed.  Topic 2 has no labels yet — but write
     *   the code as if it did, because Topic 4 will add them and you will not
     *   remember this warning then.
     *
     * Count every rewrite in changesThisPass so optimizeTAC() knows whether to
     * run again, and in the matching optStats field so main.c can report it.
     * ---------------------------------------------------------------- */