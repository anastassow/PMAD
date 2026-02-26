#include "incPMAD.h"
#include <stdlib.h>
#include <stdio.h>

static PMAD incPMAD_instance;

int pmad_init(size_t* class_sizes, size_t* percentages) {
    PMAD_init(&incPMAD_instance, class_sizes);

    void* pool_mem = get_memory_pool_from_os();

    attach_new_pool(&incPMAD_instance, pool_mem);

    if (split_pool_by_percentage(&incPMAD_instance, percentages)) 
        return 1;
        
    return 0;
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