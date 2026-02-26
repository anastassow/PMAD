#include <stdlib.h>
#include "incPMAD.h"

int main() {

size_t class_sizes[NUM_CLASSES] = {16, 32, 64, 128, 256};
size_t percentages[5] = {10, 20, 20, 20, 30};
if (!pmad_init(class_sizes, percentages)){
    printf("Not rounded to 100 percent!\n");
    exit(-1);
}
int* something = pmad_alloc(sizeof(int) * 6);

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