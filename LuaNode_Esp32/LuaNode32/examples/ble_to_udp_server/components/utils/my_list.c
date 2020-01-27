#include "my_list.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "user_config.h"

static const char *TAG = "my_list";

static scan_list_t head;

void list_init(void)
{
    head.bda = NULL;
    head.uuid = NULL;
    head.rssi = 0;
    head.pNext = NULL;
}

scan_list_t *list_new_item(void)
{
    scan_list_t *newItem = (scan_list_t *)malloc(sizeof(scan_list_t));
    if (newItem == NULL)
    {
        ESP_LOGE(TAG, "malloc list item failed!");
        return NULL;
    }
    memset(newItem, 0, sizeof(scan_list_t));
    newItem->bda = (char *)malloc(BDA_SIZE);

    if (newItem->bda == NULL)
    {
        ESP_LOGE(TAG, "alloc for BDA failed!");
        free(newItem);
        return NULL;
    }

    newItem->uuid = (char *)malloc(UUID_SIZE);
    if (newItem->uuid == NULL)
    {
        ESP_LOGE(TAG, "alloc for UUID failed!");
        free(newItem->bda);
        free(newItem);
        return NULL;
    }

    return newItem;
}

void list_insert_to_head(scan_list_t *item)
{
    scan_list_t *next = NULL;
    next = head.pNext;
    head.pNext = item;
    item->pNext = next;
}

void list_destroy(void)
{
    scan_list_t *next = NULL;
    while (head.pNext != NULL)
    {
        next = (head.pNext)->pNext;
        free((head.pNext)->bda);
        free((head.pNext)->uuid);
        free(head.pNext);
        head.pNext = next;
    }
}

scan_list_t *list_get_head(void) { return &head; }