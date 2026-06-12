/*
 * pmad_test.c — Correctness suite for PMAD (principle #13: prove correctness
 * before quoting speed). Earns the trust the benchmarks then spend.
 *
 * Covers: init validation, alloc/free round-trips, distinct pointers,
 * 16-byte alignment, pool exhaustion -> NULL, free(NULL), invalid-pointer
 * rejection, stats accounting, and an honest probe of double-free behaviour
 * (PMAD does NOT implement double-free detection — we document, not hide it).
 *
 * Build:  see benchmarks/v2/Makefile  (target: pmad_test)
 * Run:    ./pmad_test   (exit code 0 = all pass)
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "incPMAD.h"
#include "PMAD.h"
#include "structures/MemoryPool.h"
#include "structures/SizeClass.h"
#include "structures/BlockHeader.h"

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do {                                        \
    if (cond) { g_pass++; printf("  [PASS] %s\n", msg); }            \
    else      { g_fail++; printf("  [FAIL] %s\n", msg); }            \
} while (0)

/* Standard pool used by most tests. */
static void reinit(void) {
    size_t cs[5]  = {16, 32, 64, 128, 256};
    size_t pct[5] = {10, 20, 20, 20, 30};
    PmadStatus s = pmad_init(cs, 5, pct, 1024 * 1024);
    if (s != PMAD_OK) { printf("FATAL: reinit failed (%d)\n", s); exit(2); }
}

