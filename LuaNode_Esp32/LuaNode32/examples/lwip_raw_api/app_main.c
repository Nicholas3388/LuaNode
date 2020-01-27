/**
 * This is a LwIP Raw API sample. The sample create a TCP server listen port
 *11000. When received data, the server send the data back to client.
 *
 * Nicholas3388
 **/
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
#include "lwip/tcp.h"
#include "nvs_flash.h"
#include "tmr.h"

#define FEEDBACK_DATA "ESP32_Data"
#define EXAMPLE_WIFI_SSID "TP-LINK_93D966"
#define EXAMPLE_WIFI_PASS "123456"
#define TMP_TASK_PRIORITY 13
#define TCP_TASK_PRIORITY 12
#define LOCAL_PORT 11000
#define REMOTE_PORT 6000

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

static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    ESP_LOGI(TAG, "sent data");
    return ERR_OK;
}

static err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    ESP_LOGI(TAG, "received data");
    if (err == ERR_OK && p != NULL)
    {
        uint8_t *data_ptr = NULL;
        u32_t data_cntr = 0;
        data_ptr = (uint8_t *)malloc(p->tot_len + 1);
        memset(data_ptr, 0, p->tot_len + 1);
        data_cntr = pbuf_copy_partial(p, data_ptr, p->tot_len, 0);
        pbuf_free(p);
        if (data_cntr != 0)
        {
            ESP_LOGI(TAG, "content: %s", data_ptr);
            tcp_write(pcb, data_ptr, data_cntr,
                      1);  // data is not sent yet, until tcp_output called
            tcp_output(pcb);
        }
        free(data_ptr);
        data_ptr = NULL;
    }
    return ERR_OK;
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb)
{
    ESP_LOGI(TAG, "poll");
    return ERR_OK;
}

static err_t tcp_accept_callback(void *arg, struct tcp_pcb *pcb, err_t err)
{
    ESP_LOGI(TAG, "client try connecting");
    tcp_sent(pcb, tcp_server_sent);
    tcp_recv(pcb, tcp_server_recv);
    tcp_poll(pcb, tcp_server_poll, 8); /* every 1 seconds */
    return ERR_OK;
}

static void tmp_task(void *pvParameters)
{
    while (1)
    {
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Got IP");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);

        struct tcp_pcb *pcb = tcp_new();
        if (pcb == NULL)
        {
            ESP_LOGI(TAG, "tcp_pcb create failed");
            break;
        }
        tcp_bind(pcb, IP_ADDR_ANY, LOCAL_PORT);
        pcb = tcp_listen(pcb);
        // tcp_arg(pcb, (void *)params);
        ESP_LOGI(TAG, "wait for client");
        tcp_accept(pcb, tcp_accept_callback);
        break;
    }
    vTaskDelete(NULL);
}

void app_main()
{
    nvs_flash_init();
    initialise_wifi();

    xTaskCreate(&tmp_task, "tmp_task", 4096, NULL, TMP_TASK_PRIORITY, NULL);
}
