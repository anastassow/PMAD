#include <stdio.h>
#include <stdlib.h>
#include "incPMAD.h"

int main() {
    size_t class_sizes[5] = {16, 32, 64, 128, 256};
    size_t percentages[5] = {10, 20, 20, 20, 30};
    uint8_t num_classes   = 5;
    size_t pool_size      = 1024 * 1024; // 1MB

    if (pmad_init(class_sizes, num_classes, percentages, pool_size) != PMAD_OK) {
        printf("Init failed!\n");
        exit(-1);
    }

    int* something = pmad_alloc(sizeof(int) * 6);
    if (!something) {
        printf("Alloc failed!\n");
        exit(-1);
    }

    int* ptr = something;
    for (int i = 0; i < 6; i++) {
        *ptr = i + 5;
        ptr++;
    }

    ptr = something;
    for (int i = 0; i < 6; i++) {
        printf(" %d", *ptr);
        ptr++;
    }
    printf("\n");

    pmad_free(something);
    pmad_destroy();
    
    return 0;
}