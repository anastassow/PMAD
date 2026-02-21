#include <stdlib.h>
#include "incPMAD.h"

int main() {

size_t class_sizes[NUM_CLASSES] = {16, 32, 64, 128, 1024};
size_t percentages[5] = {10, 20, 20, 20, 30};
pmad_init(class_sizes, percentages);
int* something = pmad_alloc(sizeof(int) * 6);
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

pmad_free(something);

pmad_destroy();

return 0;
}