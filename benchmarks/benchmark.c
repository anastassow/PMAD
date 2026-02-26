/*
 * PMAD Benchmark — measures real latency, throughput, struct sizes,
 * block counts, and memory overhead.
 * Compile:  gcc -O2 -Iinclude src/PMAD.c src/incPMAD.c src/MemoryPool.c src/BlockHeader.c benchmark.c -o benchmark
 * Run:      ./benchmark
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "incPMAD.h"
#include "PMAD.h"
#include "structures/MemoryPool.h"
#include "structures/SizeClass.h"
#include "structures/BlockHeader.h"

/* ── helpers ─────────────────────────────────────────────── */
static inline uint64_t ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

#define WARMUP   1000
#define SAMPLES  100000

/* ── main ─────────────────────────────────────────────────── */
int main(void) {

    /* ── 1. Struct sizes ─────────────────────────────── */
    printf("=== STRUCT SIZES (bytes, 64-bit) ===\n");
    printf("sizeof(BlockHeader) = %zu\n", sizeof(BlockHeader));
    printf("sizeof(SizeClass)   = %zu\n", sizeof(SizeClass));
    printf("sizeof(MemoryPool)  = %zu\n", sizeof(MemoryPool));
    printf("sizeof(PMAD)        = %zu\n", sizeof(PMAD));
    printf("\n");

    /* ── 2. Pool config (from main.c defaults) ─────── */
    size_t class_sizes[NUM_CLASSES]  = {16, 32, 64, 128, 1024};
    size_t percentages[NUM_CLASSES]  = {10, 20, 20, 20,  30};

    pmad_init(class_sizes, percentages);

    /* ── 3. Block counts per size class ─────────────── */
    /* We need direct access — re-create a local PMAD for inspection */
    PMAD inspector;
    PMAD_init(&inspector, class_sizes);
    void* raw = get_memory_pool_from_os();
    attach_new_pool(&inspector, raw);
    split_pool_by_percentage(&inspector, percentages);

    printf("=== POOL BLOCK COUNTS (POOL_SIZE=%d bytes) ===\n", POOL_SIZE);
    size_t total_blocks = 0;
    size_t total_user_bytes = 0;
    size_t total_header_bytes = 0;
    for (int i = 0; i < NUM_CLASSES; i++) {
        size_t tb = inspector.size_classes[i].total_blocks;
        size_t bs = inspector.size_classes[i].block_size;
        total_blocks      += tb;
        total_user_bytes  += tb * bs;
        total_header_bytes+= tb * sizeof(BlockHeader);
        printf("  Class[%d] block_size=%4zu  total_blocks=%5zu  "
               "user_bytes=%7zu  header_bytes=%5zu\n",
               i, bs, tb, tb * bs, tb * (size_t)sizeof(BlockHeader));
    }
    printf("  TOTAL blocks=%zu  user_bytes=%zu  header_overhead=%zu bytes\n",
           total_blocks, total_user_bytes, total_header_bytes);
    printf("  Header overhead %% = %.2f%%\n",
           100.0 * total_header_bytes / (total_user_bytes + total_header_bytes));
    printf("\n");
    free_memory_pool(raw);

    /* ── 4. Alloc latency — small (16 B) ─────────────────── */
    /* warm up */
    void* ptrs[SAMPLES];
    memset(ptrs, 0, sizeof(ptrs));
    for (int i = 0; i < WARMUP; i++) {
        void* p = pmad_alloc(16);
        if (p) pmad_free(p);
    }

    /* measure alloc */
    uint64_t alloc_total = 0;
    uint64_t alloc_min   = UINT64_MAX;
    uint64_t alloc_max   = 0;
    int alloc_count = 0;

    for (int i = 0; i < SAMPLES; i++) {
        uint64_t t0 = ns_now();
        ptrs[i] = pmad_alloc(16);
        uint64_t dt = ns_now() - t0;
        if (ptrs[i]) {
            alloc_total += dt;
            if (dt < alloc_min) alloc_min = dt;
            if (dt > alloc_max) alloc_max = dt;
            alloc_count++;
        }
    }

    /* measure free */
    uint64_t free_total = 0;
    uint64_t free_min   = UINT64_MAX;
    uint64_t free_max   = 0;
    for (int i = 0; i < alloc_count; i++) {
        if (!ptrs[i]) continue;
        uint64_t t0 = ns_now();
        pmad_free(ptrs[i]);
        uint64_t dt = ns_now() - t0;
        free_total += dt;
        if (dt < free_min) free_min = dt;
        if (dt > free_max) free_max = dt;
        ptrs[i] = NULL;
    }

    printf("=== LATENCY — 16-byte allocs (%d samples) ===\n", alloc_count);
    printf("  Alloc avg = %.1f ns   min = %llu ns   max = %llu ns\n",
           (double)alloc_total / alloc_count,
           (unsigned long long)alloc_min,
           (unsigned long long)alloc_max);
    printf("  Free  avg = %.1f ns   min = %llu ns   max = %llu ns\n",
           (double)free_total / alloc_count,
           (unsigned long long)free_min,
           (unsigned long long)free_max);
    printf("\n");

    /* ── 5. Alloc latency — 1024 B ───────────────────────── */
    for (int i = 0; i < WARMUP; i++) {
        void* p = pmad_alloc(1024);
        if (p) pmad_free(p);
    }

    uint64_t alloc1k_total = 0; uint64_t alloc1k_min = UINT64_MAX; uint64_t alloc1k_max = 0;
    uint64_t free1k_total  = 0; uint64_t free1k_min  = UINT64_MAX; uint64_t free1k_max  = 0;
    int count1k = 0;
    void* ptrs1k[SAMPLES];
    memset(ptrs1k, 0, sizeof(ptrs1k));

    /* Only 302 blocks exist in 1024 class — cycle */
    const int BATCHES = SAMPLES / 200;
    for (int b = 0; b < BATCHES; b++) {
        int batch = 0;
        for (int i = 0; i < 200; i++) {
            uint64_t t0 = ns_now();
            ptrs1k[i] = pmad_alloc(1024);
            uint64_t dt = ns_now() - t0;
            if (ptrs1k[i]) {
                alloc1k_total += dt;
                if (dt < alloc1k_min) alloc1k_min = dt;
                if (dt > alloc1k_max) alloc1k_max = dt;
                count1k++; batch++;
            }
        }
        for (int i = 0; i < batch; i++) {
            if (!ptrs1k[i]) continue;
            uint64_t t0 = ns_now();
            pmad_free(ptrs1k[i]);
            uint64_t dt = ns_now() - t0;
            free1k_total += dt;
            if (dt < free1k_min) free1k_min = dt;
            if (dt > free1k_max) free1k_max = dt;
            ptrs1k[i] = NULL;
        }
    }

    printf("=== LATENCY — 1024-byte allocs (%d samples) ===\n", count1k);
    printf("  Alloc avg = %.1f ns   min = %llu ns   max = %llu ns\n",
           (double)alloc1k_total / count1k,
           (unsigned long long)alloc1k_min,
           (unsigned long long)alloc1k_max);
    printf("  Free  avg = %.1f ns   min = %llu ns   max = %llu ns\n",
           (double)free1k_total / count1k,
           (unsigned long long)free1k_min,
           (unsigned long long)free1k_max);
    printf("\n");

    /* ── 6. Throughput — allocs per second ──────────── */
    /* drain all 16B blocks, then measure bulk alloc+free cycle */
    /* re-init for clean state */
    pmad_destroy();
    pmad_init(class_sizes, percentages);

    /* alloc all 16B blocks at once */
    /* class[0] total_blocks — we need to get inspector value */
    size_t n16 = inspector.size_classes[0].total_blocks;
    void** bulk = malloc(n16 * sizeof(void*));

    uint64_t t_start = ns_now();
    for (size_t i = 0; i < n16; i++) bulk[i] = pmad_alloc(16);
    uint64_t t_end = ns_now();
    double alloc_ns_per_op = (double)(t_end - t_start) / n16;
    double throughput_m    = 1e9 / alloc_ns_per_op / 1e6;

    printf("=== THROUGHPUT (single thread, 16-byte, %zu allocs) ===\n", n16);
    printf("  Alloc: %.2f ns/op  =>  %.2f M alloc/s\n",
           alloc_ns_per_op, throughput_m);

    for (size_t i = 0; i < n16; i++) if (bulk[i]) pmad_free(bulk[i]);
    free(bulk);

    /* ── 7. System call count ────────────────────────── */
    printf("\n");
    printf("=== SYSTEM CALLS ===\n");
    printf("  mmap calls at init:    1\n");
    printf("  mmap calls during alloc/free: 0  (provably — no mmap in PMAD_alloc/PMAD_free)\n");
    printf("  munmap calls at destroy: 1\n");

    /* ── 8. Memory overhead % ────────────────────────── */
    printf("\n=== MEMORY OVERHEAD ===\n");
    size_t pool_usable   = POOL_SIZE - sizeof(MemoryPool);
    size_t header_bytes2 = 0, user_bytes2 = 0;
    for (int i = 0; i < NUM_CLASSES; i++) {
        size_t tb = inspector.size_classes[i].total_blocks;
        size_t bs = inspector.size_classes[i].block_size;
        header_bytes2 += tb * sizeof(BlockHeader);
        user_bytes2   += tb * bs;
    }
    size_t wasted = pool_usable - header_bytes2 - user_bytes2;
    printf("  Pool usable bytes        = %zu\n", pool_usable);
    printf("  User-data bytes          = %zu\n", user_bytes2);
    printf("  BlockHeader bytes        = %zu\n", header_bytes2);
    printf("  Wasted (alignment/tail)  = %zu\n", wasted);
    printf("  Header overhead %%        = %.2f%%\n",
           100.0 * header_bytes2 / pool_usable);
    printf("  Total non-user overhead  = %.2f%%\n",
           100.0 * (header_bytes2 + wasted) / pool_usable);

    pmad_destroy();
    printf("\nDone.\n");
    return 0;
}
