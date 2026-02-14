#include "MemoryPool.h"
#include "PMAD.h"

void attach_new_pool(PMAD* pmad, void* mem) {
    MemoryPool* pool = (MemoryPool*)mem;

    pool->start = (uint8_t*)mem + sizeof(MemoryPool);
    pool->size = POOL_SIZE - sizeof(MemoryPool);
    pool->used = 0;

    pool->next = pmad->pool_head;
    pmad->pool_head = pool;
}