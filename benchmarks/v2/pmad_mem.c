/*
 * pmad_mem.c — PMAD's exact metadata overhead by block size (Bench 5).
 *
 * PMAD's per-block overhead is deterministic: a fixed 16-byte BlockHeader.
 * This tool reports the REAL, tested numbers — it inits PMAD with a single
 * size class taking 100%% of a fixed pool, then reads the actual block count
 * back from pmad_get_stats() and computes the header overhead from it.
 *
 * Header overhead %% = header / (block_size + header).  Honest tradeoff:
 * costly at 16 B, negligible at 4096 B (principle: present, don't hide it).
 */
#include <stdio.h>
#include <stdint.h>
#include "incPMAD.h"
#include "PMAD.h"
#include "structures/BlockHeader.h"

int main(void){
    const size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    const size_t nsizes = sizeof(sizes)/sizeof(sizes[0]);
    const size_t pool = 64u * 1024u * 1024u;   /* 64 MB pool */
    const size_t hdr = sizeof(BlockHeader);

    printf("# PMAD metadata overhead — header = %zu bytes, pool = %zu bytes\n", hdr, pool);
    printf("size_B,header_B,total_blocks,user_bytes,header_bytes,header_overhead_pct\n");

    for (size_t i = 0; i < nsizes; i++){
        size_t cs[1]  = { sizes[i] };
        size_t pct[1] = { 100 };
        if (pmad_init(cs, 1, pct, pool) != PMAD_OK){
            fprintf(stderr, "init failed for size %zu\n", sizes[i]);
            return 1;
        }
        PmadClassStats st[1];
        pmad_get_stats(st, 1);
        uint32_t blocks = st[0].total_blocks;
        double user_bytes   = (double)blocks * (double)sizes[i];
        double header_bytes = (double)blocks * (double)hdr;
        double ov = 100.0 * (double)hdr / ((double)sizes[i] + (double)hdr);
        printf("%zu,%zu,%u,%.0f,%.0f,%.4f\n",
               sizes[i], hdr, blocks, user_bytes, header_bytes, ov);
        pmad_destroy();
    }
    return 0;
}
