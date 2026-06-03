/*
 * bench.c — Unified allocator micro-benchmark harness.
 *
 * ONE source file. The allocator under test is chosen at COMPILE time via
 * -DBACKEND_xxx and linked in; the timed regions below are byte-identical for
 * every backend, so swapping allocators changes exactly one variable (the
 * inlined be_alloc/be_free body). This is principle #1: isolate the thing
 * under test — we compare allocators, not harnesses.
 *
 * Backends (mutually exclusive):
 *   -DBACKEND_PMAD       PMAD (this repo)        -> pmad_alloc / pmad_free
 *   -DBACKEND_SYSTEM     macOS libmalloc / glibc -> malloc / free
 *   -DBACKEND_JEMALLOC   jemalloc 5.x            -> mallocx / dallocx
 *   -DBACKEND_TCMALLOC   gperftools tcmalloc     -> tc_malloc / tc_free
 *   -DBACKEND_MIMALLOC   mimalloc                -> mi_malloc / mi_free
 *
 * Modes (argv[1]):
 *   lat   <size> <iters> [K] [seed]               per-op alloc+free latency
 *   churn <size> <workingset> <iters> [K] [seed]  random interleave vs live set
 *   tput  <size> <batch> <rounds>                 UNINSTRUMENTED throughput (#11)
 *   mem   <size> <count>                          measured RSS overhead (#5)
 *
 * Timing (principle #2/#3): finest userspace clock per platform; the timer's
 * own floor is calibrated and subtracted from every measured interval.
 *   - Apple Silicon : CLOCK_UPTIME_RAW, 41.67 ns tick. A single sub-tick op
 *                     cannot be resolved, so we time BLOCKS of K ops and divide
 *                     (block-amortised). With K=16 the per-op resolution is
 *                     ~2.6 ns, lifting the whole distribution above the tick
 *                     while a µs-scale spike still shows as an outlier block.
 *   - x86_64        : RDTSCP cycle counter (set K=1; the counter resolves a
 *                     single op directly). This box yields the cleanest
 *                     low-percentile numbers.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* fixed-point scale for stored per-op samples: store round(ns * FP) in u32.
 * FP=64 -> 1/64 ns resolution, max ~67 ms/op. */
#define FP 64u

/* ------------------------------------------------------------------ */
/* Platform timer                                                      */
/* ------------------------------------------------------------------ */
#if defined(__x86_64__)
  #include <x86intrin.h>
  static double g_tsc_ghz = 0.0;
  #define TIMER_NAME "RDTSCP (x86 cycle counter)"
  static inline uint64_t now_raw(void){ unsigned a; return __rdtscp(&a); }
#elif defined(__APPLE__)
  static inline uint64_t now_raw(void){ return clock_gettime_nsec_np(CLOCK_UPTIME_RAW); }
  #define TIMER_NAME "CLOCK_UPTIME_RAW (Apple, 41.67 ns tick)"
#else
  static inline uint64_t now_raw(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC_RAW,&ts); return (uint64_t)ts.tv_sec*1000000000ULL+ts.tv_nsec; }
  #define TIMER_NAME "CLOCK_MONOTONIC_RAW"
#endif

/* nanoseconds per raw tick (identity except on x86, where raw == cycles) */
static double g_ns_per_raw = 1.0;
static uint64_t g_timer_floor_raw = 0;  /* min cost of two back-to-back reads */

/* compiler barrier: force the allocation to be "observed" so the
 * alloc/free pair is real work and cannot be optimised away (principle #6). */
static inline void escape(void* p){ __asm__ volatile("" : : "g"(p) : "memory"); }
static volatile uintptr_t g_sink;

/* ------------------------------------------------------------------ */
/* QoS / core stability (principle #8, best available on stock macOS)  */
/* ------------------------------------------------------------------ */
#if defined(__APPLE__)
  #include <pthread.h>
  #include <sys/qos.h>
  static void pin_self(void){ pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0); }
#elif defined(__linux__)
  #include <sched.h>
  static void pin_self(void){ cpu_set_t s; CPU_ZERO(&s); CPU_SET(0,&s); sched_setaffinity(0,sizeof(s),&s); }
#else
  static void pin_self(void){}
#endif

