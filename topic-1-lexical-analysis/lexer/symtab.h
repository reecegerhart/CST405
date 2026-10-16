#ifndef SYMTAB_H
#define SYMTAB_H

/* ============================================================================
 *  TOPIC 1 · Compiler Design Phases
 * FILE: symtab.h   —   the scanner's symbol table (identifier table)
 * ----------------------------------------------------------------------------
 * WHAT THIS TABLE IS
 * Every time the scanner recognizes an identifier it asks this table one
 * question: "have I seen this name before?"
 *
 *     first time   ->  INSERT   a new entry is created, the name is copied
 *     every other  ->  LOOKUP   the existing entry is found and updated
 *
 * So after one pass over the source the table holds every DISTINCT name the
 * program uses, where each was first seen, and every line it appears on.
 *
 * WHAT IT DOES NOT KNOW
 * Types, scopes, and addresses.  The scanner cannot know those: it sees
 * `x` but not whether `x` was declared, in which block, or as an int or an
 * array.  Those facts belong to later phases — the scope stack in
 * semantic.c answers "is this name visible here?", and the storage map in
 * symtab.c of the full compiler answers "what address does it live at?".
 * This table only answers "which names exist in this program?", which is
 * the one question the scanner is in a position to answer.
 *
 * STRUCTURE
 * A hash table with separate chaining.  Lookup is the hot path — it runs
 * once per identifier occurrence — so it has to be O(1) on average rather
 * than a linear scan over every name seen so far.
 * ==========================================================================*/

#define SYM_BUCKETS    211   /* prime, so the hash spreads names evenly     */
#define SYM_MAX_LINES  16    /* line numbers remembered per entry           */
#define SYM_MAX_TRACE  1024  /* interactions remembered for the trace      */

typedef struct Symbol {
    char* name;                  /* the identifier's text (owned copy)      */
    int   index;                 /* insertion order: 0, 1, 2, ...           */
    int   firstLine, firstCol;   /* where it was first seen                 */
    int   count;                 /* how many times it appears in total      */
    int   lines[SYM_MAX_LINES];  /* distinct lines it appears on            */
    int   lineCount;
    struct Symbol* next;         /* next entry in the same hash bucket      */
} Symbol;

/* One interaction with the table, recorded so it can be printed later. */
typedef enum { SYM_INSERT, SYM_LOOKUP } SymOp;

typedef struct {
    SymOp op;
    int   index;                 /* which entry was created or found        */
    int   line, col;             /* where in the source the lookup came from */
    int   bucket;                /* which hash bucket it landed in          */
    int   countAfter;            /* the entry's reference count afterwards  */
} SymTrace;

void    symInit(void);
Symbol* symLookup(const char* name);                      /* NULL if absent */
Symbol* symInsert(const char* name, int line, int col);  /* lookup-or-insert */
void    symPrintTrace(void);
void    symPrintTable(void);
int     symSize(void);
void    symFree(void);

#endif
