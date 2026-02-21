#include <stdlib.h>
#include "PMAD.h"

int main() {

PMAD pmad;
size_t class_sizes[NUM_CLASSES] = {16, 32, 64, 128, 1024};
init_pmad(&pmad, class_sizes);
printf("Hello, World everything is done!\n");

void* memoryPool = get_memory_pool_from_os();
attach_new_pool(&pmad, memoryPool);

size_t percentages[5] = {10, 20, 20, 20, 30};
split_pool_by_percentage(&pmad, percentages);
printf("Here\n");

int* something = pmad_alloc(&pmad, sizeof(int) * 6);
printf("This is the header's size_class: %hhu\n", ((BlockHeader*)(((uint8_t*)something) - sizeof(BlockHeader)))->size_class);
printf("This is the header's next: %p\n", ((BlockHeader*)(((uint8_t*)something) - sizeof(BlockHeader)))->next);

int* ptr = something;
for (int i = 0; i < 6; i++) {
    *ptr = i + 5;
    ptr++;
}

ptr = something;
for (int i = 0; i < 6; i++) {
    printf(" %d,", *ptr);
    ptr++;
}
printf("\n");

pmad_free(&pmad, something);

free_memory_pool(memoryPool);

return 0;
}