/* ------------------------------------------------------------------ */
/* Backend (the ONE variable)                                          */
/* ------------------------------------------------------------------ */
#if defined(BACKEND_PMAD)
  #include "incPMAD.h"
  #define BE_NAME "PMAD"
  static void be_init(size_t blksz, size_t capacity_blocks){
      size_t cs[1]  = { blksz };
      size_t pct[1] = { 100 };
      size_t hdr = 16;
      size_t need = (blksz + hdr) * (capacity_blocks + capacity_blocks/4 + 1024) + (1u<<20);
      PmadStatus s = pmad_init(cs, 1, pct, need);
      if (s != PMAD_OK){ fprintf(stderr,"PMAD init failed %d (need=%zu)\n",s,need); exit(3);} }
  static inline void* be_alloc(size_t s){ return pmad_alloc(s); }
  static inline void  be_free(void* p){ pmad_free(p); }
  static void be_shutdown(void){ pmad_destroy(); }

#elif defined(BACKEND_SYSTEM)
  #include <stdlib.h>
  #define BE_NAME "system"
  static void be_init(size_t b, size_t c){ (void)b;(void)c; }
  static inline void* be_alloc(size_t s){ return malloc(s); }
  static inline void  be_free(void* p){ free(p); }
  static void be_shutdown(void){}

#elif defined(BACKEND_JEMALLOC)
  #include <stddef.h>
  extern void* mallocx(size_t, int);
  extern void  dallocx(void*, int);
  #define BE_NAME "jemalloc"
  static void be_init(size_t b, size_t c){ (void)b;(void)c; }
  static inline void* be_alloc(size_t s){ return mallocx(s, 0); }
  static inline void  be_free(void* p){ dallocx(p, 0); }
  static void be_shutdown(void){}

#elif defined(BACKEND_TCMALLOC)
  #include <stddef.h>
  extern void* tc_malloc(size_t);
  extern void  tc_free(void*);
  #define BE_NAME "tcmalloc"
  static void be_init(size_t b, size_t c){ (void)b;(void)c; }
  static inline void* be_alloc(size_t s){ return tc_malloc(s); }
  static inline void  be_free(void* p){ tc_free(p); }
  static void be_shutdown(void){}

#elif defined(BACKEND_MIMALLOC)
  #include <stddef.h>
  extern void* mi_malloc(size_t);
  extern void  mi_free(void*);
  #define BE_NAME "mimalloc"
  static void be_init(size_t b, size_t c){ (void)b;(void)c; }
  static inline void* be_alloc(size_t s){ return mi_malloc(s); }
  static inline void  be_free(void* p){ mi_free(p); }
  static void be_shutdown(void){}

#else
  #error "Define one BACKEND_* macro"
#endif

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static void calibrate_timer(void){
    enum { K = 400000 };
    uint64_t mn = UINT64_MAX;
    for (int i = 0; i < K; i++){
        uint64_t a = now_raw();
        uint64_t b = now_raw();
        uint64_t d = b - a;
        if (d < mn) mn = d;
    }
    g_timer_floor_raw = mn;
}

static int cmp_u32(const void* a, const void* b){
    uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b;
    return (x>y) - (x<y);
}

static inline uint64_t xs(uint64_t* s){ uint64_t x=*s; x^=x<<13; x^=x>>7; x^=x<<17; *s=x; return x; }

/* raw delta over K ops -> fixed-point per-op ns sample (floor-subtracted) */
static inline uint32_t to_fp(uint64_t raw_delta, unsigned K){
    uint64_t d = (raw_delta > g_timer_floor_raw) ? (raw_delta - g_timer_floor_raw) : 0;
    double ns_per_op = ((double)d * g_ns_per_raw) / (double)K;
    double v = ns_per_op * (double)FP;
    if (v < 0) v = 0;
    if (v > (double)UINT32_MAX) v = (double)UINT32_MAX;
    return (uint32_t)(v + 0.5);
}

static double pctile(const uint32_t* sorted, size_t n, double p){
    if (!n) return 0;
    size_t idx = (size_t)(p/100.0 * (double)(n-1));
    return (double)sorted[idx] / (double)FP;
}

