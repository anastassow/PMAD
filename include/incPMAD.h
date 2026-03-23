#ifndef INCPMAD_H
#define INCPMAD_H

#include "PMAD.h"

PmadStatus pmad_init(size_t* class_sizes, size_t* percentages);
void* pmad_alloc(size_t size);
PmadStatus pmad_free(void* ptr);
void pmad_destroy();

#endif
