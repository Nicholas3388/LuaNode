#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static EventGroupHandle_t wifi_event_group;
const int SCAN_DONE_BIT = BIT0;
static wifi_scan_config_t scanConf = {.ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true};

static const char *TAG = "example";

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    if (event->event_id == SYSTEM_EVENT_SCAN_DONE)
    {
        xEventGroupSetBits(wifi_event_group, SCAN_DONE_BIT);
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void scan_task(void *pvParameters)
{
    while (1)
    {
        xEventGroupWaitBits(wifi_event_group, SCAN_DONE_BIT, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "WIFI scan doen");
        xEventGroupClearBits(wifi_event_group, SCAN_DONE_BIT);

        uint16_t apCount = 0;
        esp_wifi_scan_get_ap_num(&apCount);
        printf("Number of access points found: %d\n", apCount);
        if (apCount == 0)
        {
            ESP_LOGI(TAG, "Nothing AP found");
            return;
        }
        wifi_ap_record_t *list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, list));
        int i;
        printf(
            "=================================================================="
            "====\n");
        printf(
            "             SSID             |    RSSI    |           AUTH       "
            "    \n");
        printf(
            "=================================================================="
            "====\n");
        for (i = 0; i < apCount; i++)
        {
            char *authmode;
            switch (list[i].authmode)
            {
                case WIFI_AUTH_OPEN:
                    authmode = "WIFI_AUTH_OPEN";
                    break;
                case WIFI_AUTH_WEP:
                    authmode = "WIFI_AUTH_WEP";
                    break;
                case WIFI_AUTH_WPA_PSK:
                    authmode = "WIFI_AUTH_WPA_PSK";
                    break;
                case WIFI_AUTH_WPA2_PSK:
                    authmode = "WIFI_AUTH_WPA2_PSK";
                    break;
                case WIFI_AUTH_WPA_WPA2_PSK:
                    authmode = "WIFI_AUTH_WPA_WPA2_PSK";
                    break;
                default:
                    authmode = "Unknown";
                    break;
            }
            printf("%26.26s    |    % 4d    |    %22.22s\n", list[i].ssid, list[i].rssi, authmode);
        }
        free(list);
        printf("\n\n");

        // scan again
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        // The true parameter cause the function to block until the scan is
        // done.
        ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));
    }
}

int app_main(void)
{
    nvs_flash_init();
    tcpip_adapter_init();
    initialise_wifi();

    xTaskCreate(&scan_task, "scan_task", 2048, NULL, 15, NULL);

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));  // The true parameter cause the function to block
                                                            // until the scan is done.
    return 0;
}