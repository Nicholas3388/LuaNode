/**
 * Memory manager based on buddy algorithm
 * Modified MAX_ORDER to change the memory used
 *
 * Nicholas3388
 */

#ifndef _EASY_MEM_H_
#define _EASY_MEM_H_

#include "c_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct chunk {
  struct chunk *next;
  uint8_t order;
  uint8_t curr_order;
  uint8_t used;
  uint8_t pad;
};

#define MAX_ORDER               3
#define MIN_CHUNK_SIZE          8
#define MAX_CHUNK_SIZE          MIN_CHUNK_SIZE * (1<<MAX_ORDER)  //64
#define MAX_CHUNK_NUM           1
#define MEM_ALIGNMENT           4

#define layer_size(order)       (MIN_CHUNK_SIZE * (1<<order))
#define layer_heads_size(order) (((1<<order) - 1)*sizeof(struct chunk))

#define mem_align(mem)  (   \
        (void *)((uint32_t)(mem+MEM_ALIGNMENT-1) & ~(uint32_t)(MEM_ALIGNMENT-1))  \
)

#define quotient(num, divisor) (uint32_t)(num/divisor)
#define remainder(num, divisor) (num%divisor)
#define size_to_chunk_order(size) (   \
        remainder(size, MIN_CHUNK_SIZE)==0 ? quotient(size, MIN_CHUNK_SIZE)-1 : quotient(size, MIN_CHUNK_SIZE) \
)

#define skip_chunk_head(c)  ((uint8_t *)c + sizeof(struct chunk))
#define get_chunk_head(addr)    ((struct chunk *)((uint8_t *)addr-sizeof(struct chunk)))

#define adjust_head(h, dir)   {  \
        struct chunk *new_head = (struct chunk *)h+dir; \
        new_head->order = h->order;  \
        new_head->used = h->used; \
        h = new_head; \
}


void easy_mem_init(void);
void *easy_mem_alloc(uint32_t size);
void easy_mem_free(void *mem);
void easy_mem_statistics(void);

#ifdef __cplusplus
}
#endif

#endif // _EASY_MEM_H_
