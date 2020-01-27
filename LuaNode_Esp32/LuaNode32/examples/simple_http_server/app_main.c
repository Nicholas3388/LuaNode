#include <stdio.h>
#include <string.h>

#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
#include "platform.h"
#include "tcpip_adapter.h"
#include "tmr.h"

#define SSID_AP "DOIT"
#define HTTP_SERVER_PORT 80
#define MAX_HTTP_CLIENT_CONN 1
#define HTTP_CLIENT_BUFF_LEN 1024

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static const char *TAG = "main";
static uint8_t curentServerClient = 0;

const uint8 webserver_str[] =
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n\
<html>\r\n\
 <head>\r\n\
  <meta charset='utf-8' />\r\n\
  <meta http-equiv='x-ua-comptible' content='ie=edge' />\r\n\
  <title>Test Header</title>\r\n\
  <meta name='viewport' content='width=device-width, initial-scale=1' />\r\n\
 </head>\r\n\
 <body>\r\n\
  <p>This is a test!</p>\r\n\
 </body>\r\n\
</html>";

void led_blink(void)
{
    int res = platform_gpio_mode(2, 2);  // pin mode: OUTPUT
    if (res < 0)
    {
        printf("Led lightup failed\n");
        return;
    }
    platform_gpio_write(2, 1);  // led on
    tmr_delay_msec(250);
    platform_gpio_write(2, 0);  // led off
    tmr_delay_msec(250);
    platform_gpio_write(2, 1);
    tmr_delay_msec(250);
    platform_gpio_write(2, 0);
    tmr_delay_msec(250);
    platform_gpio_write(2, 1);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
        case SYSTEM_EVENT_AP_START:
            ESP_LOGI(TAG, "AP start now");
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_AP_STOP:
            ESP_LOGI(TAG, "AP stop now");
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "Station connect");
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "Station disconnect");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    wifi_config_t config = {
        .ap =
            {
                .ssid = SSID_AP,
                //.password = "123",
                .ssid_len = strlen(SSID_AP),
                //.channel = 0,
                .authmode = WIFI_AUTH_OPEN,
                // Max number of stations allowed to connect in, default 4, max
                // 4
                .max_connection = 10,
                //.ssid_hidden = 0,
                //.beacon_interval = 100,
            },
    };

    // set SSID
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    sprintf(config.ap.ssid, SSID_AP "_%02X%02X%02X", mac[3], mac[4], mac[5]);
    config.ap.ssid_len = strlen(config.ap.ssid);

    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    esp_event_loop_init(event_handler, NULL);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static int parse_http_request(int32 client, char *p, int len)
{
    if (NULL != strstr(p, "GET / HTTP/1.1"))
    {
        ESP_LOGI(TAG, "socket write data");
        write(client, webserver_str, strlen(webserver_str));
    }
    return 0;
}

static void task_http_server_client(void *inFd)
{
    int32 client_sock = *(int *)inFd;
    char *recv_buf = (char *)malloc(HTTP_CLIENT_BUFF_LEN);
    int recbytes = 0;

    ESP_LOGI(TAG, "setup socket:(%d)", client_sock);

    // int nNetTimeout = 5;
    // setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (char * )&nNetTimeout,
    //		sizeof(int)); //non-block
    curentServerClient++;
    while (1)
    {
        recbytes = read(client_sock, recv_buf, HTTP_CLIENT_BUFF_LEN - 1);
        if (recbytes > 0)
        {
            recv_buf[recbytes] = 0;
            ESP_LOGI(TAG, "(socket:%d) recieve:(%d): %s", client_sock, recbytes, recv_buf);
            if (parse_http_request(client_sock, recv_buf, recbytes) == 0)
            {
                break;
            }
        }
        else if (recbytes == 0)
        {
            break;
        }
    }

    free(recv_buf);
    printf("task_http_server_client > close socket:(%d)\n", client_sock);
    close(client_sock);

    curentServerClient--;
}

static void task_http_server(void *pvParameters)
{
    int32 listenfd;
    int32 ret;
    struct sockaddr_in server_addr, remote_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_len = sizeof(server_addr);
    server_addr.sin_port = htons(HTTP_SERVER_PORT);

    while ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        ESP_LOGI(TAG, "socket create failed");
        tmr_delay_msec(1000);
    }

    while (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        ESP_LOGI(TAG, "socket bind failed");
        tmr_delay_msec(1000);
    }

    while (listen(listenfd, MAX_HTTP_CLIENT_CONN) != 0)
    {
        ESP_LOGI(TAG, "socket listen failed");
        tmr_delay_msec(1000);
    }

    int32 client_sock;
    int32 len = sizeof(struct sockaddr_in);

    while (1)
    {
        ESP_LOGI(TAG, "wait client");
        if ((client_sock = accept(listenfd, (struct sockaddr *)&remote_addr, (socklen_t *)&len)) < 0)
        {
            ESP_LOGI(TAG, "socket accept failed");
            continue;
        }

        if (curentServerClient < MAX_HTTP_CLIENT_CONN)
        {
            printf("ESP32 HTTP server task > Client from %s %d\n", inet_ntoa(remote_addr.sin_addr),
                   htons(remote_addr.sin_port));
            // xTaskCreate(task_http_server_client, "task_http_server_client",
            // 1024, 		&client_sock, 1, NULL);
            task_http_server_client(&client_sock);
        }
        else
        {
            close(client_sock);  // close socket
            tmr_delay_msec(1000);
        }
    }

    vTaskDelete(NULL);
}

static void http_task(void *pvParameters)
{
    while (1)
    {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "AP started");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        xTaskCreate(task_http_server, "task_http_server", 2048, NULL, 12, NULL);
        break;
    }
    vTaskDelete(NULL);
}

void app_main()
{
    led_blink();  // led flashing
    nvs_flash_init();
    initialise_wifi();

    xTaskCreate(&http_task, "http_task", 2048, NULL, 13, NULL);

    return 0;
}
