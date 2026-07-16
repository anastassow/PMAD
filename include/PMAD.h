#ifndef PMAD_H
#define PMAD_H

#include "./structures/BlockHeader.h"
#include "./structures/MemoryPool.h"
#include "./structures/SizeClass.h"
#include <stdint.h>

#define ALIGNMENT 16
#define MAX_SIZE_OF_SIZE_CLASS 4096 /* 256 slots max */

typedef enum {
  PMAD_OK = 0,
  PMAD_ERR_INIT_FAILED,
  PMAD_ERR_MAP_FAILED,
  PMAD_ERR_INCOMPLETE_PERCENTAGE,
  PMAD_ERR_NULL_PTR,
  PMAD_ERR_INVALID_PTR,
  PMAD_ERR_CORRUPT_HEADER,
  PMAD_ERR_OOM,
} PmadStatus;

typedef struct PMAD {
  MemoryPool *pool_head;
  SizeClass *size_classes;
  uint8_t num_size_classes;
  size_t pool_size;

  int8_t size_class_reference[MAX_SIZE_OF_SIZE_CLASS / ALIGNMENT + 1];
} PMAD;

PmadStatus PMAD_init(PMAD *pmad, const size_t *class_sizes,
                     uint8_t num_size_classes, size_t pool_size);
void build_lookup_table(PMAD *pmad);

void *get_memory_pool_from_os(size_t pool_size);
void free_memory_pool(void *mem, size_t pool_size);

void *PMAD_alloc(PMAD *pmad, size_t size);
PmadStatus PMAD_free(PMAD *pmad, void *memoryToFree);

PmadStatus split_pool_by_percentage(PMAD *pmad, const size_t *percentage,
                                    uint8_t num_classes);

void createBlock(uint8_t *ptr, uint8_t class_index, PMAD *pmad);
void attach_new_pool(PMAD *pmad, void *mem, size_t pool_size);

#endif