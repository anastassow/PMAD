#include "MemoryPool.h"
#include "SizeClass.h"
#include "BlockHeader.h"
#include <stdlib.h>

#define NUM_CLASSES 5

MemoryPool* pool_head;
SizeClass size_classes[NUM_CLASSES];

void init_pmad() {
    size_t class_sizes[NUM_CLASSES] = {16, 32, 64, 128, 256};

    for (int i = 0; i < NUM_CLASSES; i++) {
        size_classes[i].block_size = class_sizes[i];
        size_classes[i].free_list = NULL;
        size_classes[i].total_blocks = 0;
        size_classes[i].allocated_blocks = 0;
    }

    pool_head = NULL;
}