# ─────────────────────────────────────────────────────────────────────────
# Makefile — messagingQueue benchmark suite
#
# Usage:
#   make              → build both bench_malloc and bench_pmad
#   make bench_malloc → build only the malloc benchmark
#   make bench_pmad   → build only the PMAD benchmark
#   make run          → build and run both benchmarks sequentially
#   make clean        → remove all build artefacts
# ─────────────────────────────────────────────────────────────────────────

CC      = gcc
STD     = -std=c99
OPT     = -O3 -march=native
WARN    = -Wall -Wextra -Wpedantic
LIBS    = -lm

# Paths relative to this Makefile (messagingQueue/)
PMAD_SRC_DIR  = ../src
PMAD_INC_DIR  = ../include

CFLAGS  = $(STD) $(OPT) $(WARN) -I. -I$(PMAD_INC_DIR)

# PMAD object files (compiled from the parent project sources)
PMAD_SRCS = $(PMAD_SRC_DIR)/PMAD.c        \
            $(PMAD_SRC_DIR)/incPmad.c      \
            $(PMAD_SRC_DIR)/MemoryPool.c   \
            $(PMAD_SRC_DIR)/BlockHeader.c

PMAD_OBJS = pmad_core.o incpmad.o mempool.o blockheader.o

# ── Default target ────────────────────────────────────────────────
.PHONY: all run clean

all: bench_malloc bench_pmad

# ── bench_malloc ─────────────────────────────────────────────────
bench_malloc: benchmark_malloc.c message.h queue.h bench_common.h
	$(CC) $(CFLAGS) benchmark_malloc.c -o bench_malloc $(LIBS)
	@echo "  → bench_malloc built."

# ── bench_pmad ───────────────────────────────────────────────────
bench_pmad: benchmark_pmad.c message.h queue.h bench_common.h $(PMAD_OBJS)
	$(CC) $(CFLAGS) benchmark_pmad.c $(PMAD_OBJS) -o bench_pmad $(LIBS)
	@echo "  → bench_pmad built."

# Compile individual PMAD source files
pmad_core.o: $(PMAD_SRC_DIR)/PMAD.c
	$(CC) $(CFLAGS) -c $< -o $@

incpmad.o: $(PMAD_SRC_DIR)/incPmad.c
	$(CC) $(CFLAGS) -c $< -o $@

mempool.o: $(PMAD_SRC_DIR)/MemoryPool.c
	$(CC) $(CFLAGS) -c $< -o $@

blockheader.o: $(PMAD_SRC_DIR)/BlockHeader.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── Run both benchmarks ───────────────────────────────────────────
run: all
	@echo ""
	@echo "══════════════════════════════════════════════════"
	@echo "  Running malloc benchmark …"
	@echo "══════════════════════════════════════════════════"
	./bench_malloc
	@echo ""
	@echo "══════════════════════════════════════════════════"
	@echo "  Running PMAD benchmark …"
	@echo "══════════════════════════════════════════════════"
	./bench_pmad

# ── Clean up ──────────────────────────────────────────────────────
clean:
	rm -f bench_malloc bench_pmad $(PMAD_OBJS)
	@echo "  → Cleaned."