static void print_dist(const char* tag, size_t size, unsigned K, uint32_t* s, size_t n){
    qsort(s, n, sizeof(uint32_t), cmp_u32);
    double mean = 0; for (size_t i=0;i<n;i++) mean += s[i];
    mean = mean/(double)FP/(double)n;
    double p50=pctile(s,n,50),  p90=pctile(s,n,90),  p99=pctile(s,n,99),
           p999=pctile(s,n,99.9), p9999=pctile(s,n,99.99), mx=(double)s[n-1]/(double)FP;
    double fan = p50>0 ? p9999/p50 : 0;
    /* tail-exceedance: count of samples whose per-op latency >= threshold.
     * Thresholds (100/1000/10000 ns) are far above the 41 ns timer floor, so
     * these counts are exact even single-op (K=1) — immune to quantisation. */
    uint32_t t100 = 100u*FP, t1k = 1000u*FP, t10k = 10000u*FP;
    size_t c100=0,c1k=0,c10k=0;
    for (size_t i=0;i<n;i++){ if(s[i]>=t100)c100++; if(s[i]>=t1k)c1k++; if(s[i]>=t10k)c10k++; }
    printf("RESULT %s backend=%-8s size=%-4zu K=%u N=%zu mean=%.2f p50=%.2f p90=%.2f "
           "p99=%.2f p99.9=%.2f p99.99=%.2f max=%.2f tail_ratio=%.1f "
           "ge100ns=%zu(%.1fppm) ge1us=%zu(%.2fppm) ge10us=%zu(%.3fppm)\n",
           tag, BE_NAME, size, K, n, mean, p50, p90, p99, p999, p9999, mx, fan,
           c100, 1e6*c100/n, c1k, 1e6*c1k/n, c10k, 1e6*c10k/n);
}

/* ================================================================== */
/* Mode: lat — per-op alloc+free round-trip latency distribution       */
/* timed in blocks of K ops (block-amortised) — see header             */
/* ================================================================== */
static void mode_lat(size_t size, size_t iters, unsigned K){
    be_init(size, 4);
    size_t nsamp = iters / K;
    uint32_t* samp = malloc(nsamp * sizeof(uint32_t));
    if (!samp){ fprintf(stderr,"OOM samples\n"); exit(4); }
    memset(samp, 0, nsamp * sizeof(uint32_t));   /* pre-fault sample buffer (#4) */

    /* warmup: trains caches/branch predictor, faults pages; discarded (#4) */
    for (size_t i = 0; i < 200000; i++){
        void* p = be_alloc(size);
        if (p){ ((volatile char*)p)[0] = 1; escape(p); be_free(p); }
    }

    /* timed region: NOTHING but alloc/free + clock reads (#5) */
    for (size_t b = 0; b < nsamp; b++){
        uint64_t t0 = now_raw();
        for (unsigned k = 0; k < K; k++){
            void* p = be_alloc(size);
            escape(p);                /* observe (#6) */
            be_free(p);
            g_sink += (uintptr_t)p;
        }
        uint64_t t1 = now_raw();
        samp[b] = to_fp(t1 - t0, K);
    }

    print_dist("LAT", size, K, samp, nsamp);
    free(samp);
    be_shutdown();
}

