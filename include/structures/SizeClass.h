#ifndef SIZE_CLASS_H
#define SIZE_CLASS_H

#include "./BlockHeader.h"
#include <stddef.h>
#include <stdint.h>

/*
Structure holding a free list of free blocks with the same bytes size.
*/
typedef struct SizeClass {
    size_t block_size;          /* size of the block that it is holding */
    BlockHeader* free_list;     /* ponter to the start of the free list */

    uint32_t total_blocks;      /* total blocks in the list */
    uint32_t allocated_blocks;  /* total allocated blocks from the list */
} SizeClass;

_Static_assert(sizeof(struct SizeClass) == 24, "Size of SizeClass is not 24");

#endif