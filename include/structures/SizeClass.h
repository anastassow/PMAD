#ifndef SIZE_CLASS_H
#define SIZE_CLASS_H

#include "BlockHeader.h"
#include <stddef.h>
#include <stdint.h>

typedef struct SizeClass {
    size_t block_size;
    BlockHeader* free_list;

    uint32_t total_blocks;
    uint32_t allocated_blocks;
} SizeClass;

#endif