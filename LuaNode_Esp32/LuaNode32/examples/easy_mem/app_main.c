#include <stdio.h>
#include <string.h>

#include "easy_heap.h"
#include "easy_mem.h"
#include "esp_log.h"
#include "esp_system.h"

#define BUF_SIZE 10

static const char *TAG = "main";

void app_main()
{
    easy_heap_init();
    easy_mem_init();

    unsigned char *addr1 = (unsigned char *)easy_heap_alloc(BUF_SIZE);
    memset(addr1, 0, BUF_SIZE);
    memcpy(addr1, "hello", 5);
    printf("%s\n", addr1);
    easy_heap_free(addr1);

    unsigned char *addr2 = (unsigned char *)easy_mem_alloc(BUF_SIZE);
    memset(addr2, 0, BUF_SIZE);
    memcpy(addr2, "world", 5);
    printf("%s\n", addr2);
    easy_mem_free(addr2);

    // print statistics
    easy_heap_statistics();
    easy_mem_statistics();
}
