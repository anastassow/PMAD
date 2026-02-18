#ifndef BLOCK_HEADER_H
#define BLOCK_HEADER_H

#include <stdint.h>

typedef struct BlockHeader {
    struct BlockHeader* next;
    uint8_t size_class;
} BlockHeader;


struct PMAD;
void createBlock(uint8_t* ptr, uint8_t class_index, PMAD* pmad);

#endif
