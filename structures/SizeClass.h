#ifndef SIZE_CLASS_H
#define SIZR_CLASS_H

#include "BlockHeader.h"
#include <stddef.h>

typedef struct SizeClass {
    size_t block_size;
    BlockHeader* free_list;

    size_t total_blocks;
    size_t allocated_blocks;
} SizeClass;

#endif