/* ================================================================== */
/* Mode: churn — random interleave against a live working set (#7)     */
/* tracks per-window percentiles to expose latency DRIFT over time     */
/* ================================================================== */
static void mode_churn(size_t size, size_t W, size_t iters, unsigned K, uint64_t seed){
    be_init(size, W);
    void** live = malloc(W * sizeof(void*));
    size_t nsamp = iters / K;
    uint32_t* samp = malloc(nsamp * sizeof(uint32_t));
    if (!live || !samp){ fprintf(stderr,"OOM\n"); exit(4); }
    memset(samp, 0, nsamp * sizeof(uint32_t));

    /* build + pre-fault the working set (#4) */
    for (size_t i = 0; i < W; i++){
        live[i] = be_alloc(size);
        if (!live[i]){ fprintf(stderr,"churn: working set %zu too big at i=%zu\n", W, i); exit(5);}
        ((volatile char*)live[i])[0] = (char)i;
    }
    uint64_t s = seed ? seed : 0x9E3779B97F4A7C15ULL;
    /* warmup churn (discarded): reach steady state */
    for (size_t i = 0; i < 500000; i++){
        size_t j = xs(&s) % W;
        be_free(live[j]);
        live[j] = be_alloc(size);
        if (live[j]){ ((volatile char*)live[j])[0]=1; escape(live[j]); }
    }

    /* timed steady-state churn, blocks of K interleaved free+alloc steps */
    for (size_t b = 0; b < nsamp; b++){
        uint64_t t0 = now_raw();
        for (unsigned k = 0; k < K; k++){
            size_t j = xs(&s) % W;
            be_free(live[j]);
            void* p = be_alloc(size);
            escape(p);
            live[j] = p;
            if (!p){ fprintf(stderr,"churn: NULL at block %zu\n", b); exit(6);}
            g_sink += (uintptr_t)p;
        }
        uint64_t t1 = now_raw();
        samp[b] = to_fp(t1 - t0, K);
    }

    /* per-window drift table -> stderr (committed via redirect) */
    const int WINDOWS = 100;
    size_t per_win = nsamp / WINDOWS;
    fprintf(stderr, "WINDOW backend=%s size=%zu W=%zu windows=%d samples_per_win=%zu\n",
            BE_NAME, size, W, WINDOWS, per_win);
    uint32_t* tmp = malloc(per_win * sizeof(uint32_t));
    for (int w = 0; w < WINDOWS; w++){
        memcpy(tmp, samp + (size_t)w*per_win, per_win*sizeof(uint32_t));
        qsort(tmp, per_win, sizeof(uint32_t), cmp_u32);
        double p50  = (double)tmp[(size_t)(0.50*(per_win-1))]/FP;
        double p99  = (double)tmp[(size_t)(0.99*(per_win-1))]/FP;
        double p999 = (double)tmp[(size_t)(0.999*(per_win-1))]/FP;
        double mx   = (double)tmp[per_win-1]/FP;
        fprintf(stderr, "WIN %s %zu %d %zu %.2f %.2f %.2f %.2f\n",
                BE_NAME, size, w, (size_t)w*per_win*K, p50, p99, p999, mx);
    }
    free(tmp);

    print_dist("CHURN", size, K, samp, nsamp);

    for (size_t i = 0; i < W; i++) if (live[i]) be_free(live[i]);
    free(live); free(samp);
    be_shutdown();
}

/* ================================================================== */
/* Mode: tput — UNINSTRUMENTED throughput (principle #11)              */
/* ================================================================== */
static void mode_tput(size_t size, size_t batch, size_t rounds){
    be_init(size, batch);
    void** v = malloc(batch * sizeof(void*));
    if (!v){ fprintf(stderr,"OOM\n"); exit(4); }
    for (size_t i = 0; i < batch; i++){ v[i] = be_alloc(size); if(v[i]) ((volatile char*)v[i])[0]=1; }
    for (size_t i = 0; i < batch; i++) if (v[i]) be_free(v[i]);

    uint64_t t0 = now_raw();
    for (size_t r = 0; r < rounds; r++){
        for (size_t i = 0; i < batch; i++){ v[i] = be_alloc(size); escape(v[i]); }
        for (size_t i = 0; i < batch; i++){ be_free(v[i]); }
    }
    uint64_t t1 = now_raw();
    for (size_t i=0;i<batch;i++) g_sink += (uintptr_t)v[i];

    double ns = (double)(t1 - t0) * g_ns_per_raw;
    double ops = (double)rounds * (double)batch * 2.0;   /* alloc + free */
    double ns_per_op = ns / ops;
    double mops = 1000.0 / ns_per_op;
    printf("RESULT TPUT backend=%-8s size=%-4zu batch=%zu rounds=%zu ops=%.0f "
           "ns_per_op=%.3f Mops_per_s=%.2f\n",
           BE_NAME, size, batch, rounds, ops, ns_per_op, mops);
    free(v);
    be_shutdown();
}

/* ================================================================== */
/* Mode: mem — measured RSS overhead (#5)                              */
/* ================================================================== */
#if defined(__APPLE__)
  #include <mach/mach.h>
  static size_t rss_bytes(void){
      mach_task_basic_info_data_t info; mach_msg_type_number_t cnt = MACH_TASK_BASIC_INFO_COUNT;
      if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,(task_info_t)&info,&cnt)!=KERN_SUCCESS) return 0;
      return info.resident_size; }
#elif defined(__linux__)
  #include <unistd.h>
  static size_t rss_bytes(void){ FILE* f=fopen("/proc/self/statm","r"); long t=0,r=0; if(f){ if(fscanf(f,"%ld %ld",&t,&r)!=2) r=0; fclose(f);} return (size_t)r*(size_t)sysconf(_SC_PAGESIZE); }
