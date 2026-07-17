# v3 benchmark suite — one harness source (bench.c), compiled once per
# allocator. Timed regions are identical across binaries; only the linked
# allocator differs.
#
#   make            # build everything
#   make run        # build + run the full suite (writes raw/ and results/)
#   make clean

CC       ?= clang
ROOT      = ../..
INC       = -I$(ROOT)/include
# -O3 -march=native, no LTO: every allocator (PMAD included) pays a real,
# non-inlined call — no harness favours.
CFLAGS    = -O3 -march=native -std=gnu11 -Wall $(INC)
BREW     ?= /opt/homebrew
LIBDIR    = -L$(BREW)/lib
JEINC     = -I$(BREW)/include
MIINC     = -I$(BREW)/include

PMAD_SRC  = $(ROOT)/src/PMAD.c $(ROOT)/src/incPMAD.c \
            $(ROOT)/src/MemoryPool.c $(ROOT)/src/BlockHeader.c

BINS = bench_pmad bench_system bench_jemalloc bench_tcmalloc bench_mimalloc

all: $(BINS)

bench_pmad: bench.c $(PMAD_SRC)
	$(CC) $(CFLAGS) -DBACKEND_PMAD bench.c $(PMAD_SRC) -o $@

bench_system: bench.c
	$(CC) $(CFLAGS) -DBACKEND_SYSTEM bench.c -o $@

bench_jemalloc: bench.c
	$(CC) $(CFLAGS) $(JEINC) -DBACKEND_JEMALLOC bench.c $(LIBDIR) -ljemalloc -o $@

bench_tcmalloc: bench.c
	$(CC) $(CFLAGS) -DBACKEND_TCMALLOC bench.c $(LIBDIR) -ltcmalloc_minimal -o $@

bench_mimalloc: bench.c
	$(CC) $(CFLAGS) $(MIINC) -DBACKEND_MIMALLOC bench.c $(LIBDIR) -lmimalloc -o $@

run: all
	./run_all.sh

clean:
	rm -f $(BINS)

.PHONY: all run clean
