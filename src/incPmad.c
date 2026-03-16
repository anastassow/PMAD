#include "incPMAD.h"
#include <stdlib.h>
#include <stdio.h>

static PMAD incPMAD_instance;

PmadStatus pmad_init(size_t* class_sizes, size_t* percentages) {
    if (PMAD_init(&incPMAD_instance, class_sizes) != PMAD_STATUS_READY)
        return PMAD_STATUS_INIT_FAILED;

    void* pool_mem = get_memory_pool_from_os();
    if (!pool_mem)
        return PMAD_STATUS_MAP_FAILED;

    attach_new_pool(&incPMAD_instance, pool_mem);

    if (split_pool_by_percentage(&incPMAD_instance, percentages))
        return PMAD_STATUS_INCOMPLETE_PERCENTAGE;
        
    return PMAD_STATUS_INITIALIZED;
}

void* pmad_alloc(size_t size) {
    return PMAD_alloc(&incPMAD_instance, size);
}

void pmad_free(void* ptr) {
    PMAD_free(&incPMAD_instance, ptr);
}

void pmad_destroy() {
    MemoryPool* pool = incPMAD_instance.pool_head;
    while (pool) {
            MemoryPool* next = pool->next;
            free_memory_pool(pool);
            pool = next;
    }
    incPMAD_instance.pool_head = NULL;
}