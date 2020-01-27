#include "driver/gpio.h"
#include "ds1307.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#define TAG "main"

#define DEFAULT_SSID "Doit"
#define DEFAULT_PASS "doit3305"

EventGroupHandle_t station_event_group;
const int STA_GOTIP_BIT = BIT0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
        case SYSTEM_EVENT_STA_START:  // station start
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
        {  // station disconnect from ap
            ESP_LOGI(TAG, "Station disconnected");
            break;
        }
        case SYSTEM_EVENT_STA_CONNECTED:  // station connect to ap
            ESP_LOGI(TAG, "Station connected to AP");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
        {  // station get ip
            ESP_LOGI(TAG, "got ip:%s\n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(station_event_group, STA_GOTIP_BIT);
            // create_tcp_client("192.168.9.115", 5556);
            break;
        }
        case SYSTEM_EVENT_AP_STACONNECTED:  // a station connect to ap
            ESP_LOGI(TAG, "station:" MACSTR " join,AID=%d\n", MAC2STR(event->event_info.sta_connected.mac),
                     event->event_info.sta_connected.aid);
            // xEventGroupSetBits(tcp_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:  // a station disconnect from ap
            ESP_LOGI(TAG, "station:" MACSTR "leave,AID=%d\n", MAC2STR(event->event_info.sta_disconnected.mac),
                     event->event_info.sta_disconnected.aid);
            // xEventGroupClearBits(tcp_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            // xEventGroupClearBits(eth_event_group, ETH_CONNECTED_BIT);
            // xEventGroupClearBits(eth_event_group, ETH_GOTIP_BIT);
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            break;
        default:
            break;
    }
    return ESP_OK;
}

void app_main(void)
{
    nvs_flash_init();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t sta_config = {.sta = {
                                    .ssid = DEFAULT_SSID,
                                    .password = DEFAULT_PASS,
                                }};
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    // ESP_ERROR_CHECK( esp_wifi_connect() );
    station_event_group = xEventGroupCreate();
    initDS1307();
}
