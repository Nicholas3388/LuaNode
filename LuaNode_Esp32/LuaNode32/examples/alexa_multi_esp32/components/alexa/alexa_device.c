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

#include "alexa_device.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "lwip/igmp.h"
#include "lwip/sockets.h"
#include "user_config.h"
#include "utils.h"

#define RECV_BUFFSIZE 1024
#define MAX_HTTP_CLIENT_CONN 1

static const char *TAG = "alexa_dev";
static alexa_dev_t root;
// static char *persistent_uuid =
// "Socket-1_0-38323636-4558-4dda-9188-cda0e6332211";
static char *persistent_uuid = "Socket-1_0-38323636-4558-4dda-9188-cda0e63322";

static char text[RECV_BUFFSIZE + 1] = {0};

extern unsigned char *get_ip_num(void);
extern char *get_local_ip(void);

////////////////////////////////////////////////
// device list operations
void alexa_device_remove(alexa_dev_t *dev)
{
    if (dev == NULL)
    {
        return;
    }

    alexa_dev_t *tmp = root.next;
    alexa_dev_t *prev = &root;
    int found = 0;
    while (tmp != NULL)
    {
        if (tmp == dev)
        {
            found = 1;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    if (found == 1)
    {
        ESP_LOGI(TAG, "remove device");
        prev = tmp->next;
        vTaskDelete(tmp->task_handle);
        free(tmp);
    }
}

void alexa_device_add(alexa_dev_t *dev)
{
    if (dev == NULL)
    {
        return;
    }

    alexa_dev_t *tmp = &root;
    while (tmp->next != NULL) { tmp = tmp->next; }
    tmp->next = dev;
}

void alexa_device_list_init(void)
{
    memset((void *)&root, 0, sizeof(alexa_dev_t));
    root.next = NULL;
}

alexa_dev_t *alexa_device_get_dev_by_index(int index)
{
    alexa_dev_t *tmp = root.next;
    while (tmp != NULL)
    {
        if (tmp->index == index)
        {
            return tmp;
        }
        tmp = tmp->next;
    }
    return NULL;
}

int alexa_device_get_num(void)
{
    int num = 0;
    alexa_dev_t *tmp = root.next;
    while (tmp != NULL)
    {
        num++;
        tmp = tmp->next;
    }
    return num;
}

void alexa_device_list_destroy(void)
{
    alexa_dev_t *tmp = NULL;
    while (root.next != NULL)
    {
        tmp = root.next;
        root.next = tmp->next;
        if (tmp->task_handle != NULL)
        {
            vTaskDelete(tmp->task_handle);
        }
        free(tmp);
    }
}

void alexa_device_list_traverse(void (*cb)(alexa_dev_t *dev))
{
    alexa_dev_t *tmp = root.next;
    while (tmp != NULL)
    {
        if (cb != NULL)
        {
            cb(tmp);
        }
        tmp = tmp->next;
    }
}

////////////////////////////////////////////////
//
void response_to_search(struct netconn *udpconn, unsigned char *addr, int port)
{
    char resp[768] = {0};
    char *local_ip = get_local_ip();

    alexa_dev_t *tmp = root.next;
    while (tmp != NULL)
    {
        int local_port = tmp->local_port;
        char p[4] = {0};
        sprintf(p, "%d", local_port);
        sprintf(resp,
                "HTTP/1.1 200 OK\r\n"
                "CACHE-CONTROL: max-age=86400\r\n"
                "DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
                "EXT:\r\n"
                "LOCATION: http://%s:%s/setup.xml\r\n"
                "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
                "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
                "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
                "ST: urn:Belkin:device:**\r\n"
                "USN: uuid:%s%s::urn:Belkin:device:**\r\n"
                "X-User-Agent: redsonic\r\n\r\n",
                local_ip, p, persistent_uuid, p);

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

        tmp = tmp->next;
    }
}

static int parse_http_request(int sock, char *data, int len, int port, alexa_dev_t *dev)
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
            (dev->turn_on_cb)();
        }
        if (NULL != strstr(data, "<BinaryState>0</BinaryState>"))
        {
            ESP_LOGI(TAG, "turn off LED");
            (dev->turn_off_cb)();
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
        char p[4] = {0};
        sprintf(p, "%d", port);
        sprintf(setup_xml,
                "<?xml version=\"1.0\"?>"
                "<root>"
                "<device>"
                "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
                "<friendlyName>%s</friendlyName>"
                "<manufacturer>Belkin International Inc.</manufacturer>"
                "<modelName>Emulated Socket</modelName>"
                "<modelNumber>3.1415</modelNumber>"
                "<UDN>uuid:%s%s</UDN>"
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
                dev->dev_name, persistent_uuid, p);
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

static void task_http_server_client(int client_sock, int port, alexa_dev_t *dev)
{
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
            if (parse_http_request(client_sock, text, recbytes, port, dev) == 0)
            {
                break;
            }
        }
        else if (recbytes == 0)
        {
            break;
        }
    }

    ESP_LOGI(TAG, "task_http_server_client > close socket:(%d) port: (%d)\n", client_sock, port);
    close(client_sock);
}

