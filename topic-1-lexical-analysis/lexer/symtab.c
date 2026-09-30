/* ============================================================================
 *  TOPIC 1 · Compiler Design Phases
 * FILE: symtab.c   —   the scanner's symbol table (identifier table)
 * ----------------------------------------------------------------------------
 * Called from exactly one place: the {ident} rule in scanner.l.  main.c
 * only initializes it, prints it, and frees it.  See symtab.h for what the
 * table is for and what it deliberately does not know.
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

static Symbol*  buckets[SYM_BUCKETS];

/* The same entries again, in the order they were inserted, so the table
 * prints in source order rather than in hash order. */
static Symbol** ordered   = NULL;
static int      nSymbols  = 0;
static int      capacity  = 0;

static SymTrace trace[SYM_MAX_TRACE];
static int      nTrace       = 0;
static int      traceDropped = 0;

/* djb2: h = h*33 + c.  Simple, fast, and spreads short identifiers well. */
static unsigned hash(const char* s) {
    unsigned h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h % SYM_BUCKETS;
}

static void record(SymOp op, const Symbol* s, int line, int col) {
    if (nTrace == SYM_MAX_TRACE) { traceDropped++; return; }
    trace[nTrace++] = (SymTrace){ op, s->index, line, col,
                                  (int)hash(s->name), s->count };
}

/* Remember a line number once, even if the name appears on it twice. */
static void addLine(Symbol* s, int line) {
    if (s->lineCount > 0 && s->lines[s->lineCount - 1] == line) return;
    if (s->lineCount < SYM_MAX_LINES) s->lines[s->lineCount++] = line;
}

void symInit(void) {
    memset(buckets, 0, sizeof buckets);
    nSymbols = 0;
    nTrace = 0;
    traceDropped = 0;
}

Symbol* symLookup(const char* name) {
    for (Symbol* s = buckets[hash(name)]; s; s = s->next)
        if (strcmp(s->name, name) == 0)
            return s;
    return NULL;
}

Symbol* symInsert(const char* name, int line, int col) {
    Symbol* s = symLookup(name);
    if (s) {                                   /* seen before: LOOKUP hit */
        s->count++;
        addLine(s, line);
        record(SYM_LOOKUP, s, line, col);
        return s;
    }

    s = calloc(1, sizeof *s);                  /* first sighting: INSERT  */
    s->name      = strdup(name);
    s->index     = nSymbols;
    s->firstLine = line;
    s->firstCol  = col;
    s->count     = 1;
    addLine(s, line);

    unsigned b = hash(name);                   /* push onto its bucket    */
    s->next    = buckets[b];
    buckets[b] = s;

    if (nSymbols == capacity) {
        capacity = capacity ? capacity * 2 : 16;
        ordered  = realloc(ordered, capacity * sizeof *ordered);
    }
    ordered[nSymbols++] = s;

    record(SYM_INSERT, s, line, col);
    return s;
}

int symSize(void) { return nSymbols; }

void symPrintTrace(void) {
    printf("\n  SYMBOL TABLE INTERACTIONS  (one line per identifier scanned)\n");
    printf("   #  LINE  COL  OPERATION  NAME              BUCKET  RESULT\n");
    printf(" ───  ────  ───  ─────────  ────────────────  ──────  ───────────────────────\n");
    for (int i = 0; i < nTrace; i++) {
        const SymTrace* t = &trace[i];
        const Symbol*   s = ordered[t->index];
        if (t->op == SYM_INSERT)
            printf(" %3d  %4d  %3d  INSERT     %-16.16s  %6d  new entry [%d]\n",
                   i + 1, t->line, t->col, s->name, t->bucket, t->index);
        else
            printf(" %3d  %4d  %3d  LOOKUP     %-16.16s  %6d  found [%d], refs now %d\n",
                   i + 1, t->line, t->col, s->name, t->bucket, t->index,
                   t->countAfter);
    }
    if (nTrace == 0)
        printf("   (no identifiers in this file)\n");
    if (traceDropped)
        printf("   ... %d more interactions not shown\n", traceDropped);
}

void symPrintTable(void) {
    int used = 0;
    for (int b = 0; b < SYM_BUCKETS; b++)
        if (buckets[b]) used++;

    printf("\n  SYMBOL TABLE  (%d distinct identifier(s), %d of %d buckets used)\n",
           nSymbols, used, SYM_BUCKETS);
    printf("  IDX  NAME              BUCKET  FIRST SEEN  REFS  LINES\n");
    printf("  ───  ────────────────  ──────  ──────────  ────  ─────────────────\n");
    for (int i = 0; i < nSymbols; i++) {
        const Symbol* s = ordered[i];
        char first[24];
        snprintf(first, sizeof first, "%d:%d", s->firstLine, s->firstCol);
        printf("  %3d  %-16.16s  %6u  %-10s  %4d  ",
               s->index, s->name, hash(s->name), first, s->count);
        for (int j = 0; j < s->lineCount; j++)
            printf("%s%d", j ? ", " : "", s->lines[j]);
        if (s->lineCount == SYM_MAX_LINES) printf(", ...");
        printf("\n");
    }
    if (nSymbols == 0)
        printf("    (empty)\n");
}

void symFree(void) {
    for (int i = 0; i < nSymbols; i++) {
        free(ordered[i]->name);
        free(ordered[i]);
    }
    free(ordered);
    ordered  = NULL;
    capacity = 0;
    symInit();
}
