#include "PMAD.h"

void createBlock(uint8_t* ptr, uint8_t class_index, PMAD* pmad) {
    BlockHeader* block = (BlockHeader*)ptr;
    block->size_class = class_index;

    block->next = pmad->size_classes[class_index].free_list;
    pmad->size_classes[class_index].free_list = block;
}