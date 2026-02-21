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

int* something = pmad_alloc(&pmad, sizeof(int));
printf("This is the header: %hhu\n", ((BlockHeader*)something)->size_class);
printf("This is the header: %p\n", ((BlockHeader*)something)->next);

*something = 42;
printf("This is the value: %d\n", *something);

free_memory_pool(memoryPool);

return 0;
}