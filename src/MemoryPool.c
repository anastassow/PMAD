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

void split_pool_by_percentage(struct PMAD* pmad, size_t percentage[NUM_CLASSES]){
    uint8_t* ptr = (uint8_t*)pmad->pool_head->start;

    for (uint8_t i = 0; i < NUM_CLASSES; i++) {
        size_t block_size = pmad->size_classes[i].block_size;
        
        size_t class_size = (pmad->pool_head->size * percentage[i]) / 100;
        size_t blocks_fit = class_size / block_size;

        for (int j = 0; j < blocks_fit; j++) {
            createBlock(ptr, i, pmad);
            pmad->size_classes[i].total_blocks++;
            ptr += block_size;
        }
    }
}