static void alexa_http_task(void *pvParameter)
{
    int task_index = (int)pvParameter;
    ESP_LOGI(TAG, "Starting ALEXA http task %d ...", task_index);
    alexa_dev_t *dev_t = alexa_device_get_dev_by_index(task_index);
    if (dev_t == NULL)
    {
        ESP_LOGE(TAG, "alexa struct not found!");
        return;
    }
    else
    {
        ESP_LOGI(TAG, "got struct");
    }
    int local_port = dev_t->local_port;
    ESP_LOGI(TAG, "port : %d", local_port);

    int32_t listenfd;
    struct sockaddr_in server_addr, remote_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_len = sizeof(server_addr);
    server_addr.sin_port = htons(local_port);

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
        ESP_LOGI(TAG, "http wait client %d", task_index);
        if ((dev_t->sock_id = accept(listenfd, (struct sockaddr *)&remote_addr, (socklen_t *)&len)) < 0)
        {
            ESP_LOGI(TAG, "socket accept failed");
            delay_ms(1000);
            continue;
        }

        printf("ESP32 HTTP server task > Client from %s %d\n", inet_ntoa(remote_addr.sin_addr),
               htons(remote_addr.sin_port));
        // xTaskCreate(task_http_server_client, "task_http_server_client", 1024,
        //		&client_sock, 1, NULL);
        task_http_server_client(dev_t->sock_id, local_port, dev_t);
    }

    vTaskDelete(NULL);
}

alexa_dev_t *create_new_device(const char *name, int port, void (*on_cb)(void), void (*off_cb)(void))
{
    ESP_LOGI(TAG, "create new alexa device");

    alexa_dev_t *new_dev = (alexa_dev_t *)malloc(sizeof(alexa_dev_t));
    if (new_dev == NULL)
    {
        ESP_LOGE(TAG, "malloc for device struct failed!");
        return NULL;
    }
    else
    {
        ESP_LOGI(TAG, "alexa device malloc OK");
    }

    memset(new_dev->dev_name, 0, 20);
    strcpy(new_dev->dev_name, name);
    new_dev->local_port = port;
    new_dev->turn_on_cb = on_cb;
    new_dev->turn_off_cb = off_cb;
    new_dev->task_handle = NULL;
    new_dev->next = NULL;
    ESP_LOGI(TAG, "dev set OK");

    int index = alexa_device_get_num() + 1;
    new_dev->index = index;
    ESP_LOGI(TAG, "new device index: %d", new_dev->index);
    alexa_device_add(new_dev);
    xTaskCreate(&alexa_http_task, "alexa_http_task", ALEXA_TASK_STACK_SIZE, (void *)index, ALEXA_TASK_PRIO,
                &(new_dev->task_handle));

    return new_dev;
}
