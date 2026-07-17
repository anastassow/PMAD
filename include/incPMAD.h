#ifndef INCPMAD_H
#define INCPMAD_H

#include "./PMAD.h"

#define MAX_PMAD_CLASSES 32

/*
Structure for stats lookup.
*/
typedef struct {
    uint32_t block_size;        /* block size in bytes */
    uint32_t total_blocks;      /* total blocks */
    uint32_t allocated_blocks;  /* total allocated blocks*/
} PmadClassStats;

_Static_assert(sizeof(PmadClassStats) == 12, "Size of PmadClassStats is not 12");

/*
Function that initializes PMAD and makes it fully prepared for allocations.
Calls get_memory_pool_from_os, attach_new_pool, PMAD_init and split_pool_by_percentage.
*/
PmadStatus pmad_init(const size_t* class_sizes, uint8_t num_size_classes,
                     const size_t* percentages, size_t pool_size);

/*
Wrapper function of PMAD_alloc.
Includes error handling and make the interface better.
*/
void* pmad_alloc(size_t size);

/*
Wrapper function of PMAD_free in order PMAD to be with a good interface.
*/
PmadStatus pmad_free(void* ptr);

/*
Function that frees all of the memory in the pools.
*/
PmadStatus pmad_destroy(void);

/*
Function that fills out a out with stats from PMAD.
*/
int pmad_get_stats(PmadClassStats* out, int max_classes);

#endif
