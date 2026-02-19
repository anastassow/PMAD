#ifndef PMAD_H
#define PMAD_H

#include "structures/MemoryPool.h"
#include "structures/SizeClass.h"

#define NUM_CLASSES 5
#define POOL_SIZE (1024 * 1024)
#define ALIGNMENT 16
#define MAX_SIZE_OF_SIZE_CLASS 4096 // 256 entities max

typedef struct PMAD{
    MemoryPool* pool_head;
    SizeClass size_classes[NUM_CLASSES];

    int size_class_reference[MAX_SIZE_OF_SIZE_CLASS / ALIGNMENT + 1];
} PMAD;

void init_pmad(PMAD* pmad, const size_t* class_sizes);
void build_lookup_table(PMAD* pmad);

void* get_memory_pool_from_os();
void free_memory_pool(void* mem);

// void* pmad_alloc(PMAD* pmad, size_t size);

#endif