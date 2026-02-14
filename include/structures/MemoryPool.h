#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stdlib.h>
#include <stdio.h>
#include "PMAD.h"

typedef struct MemoryPool {
    void* start;
    size_t size;
    size_t used;
    struct MemoryPool* next;
} MemoryPool;

void attach_new_pool(PMAD* pmad, void* mem);

#endif