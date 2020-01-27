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

#include "upnp_broadcast_responder.h"

#include <string.h>

#include "alexa_device.h"
#include "esp_log.h"
#include "lwip/api.h"
#include "lwip/igmp.h"
#include "lwip/sockets.h"
#include "user_config.h"
#include "utils.h"

#define UDP_PORT 1900
#define RECV_BUFFSIZE 1024

static const char *TAG = "upnp_broadcast_responder";
static bool udp_connected = false;

static struct netconn *udpconn = NULL;
static TaskHandle_t udp_tsk_handle = NULL;
static char udp_buff[RECV_BUFFSIZE + 1] = {0};

extern unsigned char *get_ip_num(void);
extern char *get_local_ip(void);

static void parse_udp_data(char *data, int len, unsigned char *addr, int port)
{
    if (NULL != strstr(data, "M-SEARCH"))
    {
        ESP_LOGI(TAG, "M-SEARCH");
        if (NULL != strstr(data, "urn:Belkin:device:**"))
        {
            ESP_LOGI(TAG, "response to search request ......");
            response_to_search(udpconn, addr, port);
        }
    }
}

static void alexa_udp_task(void *pvParameter)
{
    struct netbuf *recvbuf;
    struct pbuf *q;

    if (udp_connected)
    {
        while (1)
        {
            int data_len = 0;
            netconn_recv(udpconn, &recvbuf);
            if (recvbuf != NULL)
            {
                int fromport = netbuf_fromport(recvbuf);
                ip_addr_t *addr = netbuf_fromaddr(recvbuf);
                unsigned char n_addr[4] = {0};
                ip_addr_to_num(n_addr, addr);
                ESP_LOGI(TAG, "==> received from addr:%d.%d.%d.%d, port:%d", n_addr[0], n_addr[1], n_addr[2], n_addr[3],
                         fromport);
                // received data
                memset(udp_buff, 0, RECV_BUFFSIZE + 1);
                for (q = recvbuf->p; q != NULL; q = q->next)
                {
                    if (q->len > (RECV_BUFFSIZE - data_len))
                    {
                        memcpy(udp_buff + data_len, q->payload, (RECV_BUFFSIZE - data_len));
                    }
                    else
                    {
                        memcpy(udp_buff + data_len, q->payload, q->len);
                    }
                    data_len += q->len;
                    if (data_len > RECV_BUFFSIZE)
                    {
                        ESP_LOGE(TAG, "receive buffer overflow!!!");
                        break;
                    }
                }
                // ESP_LOGI(TAG, "==> received: %s", udp_buff);
                parse_udp_data(udp_buff, data_len, n_addr, fromport);
                netbuf_delete(recvbuf);  // do not forget to delete netbuf
            }
            delay_ms(50);
        }
    }
    vTaskDelete(NULL);
}

static bool connect_udp(void)
{
    ESP_LOGI(TAG, "connecting to UDP");
    udpconn = netconn_new(NETCONN_UDP);
    if (udpconn == NULL)
    {
        ESP_LOGE(TAG, "new netconn failed");
        return false;
    }

    err_t err = netconn_bind(udpconn, IP_ADDR_ANY, UDP_PORT);
    if (err != 0)
    {
        ESP_LOGE(TAG, "netconn bind failed");
        return false;
    }

    ip_addr_t destipaddr;
    IP_ADDR4(&destipaddr, 239, 255, 255, 250);
    /*err = netconn_connect(udpconn, &destipaddr, UDP_PORT);
    if (err != 0) {
            ESP_LOGE(TAG, "netconn connect failed");
            return false;
    }*/

    unsigned char *ip_num = get_ip_num();
    ip_addr_t loc;
    ESP_LOGI(TAG, "local ip: %d.%d.%d.%d", ip_num[0], ip_num[1], ip_num[2], ip_num[3]);
    IP_ADDR4(&loc, ip_num[0], ip_num[1], ip_num[2], ip_num[3]);
    if (igmp_joingroup(&loc, &destipaddr) != ERR_OK)
    {
        ESP_LOGE(TAG, "igmp join group failed");
        return false;
    }

    xTaskCreate(&alexa_udp_task, "alexa_udp_task", ALEXA_UDP_TASK_STACK_SIZE, NULL, ALEXA_UDP_TASK_PRIO,
                &udp_tsk_handle);

    return true;
}

void upnp_broadcast_responder_init(void)
{
    ESP_LOGI(TAG, "upnp broadcast responder init");
    udp_connected = connect_udp();
    if (!udp_connected)
    {
        ESP_LOGE(TAG, "udp connect failed!");
        return;
    }
}
