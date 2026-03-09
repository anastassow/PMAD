/*
 * bench_common.h
 * Shared timing helpers and statistics types used by both
 * benchmark_malloc.c and benchmark_pmad.c.
 */

#ifndef BENCH_COMMON_H
#define BENCH_COMMON_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

/* -------------------------------------------------------------------
 * Parameters
 * ------------------------------------------------------------------- */
#define TOTAL_MESSAGES 50000000UL /* 50 million                  */
#define BURST_SIZE 1000UL         /* alloc/free in chunks of 1k  */
#define SAMPLE_INTERVAL 1000UL    /* record every Nth allocation  */
#define MAX_SAMPLES (TOTAL_MESSAGES / SAMPLE_INTERVAL)

/* -------------------------------------------------------------------
 * High-resolution timestamp (nanoseconds)
 * ------------------------------------------------------------------- */
static inline uint64_t now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* -------------------------------------------------------------------
 * Statistics accumulator
 * ------------------------------------------------------------------- */
typedef struct {
  uint64_t count;
  double sum;    /* ns */
  double sum_sq; /* ns^2 */
  uint64_t min;  /* ns */
  uint64_t max;  /* ns */

  /* Sampled latency array (heap-allocated by the benchmark) */
  uint64_t *samples;
  size_t n_samples;
  size_t samples_cap;
} Stats;

static inline void stats_init(Stats *s, size_t capacity) {
  s->count = 0;
  s->sum = 0.0;
  s->sum_sq = 0.0;
  s->min = UINT64_MAX;
  s->max = 0;
  s->samples = (uint64_t *)malloc(capacity * sizeof(uint64_t));
  s->n_samples = 0;
  s->samples_cap = capacity;
}

static inline void stats_record(Stats *s, uint64_t latency_ns) {
  s->count++;
  s->sum += (double)latency_ns;
  s->sum_sq += (double)latency_ns * (double)latency_ns;
  if (latency_ns < s->min)
    s->min = latency_ns;
  if (latency_ns > s->max)
    s->max = latency_ns;

  /* Collect samples at the chosen interval */
  if ((s->count % SAMPLE_INTERVAL == 0) && (s->n_samples < s->samples_cap)) {
    s->samples[s->n_samples++] = latency_ns;
  }
}

static inline double stats_average(const Stats *s) {
  return s->count ? s->sum / (double)s->count : 0.0;
}

static inline double stats_stddev(const Stats *s) {
  if (s->count < 2)
    return 0.0;
  double mean = stats_average(s);
  double variance = (s->sum_sq / (double)s->count) - (mean * mean);
  return (variance > 0.0) ? sqrt(variance) : 0.0;
}

static inline void stats_free(Stats *s) {
  free(s->samples);
  s->samples = NULL;
}

/* -------------------------------------------------------------------
 * Pretty printer
 * ------------------------------------------------------------------- */
static inline void print_report(const char *allocator_name, size_t total_ops,
                                size_t msg_size, const Stats *s,
                                double wall_sec) {
  double throughput = (double)total_ops / wall_sec / 1e6; /* M ops/sec */

  printf("\n");
  printf("══════════════════════════════════════════════════\n");
  printf("  Benchmark: %'zu messages\n", total_ops);
  printf("  Message size: %zu bytes\n", msg_size);
  printf("══════════════════════════════════════════════════\n");
  printf("  Allocator : %s\n", allocator_name);
  printf("──────────────────────────────────────────────────\n");
  printf("  Avg  latency  : %8.1f ns\n", stats_average(s));
  printf("  Std  deviation: %8.1f ns\n", stats_stddev(s));
  printf("  Min  latency  : %8" PRIu64 " ns\n",
         s->min == UINT64_MAX ? 0 : s->min);
  printf("  Max  latency  : %8" PRIu64 " ns\n", s->max);
  printf("  Throughput    : %8.1f M ops/sec\n", throughput);
  printf("  Wall time     : %8.3f s\n", wall_sec);
  printf("  Total samples : %8zu\n", s->n_samples);
  printf("══════════════════════════════════════════════════\n");
  printf("\n");
}

#endif /* BENCH_COMMON_H */
