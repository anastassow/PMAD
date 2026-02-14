#include "include/PMAD.h"

#include <sys/mman.h>
#include <stdio.h>
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

void free_memory_pool(void* mem){
    if (munmap(mem, POOL_SIZE) != 0) {
        perror("munmap failed");
    }

    printf("Memmory freed!");
}
