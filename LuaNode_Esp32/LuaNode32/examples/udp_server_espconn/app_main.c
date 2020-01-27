#include <stdio.h>
#include <string.h>

#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "espconn.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "tmr.h"

#define USER_DATA "ESP32_Data"
#define EXAMPLE_WIFI_SSID "TP-LINK_93D966"
#define EXAMPLE_WIFI_PASS "123456"
#define TMP_TASK_PRIORITY 13
#define LOCAL_PORT 11000
#define BUFF_SIZE 128

static struct espconn *_ptrUDPServer;
static EventGroupHandle_t wifi_event_group;

const int CONNECTED_BIT = BIT0;
static const char *TAG = "main";

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "STA start");
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            /* This is a workaround as ESP32 WiFi libs don't currently
               auto-reassociate. */
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            free(_ptrUDPServer);
            free(_ptrUDPServer->proto.udp);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = EXAMPLE_WIFI_SSID,
                .password = EXAMPLE_WIFI_PASS,
            },
    };
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    esp_event_loop_init(event_handler, NULL);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void udp_recv(void *arg, char *pdata, unsigned short length)
{
    struct espconn *pesp_conn = arg;
    char buff[BUFF_SIZE];
    memset(buff, 0, BUFF_SIZE);
    ESP_LOGI(TAG, "client %d.%d.%d.%d:%d -> ", pesp_conn->proto.udp->remote_ip[0], pesp_conn->proto.udp->remote_ip[1],
             pesp_conn->proto.udp->remote_ip[2], pesp_conn->proto.udp->remote_ip[3], pesp_conn->proto.udp->remote_port);
    ESP_LOGI(TAG, "local %d.%d.%d.%d:%d -> ", pesp_conn->proto.udp->local_ip[0], pesp_conn->proto.udp->local_ip[1],
             pesp_conn->proto.udp->local_ip[2], pesp_conn->proto.udp->local_ip[3], pesp_conn->proto.udp->local_port);
    ESP_LOGI(TAG, "received %d bytes of data", length);
    if (length > BUFF_SIZE)
    {
        ESP_LOGI(TAG, "received data out of bound");
    }
    memcpy(buff, pdata, length);
    ESP_LOGI(TAG, "content: %s", pdata);
}

static void tmp_task(void *pvParameters)
{
    while (1)
    {
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Got IP");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);

        // start espconn
        _ptrUDPServer = (struct espconn *)malloc(sizeof(struct espconn));
        _ptrUDPServer->type = ESPCONN_UDP;
        _ptrUDPServer->state = ESPCONN_NONE;
        _ptrUDPServer->proto.udp = (esp_udp *)malloc(sizeof(esp_udp));
        memset(_ptrUDPServer->proto.udp, 0, sizeof(esp_udp));
        _ptrUDPServer->proto.udp->local_port = LOCAL_PORT;

        espconn_regist_recvcb(_ptrUDPServer, udp_recv);
        espconn_create(_ptrUDPServer);
        ESP_LOGI(TAG, "Wait for client");

        break;
    }
    vTaskDelete(NULL);
}

void app_main()
{
    nvs_flash_init();
    initialise_wifi();

    xTaskCreate(&tmp_task, "tmp_task", 2048, NULL, TMP_TASK_PRIORITY, NULL);
}
