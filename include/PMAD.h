#ifndef PMAD_H
#define PMAD_H

#include "structures/MemoryPool.h"
#include "structures/SizeClass.h"

#define NUM_CLASSES 5
#define POOL_SIZE (1024 * 1024)

typedef struct {
    MemoryPool* pool_head;
    SizeClass size_classes[NUM_CLASSES];
} PMAD;

void init_pmad(PMAD* pmad);

void* get_memory_pool_from_os();
void free_memory_pool(void* mem);

#endif