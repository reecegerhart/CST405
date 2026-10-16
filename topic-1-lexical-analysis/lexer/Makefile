# ============================================================================
# Topic 1 · Lexical analyzer
#   make          build ./lexer
#   make test     run it over every file in tests/ (full output)
#   make check    run every test and verify its expected error count
#   make clean    remove generated files
# ============================================================================
CC     = gcc
LEX    = flex
CFLAGS = -g -Wall

TARGET = lexer

all: $(TARGET)

$(TARGET): lex.yy.o main.o symtab.o
	$(CC) $(CFLAGS) -o $(TARGET) lex.yy.o main.o symtab.o

lex.yy.c: scanner.l tokens.h symtab.h
	$(LEX) scanner.l

lex.yy.o: lex.yy.c tokens.h symtab.h
	$(CC) -c $(CFLAGS)  lex.yy.c

main.o: main.c tokens.h symtab.h
	$(CC) -c $(CFLAGS)  main.c

symtab.o: symtab.c symtab.h
	$(CC) -c $(CFLAGS)  symtab.c

test: $(TARGET)
	@for f in tests/*.cm; do \
	   echo "══ $$f ═══════════════════════════════════════════"; \
	   ./$(TARGET) $$f; \
	   echo "   (exit status $$?)"; \
	done

# ----------------------------------------------------------------------------
# Regression check: each entry is  file:expected-lexical-errors.  A file with
# 0 expected errors must also exit 0 (certified); any other count must exit 1.
# Prints PASS/FAIL per file and fails the make if any test fails.
# ----------------------------------------------------------------------------
EXPECT = 01_all_tokens:0 02_longest_match:0 03_keywords_vs_ids:0 \
         04_comments:0 05_lexical_errors:4 06_unterminated_comment:1 \
         07_symbol_table:0 08_error_kinds:7 09_positions:2 \
         10_valid_program:0 11_unterminated_stars:1

check: $(TARGET)
	@fail=0; for e in $(EXPECT); do \
	   f=tests/$${e%%:*}.cm; want=$${e##*:}; \
	   got=$$(./$(TARGET) $$f -c 2>/dev/null | awk '/Lexical errors/ {print $$4}'); \
	   ./$(TARGET) $$f -c >/dev/null 2>&1; rc=$$?; \
	   if [ "$$want" = 0 ]; then wantrc=0; else wantrc=1; fi; \
	   if [ "$$got" = "$$want" ] && [ "$$rc" = "$$wantrc" ]; then \
	      echo "PASS  $$f  ($$got errors, exit $$rc)"; \
	   else \
	      echo "FAIL  $$f  (expected $$want errors / exit $$wantrc, got $$got / exit $$rc)"; fail=1; \
	   fi; \
	done; exit $$fail

clean:
	rm -f $(TARGET) *.o lex.yy.c

.PHONY: all test check clean
