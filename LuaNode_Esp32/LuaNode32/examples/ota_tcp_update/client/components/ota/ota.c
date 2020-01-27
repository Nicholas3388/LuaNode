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

#include "ota.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include "user_config.h"
#include "utils.h"

#define PACKAGE_SIZE 512  // the server send 512 byte one time
#define RECV_BUFF_SIZE 768

#define KEY_TOTAL "total: "
#define KEY_LENGTH "length: "
#define HEADER_END "\r\n\r\n"
#define REQ_DATA "req0\r\n\r\n"
#define REQ_RESEND "req1\r\n\r\n"

static const char *TAG = "ota";

static char recv_buff[RECV_BUFF_SIZE] = {0};
static char ota_write_data[RECV_BUFF_SIZE] = {0};

static char remote_ip[4] = {192, 168, 9, 154};

static int str2num(char *str, int len)
{
    int i, res = 0;
    for (i = 0; i < len; i++) { res += pow10(len - i - 1) * (str[i] - '0'); }
    return res;
}

static int get_length(char *s, int skip)
{
    int res = -1;
    char *start = s + skip;
    char buff[16] = {0};
    int i = 0;
    while (*start != '\r' && *start != '\n')
    {
        buff[i] = *start;
        if (i > 16)
        {
            ESP_LOGE(TAG, "total length is > 16");
            return -1;
        }
        i++;
        start++;
    }
    res = str2num(buff, i);

    return res;
}

static void ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA task...");
    esp_err_t err;
    struct netbuf *recvbuf = NULL;
    struct pbuf *q;
    struct netconn *conn = NULL;
    int data_len = 0;
    int total_len = 0;
    int recv_count = 0;
    bool resend = false;

    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end()
     */
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG, "Starting OTA example...");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    assert(configured == running); /* fresh from reset, should be running from
                                      configured boot partition */
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)", configured->type, configured->subtype,
             configured->address);

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x", update_partition->subtype,
             update_partition->address);
    assert(update_partition != NULL);

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
    }
    ESP_LOGI(TAG, "esp_ota_begin succeeded");

    while ((conn = netconn_new(NETCONN_TCP)) == NULL)
    {
        ESP_LOGE(TAG, "Create conn failed");
        delay_ms(1000);
    }
    ip_addr_t serverip;
    IP_ADDR4(&serverip, remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3]);

    netconn_bind(conn, NULL, LOCAL_PORT);

    err_t er;
    while ((er = netconn_connect(conn, &serverip, REMOTE_PORT)) != ERR_OK)
    {
        ESP_LOGE(TAG, "Connect server failed, error code: %d", er);
        delay_ms(1000);
    }

    while (1)
    {
        if (resend)
        {
            netconn_write(conn, REQ_RESEND, strlen(REQ_RESEND),
                          NETCONN_NOCOPY);  // request server to send last package again
            resend = false;
        }
        else
        {
            netconn_write(conn, REQ_DATA, strlen(REQ_DATA),
                          NETCONN_NOCOPY);  // request server to send next package
        }

        // ESP_LOGI(TAG, "waiting for server send data");
        netconn_recv(conn, &recvbuf);  // wait for data comes
        if (recvbuf != NULL)
        {
            // received data
            memset(recv_buff, 0, RECV_BUFF_SIZE);
            memset(ota_write_data, 0, RECV_BUFF_SIZE);
            data_len = 0;
            for (q = recvbuf->p; q != NULL; q = q->next)
            {
                if (q->len > (RECV_BUFF_SIZE - data_len))
                {
                    memcpy(recv_buff + data_len, q->payload, (RECV_BUFF_SIZE - data_len));
                }
                else
                {
                    memcpy(recv_buff + data_len, q->payload, q->len);
                }
                data_len += q->len;
                if (data_len > RECV_BUFF_SIZE)
                {
                    ESP_LOGE(TAG, "receive buffer overflow!!!");
                    break;
                }
            }

            // ota handle here
            // package format: "total: 123456\r\nlength: 123\r\n\r\n#@#$$%Q&#"
            // package data start after "\r\n\r\n"
            char *total_start;
            char *pack_len_start;
            char *data_start;
            int pack_len = 0;
            // extract total len
            if (total_len == 0)
            {
                if ((total_start = strstr(recv_buff, KEY_TOTAL)) != NULL)
                {
                    total_len = get_length(total_start, strlen(KEY_TOTAL));
                    ESP_LOGI(TAG, "OTA file size: %d", total_len);
                }
                else
                {
                    ESP_LOGE(TAG, "invalid package, resend later");
                    netbuf_delete(recvbuf);
                    resend = true;
                    delay_ms(1000);
                    break;
                }
            }

            // extract current package length
            if ((pack_len_start = strstr(recv_buff, KEY_LENGTH)) != NULL)
            {
                pack_len = get_length(pack_len_start, strlen(KEY_LENGTH));
                if ((data_start = strstr(recv_buff, HEADER_END)) != NULL)
                {
                    data_start += strlen(HEADER_END);
                    memcpy(ota_write_data, data_start, pack_len);
                    err = esp_ota_write(update_handle, (const void *)ota_write_data, pack_len);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG,
                                 "Error: esp_ota_write failed! err=0x%x, "
                                 "resend later",
                                 err);
                        netbuf_delete(recvbuf);
                        resend = true;
                        delay_ms(1000);
                        break;
                    }
                    recv_count += pack_len;
                    printf("OTA progress: %%%d\r", (int)(recv_count * 100 / total_len));
                    if (recv_count >= total_len)
                    {
                        printf("OTA progress: %%100\n");
                        ESP_LOGI(TAG, "OTA recevied all data! OK");
                        break;
                    }
                }
            }
            else
            {
                ESP_LOGE(TAG, "invalid package length, resend later");
                netbuf_delete(recvbuf);
                resend = true;
                delay_ms(1000);
                break;
            }

            netbuf_delete(recvbuf);  // do not forget to delete netbuf
            recvbuf = NULL;
        }
        delay_ms(50);
    }

    ESP_LOGI(TAG, "Total Write binary data length : %d", recv_count);

    if (esp_ota_end(update_handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed!");
        goto end_err_occur;
    }
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
        goto end_err_occur;
    }
    ESP_LOGI(TAG, "Prepare to restart system!");
    esp_restart();

    ESP_LOGE(TAG, "some error ocurr");
    netbuf_delete(recvbuf);
end_err_occur:
    vTaskDelete(NULL);
}

void ota_init(void) { xTaskCreate(&ota_task, "ota_task", OTA_TASK_STACK_SIZE, NULL, OTA_TASK_PRIO, NULL); }