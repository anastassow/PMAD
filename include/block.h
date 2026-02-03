#ifndef BLOCK_H
#define BLOCK_H

#include <stddef.h>
#include <stdbool.h>

typedef struct Block {
    size_t size;
    bool free;
    struct Block* next;
} Block;

#endif