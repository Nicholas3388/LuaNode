#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "flash_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform.h"
#include "spiffs.h"
#include "vfs.h"

static const char *TAG = "main";
static spiffs fs;

void app_main()
{
    char buf[12] = {0};

    if (flash_safe_get_size_byte() != flash_rom_get_size_byte())
    {
        ESP_LOGI(TAG, "Incorrect flash size reported, adjusting...");
        flash_rom_set_size_byte(flash_safe_get_size_byte());
        system_restart();
        return;
    }

    ESP_LOGI(TAG, "Mounting flash filesystem...");
    if (!vfs_mount("/FLASH", 0))
    {
        // Failed to mount -- try reformat
        ESP_LOGI(TAG, "Formatting file system. Please wait...");
        if (!vfs_format())
        {
            ESP_LOGI(TAG, "*** ERROR ***: unable to format. FS might be compromised.");
            ESP_LOGI(TAG, "It is advised to re-flash the NodeMCU image.");
        }
        // Note that fs_format leaves the file system mounted
    }

    spiffs_file fd = SPIFFS_open(&fs, "test_file", SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_RDWR, 0);
    ESP_LOGI(TAG, "fd=%d", fd);
    if (SPIFFS_write(&fs, fd, (u8_t *)"Hello SPIFFS", 12) < 0)
    {
        ESP_LOGI(TAG, "write errno %i", SPIFFS_errno(&fs));
    }
    SPIFFS_close(&fs, fd);

    vTaskDelay(1000 / portTICK_RATE_MS);

    fd = SPIFFS_open(&fs, "test_file", SPIFFS_RDWR, 0);
    ESP_LOGI(TAG, "fd=%d", fd);
    if (SPIFFS_read(&fs, fd, (u8_t *)buf, 12) < 0)
    {
        ESP_LOGI(TAG, "read errno %i", SPIFFS_errno(&fs));
    }
    SPIFFS_close(&fs, fd);

    ESP_LOGI(TAG, "--> %s <--", buf);
}
