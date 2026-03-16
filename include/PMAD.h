#ifndef PMAD_H
#define PMAD_H

#include "structures/MemoryPool.h"
#include "structures/SizeClass.h"

#define NUM_CLASSES 5
#define POOL_SIZE (1024 * 1024)
#define ALIGNMENT 16
#define MAX_SIZE_OF_SIZE_CLASS 4096 // 256 entities max

typedef enum {
    PMAD_STATUS_ALLOCATED = 0,
    PMAD_STATUS_INITIALIZED = 1,
    PMAD_STATUS_INIT_FAILED = 2,
    PMAD_STATUS_MAP_FAILED = 3,
    PMAD_STATUS_INCOMPLETE_PERCENTAGE = 4,
    PMAD_STATUS_READY = 5
} PmadStatus;

typedef struct PMAD{
    MemoryPool* pool_head;
    SizeClass size_classes[NUM_CLASSES];

    int size_class_reference[MAX_SIZE_OF_SIZE_CLASS / ALIGNMENT + 1];
} PMAD;

PmadStatus PMAD_init(PMAD* pmad, const size_t* class_sizes);
void build_lookup_table(PMAD* pmad);

void* get_memory_pool_from_os();
void free_memory_pool(void* mem);

void* PMAD_alloc(PMAD* pmad, size_t size);
void PMAD_free(PMAD* pmad, void* memoryToFree);

#endif