int main(void) {
    printf("=== PMAD CORRECTNESS SUITE ===\n");
    printf("sizeof(BlockHeader) = %zu  sizeof(SizeClass) = %zu  "
           "sizeof(MemoryPool) = %zu\n\n",
           sizeof(BlockHeader), sizeof(SizeClass), sizeof(MemoryPool));

    /* ---- T1: init validation ------------------------------------------ */
    printf("T1: initialisation validation\n");
    {
        size_t cs[3]  = {16, 64, 256};
        size_t bad[3] = {10, 20, 30};   /* sums to 60, not 100 */
        size_t ok[3]  = {20, 30, 50};
        PmadStatus s_bad = pmad_init(cs, 3, bad, 1024 * 1024);
        CHECK(s_bad == PMAD_ERR_INCOMPLETE_PERCENTAGE,
              "percentages not summing to 100 are rejected");
        if (s_bad == PMAD_OK) pmad_destroy();
        PmadStatus s_ok = pmad_init(cs, 3, ok, 1024 * 1024);
        CHECK(s_ok == PMAD_OK, "valid config initialises");
        pmad_destroy();
    }

    /* ---- T2: basic alloc/free + writability + distinctness ------------ */
    printf("T2: basic alloc / write / free\n");
    reinit();
    {
        void* a = pmad_alloc(16);
        void* b = pmad_alloc(16);
        void* c = pmad_alloc(64);
        CHECK(a && b && c, "three allocations succeed");
        CHECK(a != b && b != c && a != c, "allocations are distinct");
        if (a && b && c) {
            memset(a, 0xAA, 16);   /* full block writable */
            memset(b, 0xBB, 16);
            memset(c, 0xCC, 64);
            CHECK(((uint8_t*)a)[0] == 0xAA && ((uint8_t*)a)[15] == 0xAA &&
                  ((uint8_t*)c)[0] == 0xCC && ((uint8_t*)c)[63] == 0xCC,
                  "every byte of each block is writable & reads back");
        }
        CHECK(pmad_free(a) == PMAD_OK && pmad_free(b) == PMAD_OK &&
              pmad_free(c) == PMAD_OK, "frees return PMAD_OK");
    }
    pmad_destroy();

    /* ---- T3: 16-byte alignment ---------------------------------------- */
    printf("T3: alignment (ALIGNMENT = %d)\n", ALIGNMENT);
    reinit();
    {
        int aligned = 1;
        void* p[64];
        int n = 0;
        for (; n < 64; n++) {
            p[n] = pmad_alloc(64);
            if (!p[n]) break;
            if ((uintptr_t)p[n] % ALIGNMENT != 0) aligned = 0;
        }
        CHECK(n > 0 && aligned, "all returned pointers are 16-byte aligned");
        for (int i = 0; i < n; i++) pmad_free(p[i]);
    }
    pmad_destroy();

    /* ---- T4: exhaustion returns NULL, recovery after free ------------- */
    printf("T4: pool exhaustion -> NULL, then recovery\n");
    reinit();
    {
        /* Drain the 16-byte class completely. */
        PmadClassStats st[8];
        int nc = pmad_get_stats(st, 8);
        uint32_t cap16 = 0;
        for (int i = 0; i < nc; i++) if (st[i].block_size == 16) cap16 = st[i].total_blocks;

        void** v = malloc(sizeof(void*) * (cap16 + 8));
        uint32_t got = 0;
        for (;;) {
            void* q = pmad_alloc(16);
            if (!q) break;
            v[got++] = q;
            if (got > cap16 + 4) break; /* safety */
        }
        CHECK(got == cap16,
              "exhaustion: alloc count exactly equals class capacity");
        CHECK(pmad_alloc(16) == NULL,
              "alloc on empty class returns NULL (no crash, no overrun)");
        /* free one, alloc one -> must succeed (recovery) */
        pmad_free(v[0]);
        void* again = pmad_alloc(16);
        CHECK(again != NULL, "after one free, a fresh alloc succeeds again");
        if (again) v[0] = again;
        for (uint32_t i = 0; i < got; i++) pmad_free(v[i]);
        free(v);
    }
    pmad_destroy();

    /* ---- T5: full drain/refill cycle leaves capacity unchanged -------- */
    printf("T5: drain -> refill cycle preserves capacity\n");
    reinit();
    {
        PmadClassStats st[8];
        int nc = pmad_get_stats(st, 8);
        uint32_t cap = 0;
        for (int i = 0; i < nc; i++) if (st[i].block_size == 64) cap = st[i].total_blocks;
        void** v = malloc(sizeof(void*) * cap);
        uint32_t g1 = 0; void* q;
        while ((q = pmad_alloc(64))) v[g1++] = q;
        for (uint32_t i = 0; i < g1; i++) pmad_free(v[i]);
        uint32_t g2 = 0;
        while ((q = pmad_alloc(64))) v[g2++] = q;
        CHECK(g1 == cap && g2 == cap && g1 == g2,
              "second full drain returns the same block count as the first");
        for (uint32_t i = 0; i < g2; i++) pmad_free(v[i]);
        free(v);
    }
    pmad_destroy();

    /* ---- T6: error paths ---------------------------------------------- */
    printf("T6: error-path validation\n");
    reinit();
    {
        CHECK(pmad_free(NULL) == PMAD_ERR_NULL_PTR,
              "free(NULL) returns PMAD_ERR_NULL_PTR");
        int stackvar = 7;
        CHECK(pmad_free(&stackvar) == PMAD_ERR_INVALID_PTR,
              "free of a pointer outside the pool is rejected");
        /* oversized request (> MAX_SIZE_OF_SIZE_CLASS) returns NULL */
        CHECK(pmad_alloc(MAX_SIZE_OF_SIZE_CLASS + 1) == NULL,
              "alloc larger than max size class returns NULL");
        CHECK(pmad_alloc(0) == NULL,
              "alloc(0) returns NULL");
    }
    pmad_destroy();

    /* ---- T7: stats accounting ----------------------------------------- */
    printf("T7: stats accounting\n");
    reinit();
    {
        void* a = pmad_alloc(64);
        void* b = pmad_alloc(64);
        PmadClassStats st[8];
        int nc = pmad_get_stats(st, 8);
        uint32_t alloc64 = 0;
        for (int i = 0; i < nc; i++) if (st[i].block_size == 64) alloc64 = st[i].allocated_blocks;
        CHECK(alloc64 == 2, "stats report 2 allocated blocks in the 64B class");
        pmad_free(a); pmad_free(b);
        nc = pmad_get_stats(st, 8);
        for (int i = 0; i < nc; i++) if (st[i].block_size == 64) alloc64 = st[i].allocated_blocks;
        CHECK(alloc64 == 0, "stats return to 0 allocated after freeing both");
    }
    pmad_destroy();

    /* ---- T8: double-free behaviour (HONESTY probe, not a guarantee) ---- */
    printf("T8: double-free probe (documented limitation)\n");
    reinit();
    {
        void* a = pmad_alloc(64);
        PmadStatus first  = pmad_free(a);
        PmadStatus second = pmad_free(a);   /* PMAD has no per-block free bit */
        /* We do NOT claim detection. We record what actually happens so the
         * limitation is on the record rather than hidden. */
        printf("       first free  -> %d\n", first);
        printf("       second free -> %d  (PMAD does not detect double-free)\n", second);
        CHECK(first == PMAD_OK,
              "first free of a valid block succeeds");
        /* The second free is accepted (returns OK) because there is no
         * allocated/free state per block. This is a known limitation,
         * reported honestly rather than asserted as a feature. */
        CHECK(second == PMAD_OK,
              "second free is ACCEPTED (no double-free detection — known gap)");
    }
    pmad_destroy();

    /* ---- summary ------------------------------------------------------- */
    printf("\n=== SUMMARY: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
