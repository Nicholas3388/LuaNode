#include <string.h>

#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
#include "platform.h"
#include "tcpip_adapter.h"
#include "tmr.h"

#define UDP_DNS_PORT 53
#define UDP_DATA_LEN 1024
#define UDP_LOCAL_PORT 7
#define SSID_AP "DOIT"
#define DNS_SERVER_TASK_PRIO 12
#define TMP_TASK_PRIO 13

#define DNS_QR_QUERY 0
#define DNS_QR_RESPONSE 1
#define DNS_OPCODE_QUERY 0

struct DNSHeader
{
    unsigned short ID;         // identification number
    unsigned char RD : 1;      // recursion desired
    unsigned char TC : 1;      // truncated message
    unsigned char AA : 1;      // authoritive answer
    unsigned char OPCode : 4;  // message_type
    unsigned char QR : 1;      // query/response flag
    unsigned char RCode : 4;   // response code
    unsigned char Z : 3;       // its z! reserved
    unsigned char RA : 1;      // recursion available
    unsigned short QDCount;    // number of question entries
    unsigned short ANCount;    // number of answer entries
    unsigned short NSCount;    // number of authority entries
    unsigned short ARCount;    // number of resource entries
};

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static const char *TAG = "main";

void led_blink(void)
{
    if (platform_gpio_mode(2, 2) < 0)
    {
        ESP_LOGI(TAG, "Led lightup failed");
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
                .ssid = SSID_AP, .ssid_len = strlen(SSID_AP), .authmode = WIFI_AUTH_OPEN, .max_connection = 10,
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

static bool req_includes_only_one_quest(struct DNSHeader *h)
{
    return (ntohs(h->QDCount) == 1 && h->ANCount == 0 && h->NSCount == 0 && h->ARCount == 0);
}

static void reply_with_ip(int32 sock_fd, struct DNSHeader *h, uint32_t len, struct sockaddr *toaddr, socklen_t tolen)
{
    h->QR = DNS_QR_RESPONSE;
    h->ANCount = h->QDCount;
    uint32_t length = len + 16;

    uint8_t *pkg_buf = (uint8_t *)malloc(length);
    if (pkg_buf == NULL)
    {
        ESP_LOGI(TAG, "UDP packet alloc failed!\n");
        return;
    }
    memset(pkg_buf, 0, length);
    memcpy(pkg_buf, (void *)h, len);

    pkg_buf[len] = 192;     //  answer name is a pointer
    pkg_buf[len + 1] = 12;  // pointer to offset at 0x00c
    pkg_buf[len + 2] = 0;   // 0x0001  answer is type A query (host address)
    pkg_buf[len + 3] = 1;
    pkg_buf[len + 4] = 0;  // 0x0001 answer is class IN (internet address)
    pkg_buf[len + 5] = 1;

    // write ttl
    uint32_t _ttl = htonl(60);
    uint8_t *pttl = (uint8_t *)(&_ttl);
    pkg_buf[len + 6] = pttl[0];
    pkg_buf[len + 7] = pttl[1];
    pkg_buf[len + 8] = pttl[2];
    pkg_buf[len + 9] = pttl[3];
    pkg_buf[len + 10] = 0;
    pkg_buf[len + 11] = 4;

    // ip
    pkg_buf[len + 12] = 192;
    pkg_buf[len + 13] = 168;
    pkg_buf[len + 14] = 4;
    pkg_buf[len + 15] = 1;

    uint32_t ret = sendto(sock_fd, pkg_buf, length, 0, toaddr, tolen);
    free(pkg_buf);
}

static void reply_with_custom_code(struct DNSHeader *h) { ESP_LOGI(TAG, "reply_with_custom_code"); }

static void dns_server_task(void *pvParameters)
{
    int32 sock_fd;
    int ret = 0;
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_DNS_PORT);
    server_addr.sin_len = sizeof(server_addr);

    while ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        ESP_LOGI(TAG, "create socket failed");
        tmr_delay_msec(1000);
    }

    while (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        ESP_LOGI(TAG, "socket bind failed");
        tmr_delay_msec(1000);
    }

    char *udp_msg = (char *)malloc(UDP_DATA_LEN);
    struct sockaddr_in from;
    int fromlen = 0;
    int nNetTimeout = 5;
    memset(udp_msg, 0, UDP_DATA_LEN);
    memset(&from, 0, sizeof(from));
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&nNetTimeout,
               sizeof(int));  // non-block
    fromlen = sizeof(struct sockaddr_in);

    ESP_LOGI(TAG, "udp server at port:%d\r\n", UDP_DNS_PORT);
    while (1)
    {
        ret = recvfrom(sock_fd, (uint8 *)udp_msg, UDP_DATA_LEN, 0, (struct sockaddr *)&from, (socklen_t *)&fromlen);
        if (ret > 0)
        {
            struct DNSHeader *dns_header = (struct DNSHeader *)udp_msg;

            if (dns_header->QR == DNS_QR_QUERY && dns_header->OPCode == DNS_OPCODE_QUERY &&
                req_includes_only_one_quest(dns_header))
            {
                reply_with_ip(sock_fd, dns_header, ret, (struct sockaddr *)&from, fromlen);
            }
            else if (dns_header->QR == DNS_QR_QUERY)
            {
                reply_with_custom_code(dns_header);
            }

            memset(udp_msg, 0, UDP_DATA_LEN);
        }
        else if (ret == 0)
        {
            ESP_LOGI(TAG, "recvfrom error!");
            break;
        }
    }

    if (udp_msg)
    {
        free(udp_msg);
        udp_msg = NULL;
    }

    close(sock_fd);
    vTaskDelete(NULL);
}

static void tmp_task(void *pvParameters)
{
    while (1)
    {
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "AP started");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        xTaskCreate(dns_server_task, "dns_server_task", 2048, NULL, DNS_SERVER_TASK_PRIO, NULL);
        break;
    }
    vTaskDelete(NULL);
}

void app_main()
{
    led_blink();  // led flashing
    nvs_flash_init();
    initialise_wifi();

    xTaskCreate(tmp_task, "tmp_task", 2048, NULL, TMP_TASK_PRIO, NULL);

    return 0;
}
