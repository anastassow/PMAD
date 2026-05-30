#include "incPMAD.h"
#include <stdlib.h>
#include <stdio.h>

static PMAD incPMAD_instance;

PmadStatus pmad_init(const size_t* class_sizes, uint8_t num_size_classes,
                     const size_t* percentages, size_t pool_size) {
    incPMAD_instance.pool_head = NULL;

    void* pool_mem = get_memory_pool_from_os(pool_size);
    if (!pool_mem)
        return PMAD_ERR_MAP_FAILED;

    attach_new_pool(&incPMAD_instance, pool_mem, pool_size);

    if (PMAD_init(&incPMAD_instance, class_sizes, num_size_classes, pool_size) != PMAD_OK)
        return PMAD_ERR_INIT_FAILED;

    if (split_pool_by_percentage(&incPMAD_instance, percentages, num_size_classes) != PMAD_OK)
        return PMAD_ERR_INCOMPLETE_PERCENTAGE;

    return PMAD_OK;
}

void* pmad_alloc(size_t size) {
    return PMAD_alloc(&incPMAD_instance, size);
}

PmadStatus pmad_free(void* ptr) {
    return PMAD_free(&incPMAD_instance, ptr);
}

void pmad_destroy(void) {
    MemoryPool* pool = incPMAD_instance.pool_head;
    while (pool) {
        MemoryPool* next = pool->next;
        free_memory_pool(pool, incPMAD_instance.pool_size);
        pool = next;
    }
    incPMAD_instance.size_classes     = NULL;
    incPMAD_instance.pool_head        = NULL;
    incPMAD_instance.num_size_classes = 0;
}

int pmad_get_stats(PmadClassStats* out, int max_classes) {
    int count = 0;
    uint8_t n = incPMAD_instance.num_size_classes;
    int limit = (max_classes < (int)n) ? max_classes : (int)n;
    for (int i = 0; i < limit; i++) {
        out[i].block_size       = (uint32_t)incPMAD_instance.size_classes[i].block_size;
        out[i].total_blocks     = (uint32_t)incPMAD_instance.size_classes[i].total_blocks;
        out[i].allocated_blocks = (uint32_t)incPMAD_instance.size_classes[i].allocated_blocks;
        count++;
    }
    return count;
}