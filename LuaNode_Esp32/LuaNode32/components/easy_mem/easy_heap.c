#include "easy_heap.h"

#include "easy_conf.h"
//#include "easy_os.h"

uint8_t ram_heap[HEAP_SIZE + 2 * sizeof(struct mem_heap) + HEAP_ALIGNMENT - 1];
void *ram;
struct mem_heap *pfree;
struct mem_heap *heap_end;

/**
 * Fill hole
 */
static void plug_hole(struct mem_heap *heap)
{
    struct mem_heap *nheap, *pheap;
    nheap = (struct mem_heap *)&(((uint8_t *)ram)[heap->next]);
    if (heap != nheap && !nheap->used && nheap != heap_end)
    {
        if (pfree == nheap)
            pfree = heap;
        heap->next = nheap->next;
        ((struct mem_heap *)&(((uint8_t *)ram)[nheap->next]))->prev = (uint8_t *)heap - (uint8_t *)ram;
    }
    pheap = (struct mem_heap *)&(((uint8_t *)ram)[heap->prev]);
    if (heap != pheap && !pheap->used)
    {
        if (pfree == heap)
            pfree = pheap;
        pheap->next = heap->next;
        ((struct mem_heap *)&(((uint8_t *)ram)[heap->next]))->prev = (uint8_t *)pheap - (uint8_t *)ram;
    }
}

/**
 * Heap initialization
 */
void easy_heap_init(void)
{
    ram = heap_align(ram_heap);
    struct mem_heap *start = (struct mem_heap *)ram;
    pfree = start;
    start->used = 0;
    start->prev = 0;
    start->next = HEAP_SIZE;
    heap_end = (struct mem_heap *)(ram + HEAP_SIZE + sizeof(struct mem_heap));
    heap_end->prev = 0;
    heap_end->next = 0;
    heap_end->used = 1;
#ifdef ENABLE_DEBUG
    easy_heap_statistics();
#endif  // ENABLE_DEBUG
}

/**
 * Heap alloc
 */
void *easy_heap_alloc(uint16_t size)
{
#ifdef ENABLE_DEBUG
    easy_assert(size >= 0 && size <= HEAP_SIZE);
#endif  // ENABLE_DEBUG
    uint8_t ptr, ptr2;
    struct mem_heap *mem, *mem2;

    if (ram == NULL)
    {
        ERROR_PRINT("Heap is not initialized\n");
        return NULL;
    }

    if (size <= 0)
    {
        ERROR_PRINT("Invalid size of memory\n");
        return NULL;
    }

    uint32_t aligned_size = ((size + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1));
    if (aligned_size > HEAP_SIZE)
    {
        ERROR_PRINT("Require too much memory\n");
        return NULL;
    }

    for (ptr = (uint8_t *)pfree - (uint8_t *)ram; ptr < HEAP_SIZE;
         ptr = ((struct mem_heap *)&(((uint8_t *)ram)[ptr]))->next)
    {
        mem = (struct mem_heap *)&(((uint8_t *)ram)[ptr]);
        if (!(mem->used) && (uint32_t)(mem->next - (ptr + sizeof(struct mem_heap))) >= aligned_size)
        {
            /**
             * Now, can alloc memory
             * If the remaining free memory after alloc is enough to construct
             * a new heap, we split the current heap to two part. Otherwise,
             * return the current heap.
             */
            if ((uint8_t *)(mem->next) - (ptr + sizeof(struct mem_heap)) >= aligned_size + MIN_SIZE_ALIGNED)
            {
                ptr2 = ptr + sizeof(struct mem_heap) + aligned_size;
                mem2 = (struct mem_heap *)&(((uint8_t *)ram)[ptr2]);
                mem2->used = 0;
                mem2->next = mem->next;
                mem2->prev = ptr;
                mem->next = ptr2;
                if (mem2->next != HEAP_SIZE)
                    ((struct mem_heap *)&(((uint8_t *)ram)[mem2->next]))->prev = ptr2;
            }
            mem->used = 1;

            if (mem == pfree)
            {  // update pfree
                while (pfree->used && pfree != heap_end) pfree = (struct mem_heap *)&(((uint8_t *)ram)[pfree->next]);
            }
            return ((void *)skip_heap_head(mem));
        }
    }

    return NULL;
}

/**
 * Heap free
 */
void easy_heap_free(void *heap)
{
#ifdef ENABLE_DEBUG
    easy_assert(heap != NULL && heap >= ram + sizeof(struct mem_heap) && heap <= sizeof(ram_heap));
#endif  // ENABLE_DEBUG
    if (heap == NULL)
        return;

    if (heap < ram || heap > ram + HEAP_SIZE)
    {
        ERROR_PRINT("Invalid address\n");
        return;
    }

    struct mem_heap *mem = get_heap_head(heap);
    mem->used = 0;
    if (mem < pfree)
        pfree = mem;

    plug_hole(mem);
}

/**
 * Heap realloc
 */
void *easy_heap_realloc(void *heap, uint16_t size)
{
    easy_heap_free(heap);
    return easy_heap_alloc(size);
}

/**
 * Heap statistics
 */
void easy_heap_statistics(void)
{
    uint32_t total = sizeof(ram_heap);
    uint32_t used = 0;
    uint32_t remaining = 0;
    DEBUG_PRINT("+----- Easy heap memory -----\n");
    DEBUG_PRINT("| Total: %d bytes\n", total);
    DEBUG_PRINT("| Can be used: %d bytes\n", HEAP_SIZE);
    DEBUG_PRINT("| Used: %d bytes\n", used);
    DEBUG_PRINT("| Remaining: %d bytes\n", total - used);
    DEBUG_PRINT("| Memory start from: %p\n", ram);
    DEBUG_PRINT("+-----------------------\n");
}
