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

#include "alexa.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "lwip/api.h"
#include "lwip/igmp.h"
#include "lwip/sockets.h"
#include "user_config.h"
#include "utils.h"

#define RECV_BUFFSIZE 1024
#define UDP_PORT 1900
#define MAX_HTTP_CLIENT_CONN 1

static const char *TAG = "alexa";

static char text[RECV_BUFFSIZE + 1] = {0};
static char udp_buff[RECV_BUFFSIZE + 1] = {0};
/*socket id*/
static int sock_id = -1;

static char *device_name = "esp";
static char *persistent_uuid = "Socket-1_0-38323636-4558-4dda-9188-cda0e6332211";
static bool udp_connected = false;
static struct netconn *udpconn = NULL;

extern unsigned char *get_ip_num(void);
extern char *get_local_ip(void);
static void alexa_udp_task(void *pvParameter);

static void turn_on_led()
{
    gpio_set_level(GPIO_TEST, 1);  // turn on relay with voltage HIGH
}

static void turn_off_led()
{
    gpio_set_level(GPIO_TEST, 0);  // turn off relay with voltage LOW
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

    xTaskCreate(&alexa_udp_task, "alexa_udp_task", ALEXA_UDP_TASK_STACK_SIZE, NULL, ALEXA_UDP_TASK_PRIO, NULL);

    return true;
}

static void response_to_search(unsigned char *addr, int port)
{
    char resp[768] = {0};
    char *local_ip = get_local_ip();
    sprintf(resp,
            "HTTP/1.1 200 OK\r\n"
            "CACHE-CONTROL: max-age=86400\r\n"
            "DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
            "EXT:\r\n"
            "LOCATION: http://%s:80/setup.xml\r\n"
            "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
            "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
            "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
            "ST: urn:Belkin:device:**\r\n"
            "USN: uuid:%s::urn:Belkin:device:**\r\n"
            "X-User-Agent: redsonic\r\n\r\n",
            local_ip, persistent_uuid);

    struct netbuf *sentbuf = netbuf_new();
    if (sentbuf == NULL)
    {
        ESP_LOGE(TAG, "netbuf alloc failed");
        return;
    }
    netbuf_alloc(sentbuf, strlen(resp));
    memcpy(sentbuf->p->payload, (void *)resp, strlen(resp));
    ip_addr_t dest;
    IP_ADDR4(&dest, addr[0], addr[1], addr[2], addr[3]);
    err_t err = netconn_sendto(udpconn, sentbuf, &dest, port);
    if (err != 0)
    {
        ESP_LOGE(TAG, "udp send data failed!");
    }
    ESP_LOGI(TAG, "sent: %s", resp);
    netbuf_delete(sentbuf);
}

static void parse_udp_data(char *data, int len, unsigned char *addr, int port)
{
    if (NULL != strstr(data, "M-SEARCH"))
    {
        ESP_LOGI(TAG, "M-SEARCH");
        if (NULL != strstr(data, "urn:Belkin:device:**"))
        {
            ESP_LOGI(TAG, "response to search request ......");
            response_to_search(addr, port);
        }
    }
}

