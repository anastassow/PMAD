#ifndef PMAD_H
#define PMAD_H

#include "structures/MemoryPool.h"
#include "structures/SizeClass.h"

#define NUM_CLASSES 5

typedef struct {
    MemoryPool* pool_head;
    SizeClass size_classes[NUM_CLASSES];
} PMAD;

void init_pmad(PMAD* pmad);

#endif