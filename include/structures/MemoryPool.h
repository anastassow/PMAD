#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stdio.h>

typedef struct MemoryPool {
    void* start;
    size_t size;
    size_t used;
    struct MemoryPool* next;
} MemoryPool;

typedef struct PMAD PMAD;
void attach_new_pool(struct PMAD* pmad, void* mem, size_t pool_size);

#endif