static int parse_http_request(int sock, char *data, int len)
{
    if (NULL != strstr(data, "/index.html"))
    {
        ESP_LOGI(TAG, "response to index.xml");
        char *header =
            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
            "5\r\nConnection: close\r\n\r\n";  // 5 is the body length, if you
                                               // change body content,
        write(sock, header, strlen(header));
        char *body = "hello";
        write(sock, body, strlen(body));
    }
    else if (NULL != strstr(data, "/eventservice.xml"))
    {
        ESP_LOGI(TAG, "response to eventservice.xml");
        char *eventservice_xml =
            "<?scpd xmlns=\"urn:Belkin:service-1-0\"?>"
            "<actionList>"
            "<action>"
            "<name>SetBinaryState</name>"
            "<argumentList>"
            "<argument>"
            "<retval/>"
            "<name>BinaryState</name>"
            "<relatedStateVariable>BinaryState</relatedStateVariable>"
            "<direction>in</direction>"
            "</argument>"
            "</argumentList>"
            "<serviceStateTable>"
            "<stateVariable sendEvents=\"yes\">"
            "<name>BinaryState</name>"
            "<dataType>Boolean</dataType>"
            "<defaultValue>0</defaultValue>"
            "</stateVariable>"
            "<stateVariable sendEvents=\"yes\">"
            "<name>level</name>"
            "<dataType>string</dataType>"
            "<defaultValue>0</defaultValue>"
            "</stateVariable>"
            "</serviceStateTable>"
            "</action>"
            "</scpd>\r\n"
            "\r\n";
        char header[256] = {0};
        int body_len = strlen(eventservice_xml);
        sprintf(header,
                "HTTP/1.1 200 OK\r\nContent-Type: "
                "text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
                body_len);
        write(sock, header, strlen(header));
        write(sock, eventservice_xml, body_len);
    }
    else if (NULL != strstr(data, "/upnp/control/basicevent1"))
    {
        ESP_LOGI(TAG, "response to /upnp/control/basicevent1");
        if (NULL != strstr(data, "<BinaryState>1</BinaryState>"))
        {
            ESP_LOGI(TAG, "turn on LED");
            turn_on_led();
        }
        if (NULL != strstr(data, "<BinaryState>0</BinaryState>"))
        {
            ESP_LOGI(TAG, "turn off LED");
            turn_off_led();
        }
        // you must response to Alexa when you finish your operation, otherwise,
        // Alexa will say something like "You device is poweroff"
        char *header =
            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
            "0\r\nConnection: close\r\n\r\n";
        write(sock, header, strlen(header));
    }
    else if (NULL != strstr(data, "/setup.xml"))
    {
        ESP_LOGI(TAG, "response to setup.xml");
        char setup_xml[1024] = {0};
        sprintf(setup_xml,
                "<?xml version=\"1.0\"?>"
                "<root>"
                "<device>"
                "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
                "<friendlyName>%s</friendlyName>"
                "<manufacturer>Belkin International Inc.</manufacturer>"
                "<modelName>Emulated Socket</modelName>"
                "<modelNumber>3.1415</modelNumber>"
                "<UDN>uuid:%s</UDN>"
                "<serialNumber>221517K0101769</serialNumber>"
                "<binaryState>0</binaryState>"
                "<serviceList>"
                "<service>"
                "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
                "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
                "<controlURL>/upnp/control/basicevent1</controlURL>"
                "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                "<SCPDURL>/eventservice.xml</SCPDURL>"
                "</service>"
                "</serviceList>"
                "</device>"
                "</root>\r\n"
                "\r\n",
                device_name, persistent_uuid);
        char header[256] = {0};
        int body_len = strlen(setup_xml);
        sprintf(header,
                "HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\nContent-Length: "
                "%d\r\nConnection: close\r\n\r\n",
                body_len);
        write(sock, header, strlen(header));
        write(sock, setup_xml, body_len);
    }
    else
    {
        ESP_LOGE(TAG, "unknown http request?");
    }
    return 0;
}

static void task_http_server_client(void *inFd)
{
    int32_t client_sock = *(int *)inFd;
    int recbytes = 0;

    ESP_LOGI(TAG, "setup socket:(%d)", client_sock);

    // int nNetTimeout = 5;
    // setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (char * )&nNetTimeout,
    //		sizeof(int)); //non-block
    while (1)
    {
        recbytes = read(client_sock, text, RECV_BUFFSIZE);
        if (recbytes > 0)
        {
            text[recbytes] = 0;
            ESP_LOGI(TAG, "(socket:%d) recieve:(%d): %s", client_sock, recbytes, text);
            if (parse_http_request(client_sock, text, recbytes) == 0)
            {
                break;
            }
        }
        else if (recbytes == 0)
        {
            break;
        }
    }

    ESP_LOGI(TAG, "task_http_server_client > close socket:(%d)\n", client_sock);
    close(client_sock);
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

static void alexa_http_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting ALEXA http task...");

    int32_t listenfd;
    struct sockaddr_in server_addr, remote_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_len = sizeof(server_addr);
    server_addr.sin_port = htons(HTTP_SRV_PORT);

    while ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        ESP_LOGI(TAG, "http socket create failed");
        delay_ms(1000);
    }

    while (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        ESP_LOGI(TAG, "http socket bind failed");
        delay_ms(1000);
    }

    while (listen(listenfd, MAX_HTTP_CLIENT_CONN) != 0)
    {
        ESP_LOGI(TAG, "http socket listen failed");
        delay_ms(1000);
    }

    int32_t len = sizeof(struct sockaddr_in);

    while (1)
    {
        ESP_LOGI(TAG, "http wait client");
        if ((sock_id = accept(listenfd, (struct sockaddr *)&remote_addr, (socklen_t *)&len)) < 0)
        {
            ESP_LOGI(TAG, "socket accept failed");
            delay_ms(1000);
            continue;
        }

        printf("ESP32 HTTP server task > Client from %s %d\n", inet_ntoa(remote_addr.sin_addr),
               htons(remote_addr.sin_port));
        // xTaskCreate(task_http_server_client, "task_http_server_client", 1024,
        //		&client_sock, 1, NULL);
        task_http_server_client(&sock_id);
    }

    vTaskDelete(NULL);
}

void alexa_init(void)
{
    udp_connected = connect_udp();
    if (!udp_connected)
    {
        ESP_LOGE(TAG, "udp connect failed!");
        return;
    }
    xTaskCreate(&alexa_http_task, "alexa_http_task", ALEXA_TASK_STACK_SIZE, NULL, ALEXA_TASK_PRIO, NULL);
}
