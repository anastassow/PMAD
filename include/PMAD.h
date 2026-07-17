#ifndef PMAD_H
#define PMAD_H

#include "./structures/BlockHeader.h"
#include "./structures/MemoryPool.h"
#include "./structures/SizeClass.h"
#include <stdint.h>

#define ROUND_UP(size, alignment) \
    (((size) + (alignment) - 1) & ~(size_t)((alignment) - 1))

#define ALIGNMENT 16
#define MAX_SIZE_OF_SIZE_CLASS 1024
#define MAX_NUMBER_OF_SIZE_CLASSES (MAX_SIZE_OF_SIZE_CLASS / ALIGNMENT)

/* enum for pmad codes */
typedef enum {
  PMAD_OK = 0x00,
  PMAD_ERR_INIT_FAILED = 0x01,
  PMAD_ERR_MAP_FAILED = 0x02,
  PMAD_ERR_INCOMPLETE_PERCENTAGE = 0x03,
  PMAD_ERR_NULL_PTR = 0x04,
  PMAD_ERR_INVALID_PTR = 0x05,
  PMAD_ERR_CORRUPT_HEADER = 0x06,
  PMAD_ERR_OOM = 0x07,
  PMAD_ERR_TOO_MANY_SIZE_CLASSES = 0x08,
  PMAD_ERR_DEALLOCATING_MMAP = 0x09,
  PMAD_ERR_ALLOCATION_OF_0_NOT_ALLOWED = 0x0A,
  PMAD_ERR_ALLOCATION_TOO_BIG = 0x0B,
  PMAD_ERR_OUT_OF_MEMORY = 0x0C
} PmadStatus;

/*
Structure for the allocator. Holds everything about the allocator.
*/
typedef struct PMAD {
  MemoryPool *pool_head;      /* pointer to the first pool */
  SizeClass *size_classes;    /* pointer to the first size class */
  uint8_t num_size_classes;   /* number of size classes */
  size_t pool_size;           /* size of pool */

  /* size class look up table*/
  int8_t size_class_reference[MAX_NUMBER_OF_SIZE_CLASSES + 1];
} PMAD;

/* 
Function to init PMAD with defaults and give memory 
for the size classes from the global pool,
placing them in from of the pool as matadata.
*/
PmadStatus PMAD_init(PMAD *pmad, const size_t *class_sizes,
                     uint8_t num_size_classes, size_t pool_size);

/*
Function to build the look up table for giving a size
aligned with ALIGNMENT when a pmad_alloc is called.
The table saves a index for each size that someone could
allocate and that index is the size class it should be given to.
The arry's max is MAX_NUMBER_OF_SIZE_CLASSES and that is when the user
defines all size classes up to MAX_SIZE_OF_SIZE_CLASS aligned by ALIGNMENT.
*/
PmadStatus build_lookup_table(PMAD *pmad);

/*
Function that requests memory from os. 
Returns NULL on fail and the pointer to memory on succsess.
*/
void *get_memory_pool_from_os(size_t pool_size);

/*
Frees the memory requested from the os.
*/
PmadStatus free_memory_pool(void *mem, size_t pool_size);

/*
Function to allocate from PMAD.
Uses look up table.
Returns PMAD_OK when allocateed and fills allocated_block_pointer with the pointer.
Returns error status and fills allocated_block_pointer with NULL.
*/
PmadStatus PMAD_alloc(PMAD *pmad, size_t size, void** allocated_block_pointer);

/* 
Frees pmad allocated blocks.
If pointer is not comming from pmad it retuens a error status.
If header is corrupted it returns error status.
*/
PmadStatus PMAD_free(PMAD *pmad, void *memoryToFree);

/*
Function to split the memory pool into the free lists of the size classes.
Now PMAD has blocks ready for allocation.
*/
PmadStatus split_pool_by_percentage(PMAD *pmad, const size_t *percentage,
                                    uint8_t num_classes);

/*
Function for creating a block from a size class.
Metadata and then user usable memory.
*/
void createBlock(uint8_t *ptr, uint8_t class_index, PMAD *pmad);

/*
Function to attach a new memory pool in pmad.
*/
void attach_new_pool(PMAD *pmad, void *mem, size_t pool_size);

#endif