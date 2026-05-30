#ifndef INCPMAD_H
#define INCPMAD_H

#include "PMAD.h"

#define MAX_PMAD_CLASSES 32

typedef struct {
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t allocated_blocks;
} PmadClassStats;

PmadStatus pmad_init(const size_t* class_sizes, uint8_t num_size_classes,
                     const size_t* percentages, size_t pool_size);
void*      pmad_alloc(size_t size);
PmadStatus pmad_free(void* ptr);
void       pmad_destroy(void);

int pmad_get_stats(PmadClassStats* out, int max_classes);

#endif
