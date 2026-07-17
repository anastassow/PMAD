#ifndef BLOCK_HEADER_H
#define BLOCK_HEADER_H

#include <stdint.h>

/*
Structure for a block's metadata.
Every block sits before usable from user memory.
*/
typedef struct BlockHeader {
    struct BlockHeader* next;   /* pointer to next block */
    uint8_t size_class;         /* index holding which size class it belongs to */
} BlockHeader;

_Static_assert(sizeof(struct BlockHeader) == 16, "Size of BlockHeader is not 16");

#endif
