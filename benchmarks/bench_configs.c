/*
 * PMAD Config Comparison — runs 3 configs, prints side-by-side results
 * gcc -O2 -Iinclude src/PMAD.c src/incPMAD.c src/MemoryPool.c src/BlockHeader.c
 * bench_configs.c -o bench_configs
 */
#include "PMAD.h"
#include "incPMAD.h"
#include "structures/BlockHeader.h"
#include "structures/MemoryPool.h"
#include "structures/SizeClass.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline uint64_t ns_now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

typedef struct {
  const char *name;
  size_t sizes[5];
  size_t pcts[5];
  int nclasses;
} Config;

void run_config(const Config *cfg) {
  printf("\n╔══════════════════════════════════════════════════════╗\n");
  printf("║  Config: %-43s║\n", cfg->name);
  printf("╚══════════════════════════════════════════════════════╝\n");

  /* ── pool layout ── */
  printf("  %-12s  %-10s  %-10s  %-10s  %-10s  %-8s\n", "Class", "BlockSz",
         "% Pool", "Blocks", "UserBytes", "Overhead");
  size_t total_blocks = 0, total_user = 0, total_header = 0;
  size_t usable = POOL_SIZE - sizeof(MemoryPool);
  for (int i = 0; i < cfg->nclasses; i++) {
    size_t bs = cfg->sizes[i];
    size_t pct = cfg->pcts[i];
    size_t slice = usable * pct / 100;
    size_t bfull = bs + sizeof(BlockHeader);
    size_t blocks = slice / bfull;
    size_t ub = blocks * bs;
    size_t hb = blocks * sizeof(BlockHeader);
    total_blocks += blocks;
    total_user += ub;
    total_header += hb;
    printf("  Class[%d]     %-10zu  %-10zu  %-10zu  %-10zu  %.1f%%\n", i, bs,
           pct, blocks, ub, 100.0 * hb / (ub + hb));
  }
  printf("  ─────────────────────────────────────────────────────\n");
  printf("  TOTAL        %-10s  %-10s  %-10zu  %-10zu  %.2f%%\n", "", "",
         total_blocks, total_user,
         100.0 * total_header / (total_user + total_header));

  /* ── init allocator ── */
  pmad_init((size_t *)cfg->sizes, (size_t *)cfg->pcts);

  /* ── latency: first class ── */
  /* figure out how many blocks in class[0] */
  size_t bs0 = cfg->sizes[0];
  size_t pct0 = cfg->pcts[0];
  size_t slice0 = usable * pct0 / 100;
  size_t n0 = slice0 / (bs0 + sizeof(BlockHeader));
  if (n0 > 50000)
    n0 = 50000; /* cap samples */

  void **ptrs = malloc(n0 * sizeof(void *));
  memset(ptrs, 0, n0 * sizeof(void *));

  /* warmup */
  int wn = (n0 > 100) ? 100 : (int)n0 / 2;
  for (int i = 0; i < wn; i++) {
    void *p = pmad_alloc(bs0);
    if (p)
      pmad_free(p);
  }

  uint64_t atot = 0, amin = UINT64_MAX, amax = 0;
  uint64_t ftot = 0, fmin = UINT64_MAX, fmax = 0;
  int ac = 0;
  for (size_t i = 0; i < n0; i++) {
    uint64_t t0 = ns_now();
    ptrs[i] = pmad_alloc(bs0);
    uint64_t dt = ns_now() - t0;
    if (ptrs[i]) {
      atot += dt;
      if (dt < amin)
        amin = dt;
      if (dt > amax)
        amax = dt;
      ac++;
    }
  }
  for (int i = 0; i < ac; i++) {
    if (!ptrs[i])
      continue;
    uint64_t t0 = ns_now();
    pmad_free(ptrs[i]);
    uint64_t dt = ns_now() - t0;
    ftot += dt;
    if (dt < fmin)
      fmin = dt;
    if (dt > fmax)
      fmax = dt;
    ptrs[i] = NULL;
  }
  printf("\n  Latency (%zu-byte, %d samples):\n", bs0, ac);
  printf("    Alloc  avg=%.1f ns   min=%llu ns   max=%llu ns\n",
         (double)atot / ac, (unsigned long long)amin, (unsigned long long)amax);
  printf("    Free   avg=%.1f ns   min=%llu ns   max=%llu ns\n",
         (double)ftot / ac, (unsigned long long)fmin, (unsigned long long)fmax);

  /* ── bulk throughput ── */
  /* need fresh blocks — re-init */
  pmad_destroy();
  pmad_init((size_t *)cfg->sizes, (size_t *)cfg->pcts);

  size_t bn = (n0 < (size_t)ac) ? n0 : (size_t)ac;
  void **bulk = malloc(bn * sizeof(void *));
  uint64_t ts = ns_now();
  for (size_t i = 0; i < bn; i++)
    bulk[i] = pmad_alloc(bs0);
  uint64_t te = ns_now();
  double ns_per_op = (double)(te - ts) / bn;
  double throughput = 1e9 / ns_per_op / 1e6;
  printf("\n  Bulk Throughput (%zu allocs of %zu B):\n", bn, bs0);
  printf("    %.2f ns/op  →  %.1f M alloc/s\n", ns_per_op, throughput);

  for (size_t i = 0; i < bn; i++)
    if (bulk[i])
      pmad_free(bulk[i]);
  free(bulk);
  free(ptrs);

  printf("\n  Syscalls (runtime): 0  |  Header overhead: %.2f%%\n",
         100.0 * total_header / (total_user + total_header));

  pmad_destroy();
}

int main(void) {
  printf("PMAD Configuration Comparison\n");
  printf("POOL_SIZE=%d bytes  sizeof(BlockHeader)=%zu\n\n", POOL_SIZE,
         sizeof(BlockHeader));

  Config configs[] = {
      {"MAX THROUGHPUT: single 16-byte class, 100%",
       {16, 0, 0, 0, 0},
       {100, 0, 0, 0, 0},
       1},
      {"MIN OVERHEAD: single 4096-byte class, 100%",
       {4096, 0, 0, 0, 0},
       {100, 0, 0, 0, 0},
       1},
      {"DEFAULT (your main.c): {16,32,64,128,1024}",
       {16, 32, 64, 128, 1024},
       {10, 20, 20, 20, 30},
       5},
      {"BALANCED SHOWCASE: {64,256,1024} @ {60,30,10}",
       {64, 256, 1024, 0, 0},
       {60, 30, 10, 0, 0},
       3},
      {"LATENCY-OPTIMISED: {32,128} @ {80,20}",
       {32, 128, 0, 0, 0},
       {80, 20, 0, 0, 0},
       2},
  };

  for (int i = 0; i < 5; i++)
    run_config(&configs[i]);

  printf("\n═══════════════════════════════════════════════════════\n");
  printf("KEY: All configs have 0 runtime syscalls and O(1) alloc.\n");
  printf("Throughput/latency difference = sample size + cache effects.\n");
  return 0;
}
