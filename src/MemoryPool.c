#include "structures/MemoryPool.h"
#include "PMAD.h"

void attach_new_pool(PMAD* pmad, void* mem) {
    MemoryPool* pool = (MemoryPool*)mem;

    pool->start = (uint8_t*)mem + sizeof(MemoryPool);
    pool->size = POOL_SIZE - sizeof(MemoryPool);
    pool->used = 0;

    pool->next = pmad->pool_head;
    pmad->pool_head = pool;
}

void split_pool_by_percentage(struct PMAD* pmad, MemoryPool* pool, size_t percentage[NUM_CLASSES]){
    // TODO: Initialize a pointer to the start of the memory pool

    // TODO: For each size class (i = 0 to NUM_CLASSES-1):
        // TODO: Reset the free list of this size class
        // TODO: Calculate the number of bytes allocated to this size class based on percentages[i]
        // TODO: Calculate how many blocks of this size fit into the allocated bytes
        // TODO: Set the total_blocks and allocated_blocks counters for this size class

        // TODO: For each block in this size class:
            // TODO: Cast the current pool pointer to a BlockHeader
            // TODO: Set the block's size_class index
            // TODO: Push the block to the head of the free list
            // TODO: Advance the pool pointer by the block size

    // TODO: Done — memory pool is now split and free lists are populated
}