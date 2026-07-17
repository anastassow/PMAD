#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stdio.h>

/*
Structure for a pool of memory, used by the allocator.
*/
typedef struct MemoryPool {
    void* start;                /* pointer to the star of the memory pool */
    size_t size;                /* size of the memory pool */
    size_t used;                /* counter for used memory (CURRENTLY NOT USED!!!) */
    struct MemoryPool* next;    /* pointer to the next pool (if there is one) */
} MemoryPool;

_Static_assert(sizeof(struct MemoryPool) == 32, "Size of MemoryPool is not 32");

#endif