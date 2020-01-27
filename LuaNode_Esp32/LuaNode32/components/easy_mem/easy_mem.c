#include "easy_mem.h"

#include "easy_conf.h"
//#include "easy_os.h"
#include "chain.h"

/**
 * multiply to make sure there are (2<<(MAX_ORDER+1)-1) heads
 */
uint8_t mem_total[MAX_CHUNK_NUM * MAX_CHUNK_SIZE + layer_heads_size(MAX_ORDER + 1) + MEM_ALIGNMENT - 1];
void *mem_memory = NULL;  // alignment memory
struct chunk *mem_heads[MAX_ORDER + 1];

/**
 * Split chunk recursively
 */
static struct chunk *split(struct chunk *c, uint8_t cnt, uint8_t order)
{
    add_to_head(mem_heads[cnt], c);
    c->curr_order = cnt;
    struct chunk *buddy = (struct chunk *)((void *)c + layer_size(cnt) + layer_heads_size(cnt + 1));
    buddy->next = NULL;
    buddy->order = c->order;
    buddy->curr_order = c->curr_order;
    buddy->used = 0;

    if (cnt > order)
    {
        adjust_head(buddy, 1);
        return split(buddy, --cnt, order);
    }
    buddy->used = 1;
    return buddy;
}

/**
 * Merge two free chunks to a larger one recursively
 */
static void merge(struct chunk *mem_chunk, uint8_t order)
{
    // chunk list is empty, insert directly
    if (mem_heads[order] == NULL)
    {
        mem_chunk->used = 0;
        mem_heads[order] = mem_chunk;
        return;
    }

    struct chunk *tmp = mem_heads[order];
    while (tmp != NULL)
    {
        uint32_t length = layer_size(order) + layer_heads_size(order + 1);
        if (tmp->order == mem_chunk->order)
        {
            // only those two neighboring buddies can be merged
            if ((void *)mem_chunk == (void *)tmp + length)
            {
                adjust_head(tmp, -1);
                merge((struct chunk *)tmp, ++order);
            }
            else if ((void *)mem_chunk + length == (void *)tmp)
            {
                adjust_head(mem_chunk, -1);
                merge(mem_chunk, ++order);
            }
            else
            {  // cannot merge, add to list head
                mem_chunk->used = 0;
                add_to_head(mem_heads[order], mem_chunk);
            }
            return;
        }
        tmp = tmp->next;
    }
}

/**
 * Memory initialization
 */
void easy_mem_init(void)
{
    mem_memory = (void *)mem_align(mem_total);
    mem_heads[MAX_ORDER] = (struct chunk *)mem_memory;
    struct chunk *head = mem_heads[MAX_ORDER];
    head->next = NULL;
    head->order = MAX_ORDER;
    head->curr_order = MAX_ORDER;
    head->used = 0;
#ifdef ENABLE_DEBUG
    easy_mem_statistics();
#endif  // ENABLE_DEBUG
}

/**
 * Memory alloc
 */
void *easy_mem_alloc(uint32_t size)
{
#ifdef ENABLE_DEBUG
    easy_assert(size > 0 && size < MAX_CHUNK_NUM * MAX_CHUNK_SIZE && mem_memory != NULL);
#endif  // ENABLE_DEBUG
    struct chunk *res = NULL;

    if (size < 0 || size > MAX_CHUNK_NUM * MAX_CHUNK_SIZE)
    {
        ERROR_PRINT("Invalid memory size\n");
        goto alloc_failed;
    }

    if (mem_memory == NULL)
    {
        ERROR_PRINT("Memory not init\n");
        goto alloc_failed;
    }

    uint8_t order = 0;
    while (1 << order < quotient(size, MIN_CHUNK_SIZE)) order++;

    if (mem_heads[order] == NULL)
    {  // chunk split
        if (order == MAX_ORDER)
        {
            ERROR_PRINT("Not enough memory\n");
            goto alloc_failed;
        }
        uint8_t cnt = order;
        while (++cnt <= MAX_ORDER)
        {
            if (mem_heads[cnt] != NULL)
                break;
        }
        struct chunk *target = mem_heads[cnt];
        remove_from_chain(mem_heads[cnt], struct chunk, target);
        adjust_head(target, 1);
        res = split(target, --cnt, order);
    }
    else
    {
        res = mem_heads[order];
        res->used = 1;
        remove_from_chain(mem_heads[order], struct chunk, res);
    }

    return ((void *)skip_chunk_head(res));
alloc_failed:
    return NULL;
}

/**
 * Memory free
 */
void easy_mem_free(void *mem)
{
#ifdef ENABLE_DEBUG
    easy_assert(mem != NULL && mem <= mem_memory + sizeof(mem_total) && mem >= mem_memory);
#endif  // ENABLE_DEBUG
    if (mem == NULL)
        return;

    if (mem < mem_memory || mem > mem_memory + sizeof(mem_total))
    {
        ERROR_PRINT("Invalid memory address");
        return;
    }

    struct chunk *head = (struct chunk *)get_chunk_head(mem);
    uint8_t curr_order = head->curr_order;
    merge(head, curr_order);
}

/**
 * Print system memory statistics
 */
void easy_mem_statistics(void)
{
    uint32_t total = sizeof(mem_total);
    uint32_t used = 0;
    uint32_t remaining = 0;
    DEBUG_PRINT("+---- Easy chunk memory -----\n");
    DEBUG_PRINT("| Total: %d bytes\n", total);
    DEBUG_PRINT("| Can be used: %d bytes\n", MAX_CHUNK_NUM * MAX_CHUNK_SIZE);
    DEBUG_PRINT("| Used: %d bytes\n", used);
    DEBUG_PRINT("| Remaining: %d bytes\n", total - used);
    DEBUG_PRINT("| Memory start from: %p\n", mem_memory);
    DEBUG_PRINT("+-----------------------\n");
}
