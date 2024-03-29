/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "lua.h"
#include "utils.h"
#include "uart_handler.h"
#include "luaconf.h"
#include "platform.h"


//#define WRITE_TEST_LUA_FILE

const static char *TAG = "main";

static esp_vfs_spiffs_conf_t conf = {
  .base_path = LUA_INIT_FILE_DIR,
  .partition_label = NULL,
  .max_files = 5,
  .format_if_mount_failed = true
}; 

char *get_partition_label(void)
{
	return conf.partition_label;
}

#ifdef WRITE_TEST_LUA_FILE
static void create_lua_init_file(void)
{
	FILE* f = fopen("/spiffs/init.lua", "w");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}
	fprintf(f, "print('Hello LuaNode'); print(_VERSION)\n");
	fclose(f);
	ESP_LOGI(TAG, "File written");
}
#endif

/**
 * init Spiff file system 
 */
static void spiff_init(void)
{
	ESP_LOGI(TAG, "Initializing SPIFFS");

	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem"); 
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    } 
	
	size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
#ifdef WRITE_TEST_LUA_FILE
		create_lua_init_file();
#endif
    }
}

void app_main(void)
{
	my_uart_init();
	vTaskDelay(50 / portTICK_PERIOD_MS);
	printf("\n\n");
	ESP_LOGI(TAG, "App main init");
	print_info();
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
#if (CURRENT_PLATFORM == NODE_PLATFORM_ESP8266)
    ESP_LOGI(TAG, "This is %s chip with %d CPU core(s), WiFi%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
#elif (CURRENT_PLATFORM == NODE_PLATFORM_ESP32)
	ESP_LOGI(TAG, "This is %s chip with %d CPU core(s), WiFi%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");
	ESP_LOGI(TAG, "%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
#endif

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    ESP_LOGI(TAG, "silicon revision v%d.%d, ", major_rev, minor_rev);
	
	spiff_init();

    char *lua_argv[] = {(char *)"lua", (char *)"-i", NULL};
    lua_main(2, lua_argv);
	
	while(1) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
