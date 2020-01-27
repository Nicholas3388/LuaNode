#include "udp_task.h"

#include <stdio.h>
#include <string.h>

#include "ble.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/api.h"
#include "my_list.h"
#include "user_config.h"
#include "utils.h"

#define LOCAL_PORT 9999

extern uint8_t wifi_mac[6];
extern void ble_client_app_register(void);

static const char *TAG = "udp_task";

static struct netconn *conn;

static void ip_str2num(const char *ip, int *res)
{
    int len = strlen(ip), i = 0;
    int start = 0;
    for (i = 0; i < len; i++)
    {
        if (ip[i] == '.')
        {
            *res = str2num((ip + start), (i - start));
            res++;
            start = i + 1;
            ESP_LOGI(TAG, "%s", ip + start);
        }
    }
    // last value
    *res = str2num((ip + start), (i - start));
}

void send_all_data(void)
{
    // send BLE scan results one by one
    char buff[94] = {0};
    scan_list_t *h = list_get_head();
    scan_list_t *iterator = h->pNext;
    while (iterator != NULL)
    {
        memset(buff, 0, 94);
        sprintf(buff, "%02x%02x%02x%02x%02x%02x, %s, %s, %d\r\n", wifi_mac[0], wifi_mac[1], wifi_mac[2], wifi_mac[3],
                wifi_mac[4], wifi_mac[5], iterator->bda, iterator->uuid, iterator->rssi);
        send_data((const char *)buff, 71);
#ifdef ENABLE_SCAN_OUTPUT
        ESP_LOGI(TAG, "%s", buff);
#endif
        iterator = iterator->pNext;
        vTaskDelay(400 / portTICK_PERIOD_MS);
        send_data((const char *)"\r\n", strlen("\r\n"));
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    list_destroy();
}

void connect_server(const char *ip, int port)
{
    conn = netconn_new(NETCONN_UDP);
    ip_addr_t serverip;
    int ip_val[4] = {0};
    ip_str2num(ip, ip_val);
    ESP_LOGI(TAG, "IP -> %d.%d.%d.%d", ip_val[0], ip_val[1], ip_val[2], ip_val[3]);
    IP_ADDR4(&serverip, ip_val[0], ip_val[1], ip_val[2], ip_val[3]);

    if (conn == NULL)
    {
        ESP_LOGE(TAG, "Create conn failed");
        return;
    }
    netconn_bind(conn, NULL, LOCAL_PORT);
    err_t er;
    while ((er = netconn_connect(conn, &serverip, port)) != ERR_OK)
    {
        ESP_LOGE(TAG, "Connect server failed, error code: %d, try again 1 seconds", er);
        set_panic();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    clear_panic();
    ESP_LOGI(TAG, "Connected to UDP server");
    // send_data((const char *)"test", 4);

    // ble_client_app_register();

    send_all_data();

    // restart scanning
    ESP_LOGI(TAG, "###############################################");
    ESP_LOGI(TAG, "restart scanning ......");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ble_start_scanning();
}

void send_data(const char *data, int len)
{
    struct netbuf *TCPNetbuf = netbuf_new();
    netbuf_ref(TCPNetbuf, data, len);
    netconn_send(conn, TCPNetbuf);
    netbuf_free(TCPNetbuf);
}

void disconnect_server()
{
    // not implement
}