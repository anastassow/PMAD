#include "PMAD.h"

#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

void build_lookup_table(PMAD* pmad) {
    int class_index = 0;
    for (int i = 1; i <= MAX_SIZE_OF_SIZE_CLASS / ALIGNMENT; i++) {
        size_t aligned_size = i * ALIGNMENT;

        while (class_index < NUM_CLASSES && pmad->size_classes[class_index].block_size < aligned_size)
            class_index++;

        pmad->size_class_reference[i] = (class_index < NUM_CLASSES) ? class_index : -1;
    }
}

PmadStatus PMAD_init(PMAD* pmad, const size_t* class_sizes) {
    for (size_t i = 0; i < NUM_CLASSES; i++) {
        pmad->size_classes[i].block_size = class_sizes[i];
        pmad->size_classes[i].free_list = NULL;
        pmad->size_classes[i].total_blocks = 0;
        pmad->size_classes[i].allocated_blocks = 0;
    }

    pmad->pool_head = NULL;
    build_lookup_table(pmad);

    return PMAD_OK;
}

void* get_memory_pool_from_os() {
    void* mem = mmap(
        NULL,
        POOL_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_ANON | MAP_PRIVATE,
        -1, 0
    );

    if (mem == MAP_FAILED) {
        return NULL;
    }

    return mem;
}

void free_memory_pool(void* mem) {
    if (munmap(mem, POOL_SIZE) != 0) {
        perror("munmap failed");
    }
}

PmadStatus split_pool_by_percentage(struct PMAD* pmad, size_t percentage[NUM_CLASSES]){
    size_t sumOfPercentages = 0;
    for (size_t i = 0; i < NUM_CLASSES; i++) {
        sumOfPercentages += percentage[i];
    }

    if (sumOfPercentages != 100)
        return PMAD_ERR_INCOMPLETE_PERCENTAGE;

    uint8_t* ptr = (uint8_t*)pmad->pool_head->start;

    for (uint8_t i = 0; i < NUM_CLASSES; i++) {
        size_t user_block_size = pmad->size_classes[i].block_size;
        size_t block_size = user_block_size + sizeof(BlockHeader);     
        
        size_t class_size = (pmad->pool_head->size * percentage[i]) / 100;
        size_t blocks_fit = class_size / block_size;

        for (int j = 0; j < blocks_fit; j++) {
            createBlock(ptr, i, pmad);
            pmad->size_classes[i].total_blocks++;   
            ptr += block_size;
        }
    }

    return PMAD_OK;
}

size_t roundUp(size_t size) {
    return (size + ALIGNMENT -1) & ~(ALIGNMENT -1);
}

void* PMAD_alloc(PMAD* pmad, size_t size) {
    
    size_t aligned = roundUp(size);
    if (aligned > MAX_SIZE_OF_SIZE_CLASS) return NULL;
    int index = pmad->size_class_reference[aligned / ALIGNMENT];
    if (index < 0) return NULL;

    SizeClass* sc = &pmad->size_classes[index];

    void* memory = sc->free_list;
    if (!memory) {
        return NULL;
    }

    sc->free_list = sc->free_list->next;
    sc->allocated_blocks++;

    return (void*)((uint8_t*)memory + sizeof(BlockHeader));
}

static int pointer_in_pool(PMAD* pmad, void* ptr) {
    if (!pmad->pool_head) return 0;
    uint8_t* start = pmad->pool_head->start;
    uint8_t* end = start + pmad->pool_head->size;
    return (uint8_t*)ptr >= start && (uint8_t*)ptr < end;
}

PmadStatus PMAD_free(PMAD* pmad, void* memoryToFree) {
    if (!memoryToFree) return PMAD_ERR_NULL_PTR;
    
    BlockHeader* block = (BlockHeader*)((uint8_t*)memoryToFree - sizeof(BlockHeader));

    if (!pointer_in_pool(pmad, block)) return PMAD_ERR_INVALID_PTR;
    if (block->size_class >= NUM_CLASSES) return PMAD_ERR_CORRUPT_HEADER;

    SizeClass* sc = &pmad->size_classes[block->size_class];
    block->next = sc->free_list;
    sc->free_list = block;
    sc->allocated_blocks--;

    return PMAD_OK;
}
