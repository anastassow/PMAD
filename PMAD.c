#include "MemoryPool.h"
#include "SizeClass.h"
#include "BlockHeader.h"
#include "include/PMAD.h"
#include <stdlib.h>

void init_pmad(PMAD* pmad) {
    size_t class_sizes[NUM_CLASSES] = {16, 32, 64, 128, 256};

    for (int i = 0; i < NUM_CLASSES; i++) {
        pmad->size_classes[i].block_size = class_sizes[i];
        pmad->size_classes[i].free_list = NULL;
        pmad->size_classes[i].total_blocks = 0;
        pmad->size_classes[i].allocated_blocks = 0;
    }

    pmad->pool_head = NULL;
}