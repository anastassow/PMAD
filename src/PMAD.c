#include "PMAD.h"

#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PmadStatus build_lookup_table(PMAD* pmad) {
    if ((size_t)pmad->num_size_classes > (size_t)MAX_NUMBER_OF_SIZE_CLASSES) 
        return PMAD_ERR_TOO_MANY_SIZE_CLASSES;
   
    uint8_t class_index = 0;
    for (uint16_t i = 1; i <= MAX_NUMBER_OF_SIZE_CLASSES; i++) {
        size_t aligned_size = (size_t)i * ALIGNMENT;

        while (class_index < pmad->num_size_classes &&
               pmad->size_classes[class_index].block_size < aligned_size)
            class_index++;

        pmad->size_class_reference[i] = (class_index < pmad->num_size_classes)
                                        ? (int8_t)class_index
                                        : -1;
    }
    pmad->size_class_reference[0] = -1;

    return PMAD_OK;
}

PmadStatus PMAD_init(PMAD* pmad, const size_t* class_sizes,
                     uint8_t num_size_classes, size_t pool_size) {
    if (!pmad->pool_head) 
        return PMAD_ERR_INIT_FAILED;

    size_t raw_size = (size_t)num_size_classes * sizeof(SizeClass);
    size_t size_classes_bytes = ROUND_UP(raw_size, ALIGNMENT);
    
    if (pmad->pool_head->size < size_classes_bytes)
        return PMAD_ERR_OOM;

    pmad->size_classes = (SizeClass*)pmad->pool_head->start;
    pmad->pool_head->start += size_classes_bytes;
    pmad->pool_head->size  -= size_classes_bytes;

    pmad->num_size_classes = num_size_classes;
    pmad->pool_size        = pool_size;

    for (uint8_t i = 0; i < num_size_classes; i++) {
        pmad->size_classes[i].block_size       = class_sizes[i];
        pmad->size_classes[i].free_list        = NULL;
        pmad->size_classes[i].total_blocks     = 0;
        pmad->size_classes[i].allocated_blocks = 0;
    }

    pmad->pool_head->used += size_classes_bytes;

    PmadStatus build_table_status = build_lookup_table(pmad);
    return build_table_status;
}

void* get_memory_pool_from_os(size_t pool_size) {
    void* mem = mmap(
        NULL,
        pool_size,
        PROT_READ | PROT_WRITE,
        MAP_ANON | MAP_PRIVATE,
        -1, 0
    );
    return (mem == MAP_FAILED) ? NULL : mem;
}

PmadStatus free_memory_pool(void* mem, size_t pool_size) {
    if (munmap(mem, pool_size) != 0)
        return PMAD_ERR_DEALLOCATING_MMAP;
    return PMAD_OK;
}

PmadStatus split_pool_by_percentage(PMAD* pmad, const size_t* percentage,
                                    uint8_t num_classes) {
    size_t sumOfPercentages = 0;
    for (uint8_t i = 0; i < num_classes; i++)
        sumOfPercentages += percentage[i];

    if (sumOfPercentages != 100)
        return PMAD_ERR_INCOMPLETE_PERCENTAGE;

    uint8_t* ptr = (uint8_t*)pmad->pool_head->start;

    for (uint8_t i = 0; i < num_classes; i++) {
        size_t user_block_size = pmad->size_classes[i].block_size;
        size_t block_size      = user_block_size + sizeof(BlockHeader);

        size_t class_size = (pmad->pool_head->size * percentage[i]) / 100;
        size_t blocks_fit = class_size / block_size;

        for (size_t j = 0; j < blocks_fit; j++) {
            createBlock(ptr, i, pmad);
            pmad->size_classes[i].total_blocks++;
            ptr += block_size;
        }

        pmad->pool_head->used += pmad->size_classes[i].total_blocks * sizeof(BlockHeader);
    }

    return PMAD_OK;
}

PmadStatus PMAD_alloc(PMAD* pmad, size_t size, void** allocated_block_pointer) {
    size_t aligned = ROUND_UP(size, ALIGNMENT);
    if (aligned == 0 || aligned > MAX_SIZE_OF_SIZE_CLASS) {
        *allocated_block_pointer = NULL;
        return PMAD_ERR_ALLOCATION_TOO_BIG;
    }

    int8_t index = pmad->size_class_reference[aligned / ALIGNMENT];
    if (index < 0) {
        *allocated_block_pointer = NULL;
        return PMAD_ERR_ALLOCATION_OF_0_NOT_ALLOWED;
    }

    SizeClass* sc = &pmad->size_classes[(uint8_t)index];
    if (!sc->free_list) {
        *allocated_block_pointer = NULL;
        return PMAD_ERR_OUT_OF_MEMORY;
    }

    BlockHeader* block = sc->free_list;
    sc->free_list = block->next;
    sc->allocated_blocks++;

    pmad->pool_head->used += aligned;

    *allocated_block_pointer = (uint8_t*)block + sizeof(BlockHeader);
    return PMAD_OK;
}

/*
Function to check if the pointer given to pmad_free is allocated from PMAD
*/
static int pointer_in_pool(PMAD* pmad, const void* ptr) {
    for (MemoryPool* p = pmad->pool_head; p; p = p->next) {
        const uint8_t* start = (const uint8_t*)p->start;
        const uint8_t* end   = start + p->size;
        if ((const uint8_t*)ptr >= start && (const uint8_t*)ptr < end)
            return 1;
    }
    return 0;
}

PmadStatus PMAD_free(PMAD* pmad, void* memoryToFree) {
    if (!memoryToFree) return PMAD_ERR_NULL_PTR;

    BlockHeader* block = (BlockHeader*)((uint8_t*)memoryToFree - sizeof(BlockHeader));

    if (!pointer_in_pool(pmad, block)) return PMAD_ERR_INVALID_PTR;
    if (block->size_class >= pmad->num_size_classes) return PMAD_ERR_CORRUPT_HEADER;

    SizeClass* sc = &pmad->size_classes[block->size_class];
    block->next   = sc->free_list;
    sc->free_list = block;
    sc->allocated_blocks--;

    pmad->pool_head->used -= pmad->size_classes[block->size_class].block_size;

    return PMAD_OK;
}
