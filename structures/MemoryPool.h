#ifndef MEMORY_POOP_H
#define MEMORY_POOP_H

#include <stdlib.h>
#include <stdio.h>

typedef struct MemoryPool {
    void* start;
    size_t size;
    size_t used;
    struct MemoryPool* next;
} MemoryPool;

#endif