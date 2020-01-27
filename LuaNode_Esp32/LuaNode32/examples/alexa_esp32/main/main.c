// Copyright 2015-2016 Doctors of Intelligence & Technology (Shenzhen) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * Nicholas3388
 * 2017.06.02
 * email: wangwei@doit.am
 * www.doit.am
 */

#include "alexa.h"
#include "driver/gpio.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "user_config.h"

static const char *TAG = "main";

static const int CONNECTED_BIT = BIT0;
static EventGroupHandle_t wifi_event_group;
static uint8_t wifi_mac[6] = {0};
static tcpip_adapter_ip_info_t ip_info;
static char local_ip[16] = {0};
static unsigned char local_ip_num[4] = {0};

char *get_local_ip(void) { return local_ip; }

unsigned char *get_ip_num(void) { return local_ip_num; }

static void save_ip_num(uint32_t ip)
{
    local_ip_num[0] = (unsigned char)(ip & 0xFF);
    local_ip_num[1] = (unsigned char)((ip >> 8) & 0xFF);
    local_ip_num[2] = (unsigned char)((ip >> 16) & 0xFF);
    local_ip_num[3] = (unsigned char)((ip >> 24) & 0xFF);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void tmp_task(void *pvParameters)
{
    while (1)
    {
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Got IP");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        if (tcpip_adapter_get_ip_info(ESP_IF_WIFI_STA, &ip_info) == 0)
        {
            ESP_LOGI(TAG, "~~~~~~~~~~~");
            ESP_LOGI(TAG, "IP:" IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, "MASK:" IPSTR, IP2STR(&ip_info.netmask));
            ESP_LOGI(TAG, "GW:" IPSTR, IP2STR(&ip_info.gw));
            ESP_LOGI(TAG, "~~~~~~~~~~~");
            sprintf(local_ip, IPSTR, IP2STR(&ip_info.ip));
            save_ip_num(ip_info.ip.addr);
        }

        ESP_LOGI(TAG, "AP+STA started");
        alexa_init();
    }
    vTaskDelete(NULL);
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();

    wifi_config_t sta_config = {
        .sta = {.ssid = MY_WIFI_SSID, .password = MY_WIFI_PASS, .bssid_set = false},
    };

    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

    esp_err_t er = esp_wifi_get_mac(ESP_IF_WIFI_STA, wifi_mac);
    if (er != ESP_OK)
    {
        ESP_LOGE(TAG, "get wifi MAC failed");
    }
    else
    {
        ESP_LOGI(TAG, "WiFi MAC: %02x%02x%02x%02x%02x%02x", wifi_mac[0], wifi_mac[1], wifi_mac[2], wifi_mac[3],
                 wifi_mac[4], wifi_mac[5]);
    }

    ESP_ERROR_CHECK(esp_wifi_start());
}

void gpio_init(void)
{
    gpio_pad_select_gpio(GPIO_TEST);
    gpio_set_direction(GPIO_TEST, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_TEST, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "enter main");
    initialise_wifi();
    xTaskCreate(&tmp_task, "tmp_task", TMP_TASK_STACK_SIZE, NULL, TMP_TASK_PRIO, NULL);
    gpio_init();
}
