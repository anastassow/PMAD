#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stdio.h>

typedef struct MemoryPool {
    void* start;
    size_t size;
    size_t used;
    struct MemoryPool* next;
} MemoryPool;

#endif