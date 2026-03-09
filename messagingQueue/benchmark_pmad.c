/*
 * benchmark_pmad.c
 * ─────────────────────────────────────────────────────────────────────────
 * Message-pipeline benchmark — PMAD custom allocator.
 *
 * Workload pattern (per burst):
 *   1. Allocate BURST_SIZE Message objects       (pmad_alloc)
 *   2. Enqueue them  (Node allocated via pmad_alloc)
 *   3. Dequeue them  (Node freed   via pmad_free)
 *   4. Process each  (volatile black-box prevents dead-code elimination)
 *   5. Free each     (pmad_free)
 *
 * Repeat until TOTAL_MESSAGES have been processed.
 *
 * PMAD pool sizing rationale
 * ──────────────────────────
 *   sizeof(Message) = 264 → aligned to 272 (next multiple of ALIGNMENT=16)
 *   sizeof(Node)    =  16 → aligned stays at 16
 *   BlockHeader     =  16 bytes (with alignment)
 *
 *   Peak live objects per burst:
 *     Messages : 1000 × (272+16) = 288 000 bytes
 *     Nodes    : 1000 × ( 16+16) =  32 000 bytes
 *     Total                       ≈ 320 KB  →  fits in 1 MB pool
 *
 *   Size class layout (5 classes, percentages sum to 100):
 *     Class 0 → block_size 272  (Message)  80% ≈ 2844 blocks available
 *     Class 1 → block_size  16  (Node)     15% ≈ 4800 blocks available
 *     Class 2 → block_size 512             2%  (spare small objects)
 *     Class 3 → block_size 1024            2%  (spare medium objects)
 *     Class 4 → block_size 2048            1%  (spare large objects)
 *
 * Build:
 *   gcc -O3 -march=native -std=c99 \
 *       -I../include \
 *       benchmark_pmad.c                   \
 *       ../src/PMAD.c ../src/incPmad.c     \
 *       ../src/MemoryPool.c ../src/BlockHeader.c \
 *       -o bench_pmad -lm
 * ─────────────────────────────────────────────────────────────────────────
 */

#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/incPMAD.h" /* pmad_init / pmad_alloc / pmad_free / pmad_destroy */

#include "bench_common.h"
#include "message.h"
#include "queue.h"

/* ── PMAD configuration ─────────────────────────────────────────── */
/*
 * NUM_CLASSES == 5  (defined in PMAD.h).
 * The class_sizes[] array MUST have exactly 5 entries.
 * The percentages[] array MUST have exactly 5 entries summing to 100.
 *
 * Class 0 → 272  → covers sizeof(Message)=264, rounded to next 16-byte boundary
 * Class 1 →  16  → covers sizeof(Node)=16 (two 8-byte pointers)
 * Class 2 → 512  → spare
 * Class 3 → 1024 → spare
 * Class 4 → 2048 → spare
 */
static size_t pmad_class_sizes[] = {272, 16, 512, 1024, 2048};
static size_t pmad_percentages[] = {80, 15, 2, 2, 1};

/* ── allocator wrappers (satisfy AllocFn / FreeFn typedefs in queue.h) */

static void *pmad_alloc_wrap(size_t sz) { return pmad_alloc(sz); }
static void pmad_free_wrap(void *p) { pmad_free(p); }

/* ── benchmark driver ───────────────────────────────────────────── */

int main(void) {
  /* Initialise the PMAD allocator. */
  if (pmad_init(pmad_class_sizes, pmad_percentages) == 0) {
    fprintf(stderr, "pmad_init failed: percentages do not sum to 100, "
                    "or pool could not be mapped.\n");
    return 1;
  }

  /* Pre-allocate the latency sample buffer (uses system malloc — one time). */
  Stats alloc_stats;
  stats_init(&alloc_stats, MAX_SAMPLES);

  /* Burst staging array. */
  Message *burst[BURST_SIZE];

  Queue q;
  queue_init(&q);

  uint64_t wall_start = now_ns();
  size_t processed = 0;

  while (processed < TOTAL_MESSAGES) {
    size_t this_burst = BURST_SIZE;
    if (processed + this_burst > TOTAL_MESSAGES)
      this_burst = TOTAL_MESSAGES - processed;

    /* ── Phase 1: allocate + enqueue ──────────────────────── */
    for (size_t i = 0; i < this_burst; i++) {
      uint64_t t0 = now_ns();
      Message *m = (Message *)pmad_alloc_wrap(sizeof(Message));
      uint64_t t1 = now_ns();

      if (!m) {
        fprintf(stderr,
                "pmad_alloc returned NULL at message %zu. "
                "Pool exhausted — increase POOL_SIZE or reduce BURST_SIZE.\n",
                processed + i);
        pmad_destroy();
        return 1;
      }

      stats_record(&alloc_stats, t1 - t0);

      /* Populate fields to prevent elision. */
      m->topic = (int)((processed + i) % 64);
      m->size = (int)sizeof(Message);
      memset(m->payload, (int)(i & 0xFF), PAYLOAD_SIZE);

      burst[i] = m;
    }

    for (size_t i = 0; i < this_burst; i++) {
      enqueue(&q, burst[i], pmad_alloc_wrap);
    }

    /* ── Phase 2: dequeue + process + free ────────────────── */
    for (size_t i = 0; i < this_burst; i++) {
      Message *m = dequeue(&q, pmad_free_wrap);
      process_message(m);
      pmad_free_wrap(m);
    }

    processed += this_burst;
  }

  uint64_t wall_end = now_ns();
  double wall_sec = (double)(wall_end - wall_start) / 1e9;

  print_report("PMAD", TOTAL_MESSAGES, sizeof(Message), &alloc_stats, wall_sec);

  stats_free(&alloc_stats); /* frees the sample buffer (system malloc) */
  pmad_destroy();
  return 0;
}
