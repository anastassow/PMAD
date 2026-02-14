#include <stdlib.h>
#include "PMAD.h"

int main() {

PMAD pmad;
init_pmad(&pmad);
printf("Hello, World everything is done!\n");

void* memoryPool = get_memory_pool_from_os();
attach_new_pool(&pmad, memoryPool);

free_memory_pool(memoryPool);

return 0;
}