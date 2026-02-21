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

void init_pmad(PMAD* pmad, const size_t* class_sizes) {

    for (size_t i = 0; i < NUM_CLASSES; i++) {
        pmad->size_classes[i].block_size = class_sizes[i];
        pmad->size_classes[i].free_list = NULL;
        pmad->size_classes[i].total_blocks = 0;
        pmad->size_classes[i].allocated_blocks = 0;
    }

    pmad->pool_head = NULL;
    build_lookup_table(pmad);
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
        perror("mmap failed");
        exit(1);
    }

    printf("Memmory allocated successfully\n");
    return mem;
}

void free_memory_pool(void* mem) {
    if (munmap(mem, POOL_SIZE) != 0) {
        perror("munmap failed");
    }

    printf("Memmory freed!");
}

size_t roundUp(size_t size) {
    return (size + 15) & ~((size_t)15);
}

void* pmad_alloc(PMAD* pmad, size_t size) {
    
    size_t aligned = roundUp(size);
    int index = pmad->size_class_reference[aligned / ALIGNMENT];
    
    void* memory = pmad->size_classes[index].free_list;
    pmad->size_classes[index].free_list = pmad->size_classes[index].free_list->next;
    pmad->size_classes[index].allocated_blocks++;
    pmad->size_classes[index].total_blocks--;

    return (uint8_t*)memory + sizeof(BlockHeader);
}