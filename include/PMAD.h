#ifndef PMAD_H
#define PMAD_H

#include "structures/MemoryPool.h"
#include "structures/SizeClass.h"

#define NUM_CLASSES 5
#define POOL_SIZE (1024 * 1024)
#define ALIGNMENT 16
#define MAX_SIZE_OF_SIZE_CLASS 4096 // 256 entities max

typedef enum {
    PMAD_OK = 0,
    PMAD_ERR_INIT_FAILED,
    PMAD_ERR_MAP_FAILED,
    PMAD_ERR_INCOMPLETE_PERCENTAGE,
    PMAD_ERR_NULL_PTR,
    PMAD_ERR_INVALID_PTR,
    PMAD_ERR_CORRUPT_HEADER,
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
PmadStatus PMAD_free(PMAD* pmad, void* memoryToFree);

PmadStatus split_pool_by_percentage(struct PMAD* pmad, size_t percentage[NUM_CLASSES]);

#endif