#else
  static size_t rss_bytes(void){ return 0; }
#endif

static void mode_mem(size_t size, size_t count){
    void** v = malloc(count * sizeof(void*));
    if (!v){ fprintf(stderr,"OOM\n"); exit(4); }
    memset(v, 0, count * sizeof(void*));
    /* baseline BEFORE init so a reserve-upfront pool (PMAD) is fully charged */
    size_t rss0 = rss_bytes();
    be_init(size, count);
    size_t got = 0;
    for (size_t i = 0; i < count; i++){ v[i] = be_alloc(size); if (!v[i]) break; got++; }
    for (size_t i = 0; i < got; i++){
        char* c = (char*)v[i];
        for (size_t o = 0; o < size; o += 4096) c[o] = 1;
        c[size-1] = 1;
    }
    size_t rss1 = rss_bytes();
    double requested = (double)got * (double)size;
    double rssd = (double)(rss1 - rss0);
    double overhead_pct = requested > 0 ? 100.0*(rssd - requested)/requested : 0;
    printf("RESULT MEM backend=%-8s size=%-4zu count=%zu got=%zu requested_bytes=%.0f "
           "rss_delta_bytes=%.0f bytes_per_obj=%.2f overhead_pct=%.2f\n",
           BE_NAME, size, count, got, requested, rssd, got? rssd/got:0.0, overhead_pct);
    for (size_t i = 0; i < got; i++) be_free(v[i]);
    free(v);
    be_shutdown();
}

/* ------------------------------------------------------------------ */
static void measure_tsc_ghz(void){
#if defined(__x86_64__)
    struct timespec a,b; uint64_t c0,c1;
    clock_gettime(CLOCK_MONOTONIC,&a); c0 = now_raw();
    struct timespec d; do { clock_gettime(CLOCK_MONOTONIC,&d); }
    while ((d.tv_sec-a.tv_sec)*1e9 + (d.tv_nsec-a.tv_nsec) < 1e8);
    c1 = now_raw(); clock_gettime(CLOCK_MONOTONIC,&b);
    double secs = (b.tv_sec-a.tv_sec) + (b.tv_nsec-a.tv_nsec)/1e9;
    g_tsc_ghz = (double)(c1-c0)/secs/1e9;
    g_ns_per_raw = 1.0 / g_tsc_ghz;
#else
    g_ns_per_raw = 1.0;
#endif
}

int main(int argc, char** argv){
    if (argc < 3){
        fprintf(stderr,
          "usage: %s lat   <size> <iters> [K] [seed]\n"
          "       %s churn <size> <workingset> <iters> [K] [seed]\n"
          "       %s tput  <size> <batch> <rounds>\n"
          "       %s mem   <size> <count>\n", argv[0],argv[0],argv[0],argv[0]);
        return 1;
    }
    pin_self();
    measure_tsc_ghz();
    calibrate_timer();

    fprintf(stderr, "# harness backend=%s timer=\"%s\" ns_per_raw=%.6f timer_floor_raw=%llu\n",
            BE_NAME, TIMER_NAME, g_ns_per_raw, (unsigned long long)g_timer_floor_raw);

    const char* mode = argv[1];
    if (!strcmp(mode,"lat") && argc>=4){
        unsigned K = argc>4 ? (unsigned)strtoul(argv[4],0,10) : 16;
        if (!K) K = 1;
        mode_lat(strtoull(argv[2],0,10), strtoull(argv[3],0,10), K);
    } else if (!strcmp(mode,"churn") && argc>=5){
        unsigned K = argc>5 ? (unsigned)strtoul(argv[5],0,10) : 16;
        if (!K) K = 1;
        mode_churn(strtoull(argv[2],0,10), strtoull(argv[3],0,10),
                   strtoull(argv[4],0,10), K, argc>6?strtoull(argv[6],0,10):0);
    } else if (!strcmp(mode,"tput") && argc>=5){
        mode_tput(strtoull(argv[2],0,10), strtoull(argv[3],0,10), strtoull(argv[4],0,10));
    } else if (!strcmp(mode,"mem") && argc>=4){
        mode_mem(strtoull(argv[2],0,10), strtoull(argv[3],0,10));
    } else {
        fprintf(stderr, "bad mode/args\n"); return 1;
    }
    return 0;
}
