/**
 * Heap manager like LwIP 
 * Modified HEAP_SIZE according to your APP
 *
 * Nicholas3388
 */

#ifndef _EASY_HEAP_H_
#define _EASY_HEAP_H_

#include "c_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mem_heap {
  uint16_t prev;
  uint16_t next;
  uint8_t used;
  uint8_t pad[3];
};

#define HEAP_SIZE         1024
#define HEAP_ALIGNMENT    4
#define MIN_SIZE_ALIGNED  8

#define heap_align(heap)  (   \
        (void *)((uint32_t)(heap+HEAP_ALIGNMENT-1) & ~(uint32_t)(HEAP_ALIGNMENT-1))  \
)

#define skip_heap_head(c)  ((uint8_t *)c + sizeof(struct mem_heap))
#define get_heap_head(addr)    ((struct mem_heap *)((uint8_t *)addr-sizeof(struct mem_heap)))


void easy_heap_init();
void *easy_heap_alloc(uint16_t size);
void easy_heap_free(void *heap);
void *easy_heap_realloc(void *heap, uint16_t size);
void easy_heap_statistics(void);

#ifdef __cplusplus
}
#endif

#endif // _EASY_HEAP_H_
