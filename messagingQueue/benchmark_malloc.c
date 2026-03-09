/*
 * benchmark_malloc.c
 * ─────────────────────────────────────────────────────────────────────────
 * Message-pipeline benchmark — standard malloc/free allocator.
 *
 * Workload pattern (per burst):
 *   1. Allocate BURST_SIZE Message objects       (malloc)
 *   2. Enqueue them  (Node allocated via malloc)
 *   3. Dequeue them  (Node freed   via free)
 *   4. Process each  (volatile black-box prevents dead-code elimination)
 *   5. Free each     (free)
 *
 * Repeat until TOTAL_MESSAGES have been processed.
 *
 * Build:
 *   gcc -O3 -march=native -std=c99 -I../include \
 *       benchmark_malloc.c -o bench_malloc -lm
 * ─────────────────────────────────────────────────────────────────────────
 */

#define _POSIX_C_SOURCE 200809L
#define __USE_LOCALE /* for ' flag in printf on macOS */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench_common.h"
#include "message.h"
#include "queue.h"

/* ── allocator wrappers ─────────────────────────────────────────── */

static void *malloc_alloc(size_t sz) { return malloc(sz); }
static void malloc_free(void *p) { free(p); }

/* ── benchmark driver ───────────────────────────────────────────── */

int main(void) {
  /* Pre-allocate the latency sample buffer. */
  Stats alloc_stats;
  stats_init(&alloc_stats, MAX_SAMPLES);

  /* Burst staging arrays — BURST_SIZE messages at a time. */
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
      Message *m = (Message *)malloc_alloc(sizeof(Message));
      uint64_t t1 = now_ns();

      if (!m) {
        fprintf(stderr, "malloc failed at message %zu\n", processed + i);
        return 1;
      }

      stats_record(&alloc_stats, t1 - t0);

      /* Populate to make allocations real (prevent elision). */
      m->topic = (int)((processed + i) % 64);
      m->size = (int)sizeof(Message);
      memset(m->payload, (int)(i & 0xFF), PAYLOAD_SIZE);

      burst[i] = m;
    }

    for (size_t i = 0; i < this_burst; i++) {
      enqueue(&q, burst[i], malloc_alloc);
    }

    /* ── Phase 2: dequeue + process + free ────────────────── */
    for (size_t i = 0; i < this_burst; i++) {
      Message *m = dequeue(&q, malloc_free);
      process_message(m);
      malloc_free(m);
    }

    processed += this_burst;
  }

  uint64_t wall_end = now_ns();
  double wall_sec = (double)(wall_end - wall_start) / 1e9;

  print_report("malloc", TOTAL_MESSAGES, sizeof(Message), &alloc_stats,
               wall_sec);

  stats_free(&alloc_stats);
  return 0;
}
