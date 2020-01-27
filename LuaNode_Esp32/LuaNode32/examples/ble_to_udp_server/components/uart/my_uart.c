/**
 * Nicholas3388
 * 2017.04.12
 */

#include "my_uart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ble.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "soc/uart_struct.h"
#include "user_config.h"
#include "utils.h"

static const char *TAG = "uart";
QueueHandle_t uart0_queue;
RcvMsgBuff rcvMsg;
inputStatus status = NONE;

extern unsigned char wifi_ssid[WIFI_SSID_MAX_LEN];
extern unsigned char wifi_pass[WIFI_PASS_MAX_LEN];
extern unsigned char srv_ip[SRV_IP_MAX_LEN];
extern int srv_port;

extern void initialise_wifi(void);
extern void clear_storage(void);
extern void close_nvs(void);
extern esp_err_t save_string(const char *key, const char *str);
extern esp_err_t save_int32(const char *key, int data);

void show_help_info()
{
    ESP_LOGI(TAG, " ");
    ESP_LOGI(TAG, "----------------------------------------");
    ESP_LOGI(TAG, "input the following command to setup:");
    ESP_LOGI(TAG, "  ssid - set AP ssid");
    ESP_LOGI(TAG, "  pass - set password");
    ESP_LOGI(TAG, "  ip   - set server ip");
    ESP_LOGI(TAG, "  port - set server port");
    ESP_LOGI(TAG, "  wifi - start connecting to wifi");
    ESP_LOGI(TAG, "  quit - reset wifi parameters");
    ESP_LOGI(TAG, "  help - show help info");
    ESP_LOGI(TAG, "----------------------------------------");
    ESP_LOGI(TAG, " ");
}

static bool is_valid_port(const char *port, int len)
{
    int i;
    if (len > 5)
    {
        ESP_LOGE(TAG, "invalid port!");
        return false;
    }

    if (str2num(port, len) > 65535)
    {
        ESP_LOGE(TAG, "port > 65535");
        return false;
    }

    for (i = 0; i < len; i++)
    {
        if (port[i] < '0' || port[i] > '9')
        {
            return false;
        }
    }
    return true;
}

static void uart_input_handler(const char *data, int len)
{
    int i;
    if (len < 1)
    {
        return;
    }

    if (data[0] == 0x7f || data[0] == 0x08)
    {
        if (rcvMsg.pWritePos != rcvMsg.pRcvMsgBuff)
        {
            const char back = 0x08, space = ' ';
            uart_write_bytes(UART_NUM_0, &back, 1);
            uart_write_bytes(UART_NUM_0, &space, 1);
            // uart_write_bytes(UART_NUM_0, &back, 1);
            rcvMsg.pWritePos[0] = 0;
            rcvMsg.pWritePos -= 1;
        }
        else
        {
            // nothing to delete
            return;
        }
    }

    if (data[0] == '\r' || data[0] == '\n')
    {
        // enter key press
        rcvMsg.pWritePos[0] = 0;

        if (rcvMsg.pWritePos == rcvMsg.pRcvMsgBuff)
        {
            uart_write_bytes(UART_NUM_0, PROMPT, PROMPT_LEN);
        }
        else
        {
            int length = rcvMsg.pWritePos - rcvMsg.pRcvMsgBuff;
            uart_write_bytes(UART_NUM_0, PROMPT, PROMPT_LEN);
            if (status == INPUT_SSID)
            {
                memset(wifi_ssid, 0, WIFI_SSID_MAX_LEN);
                memcpy(wifi_ssid, rcvMsg.pRcvMsgBuff, length);
                status = NONE;
                ESP_LOGI(TAG, "you set ssid: %s", wifi_ssid);
                esp_err_t er = save_string(CMD_SSID, (char *)wifi_ssid);
                if (er != ESP_OK)
                    ESP_LOGE(TAG, "save ssid error");
                goto set_end;
            }
            else if (status == INPUT_PASS)
            {
                memset(wifi_pass, 0, WIFI_PASS_MAX_LEN);
                memcpy(wifi_pass, rcvMsg.pRcvMsgBuff, length);
                status = NONE;
                ESP_LOGI(TAG, "you set password: %s", wifi_pass);
                esp_err_t er = save_string(CMD_PASS, (char *)wifi_pass);
                if (er != ESP_OK)
                    ESP_LOGE(TAG, "save pass error");
                goto set_end;
            }
            else if (status == INPUT_IP)
            {
                memset(srv_ip, 0, SRV_IP_MAX_LEN);
                memcpy(srv_ip, rcvMsg.pRcvMsgBuff, length);
                status = NONE;
                ESP_LOGI(TAG, "you set server ip: %s", srv_ip);
                esp_err_t er = save_string(CMD_IP, (char *)srv_ip);
                if (er != ESP_OK)
                    ESP_LOGE(TAG, "save ip error");
                goto set_end;
            }
            else if (status == INPUT_PORT)
            {
                if (!is_valid_port((const char *)rcvMsg.pRcvMsgBuff, length))
                {
                    rcvMsg.pWritePos = rcvMsg.pRcvMsgBuff;
                    ESP_LOGE(TAG, "invalid server port, please input again");
                    return;
                }
                srv_port = str2num((const char *)rcvMsg.pRcvMsgBuff, length);
                status = NONE;
                ESP_LOGI(TAG, "you set server port: %d", srv_port);
                // esp_err_t er = save_int32(CMD_PORT, srv_port);
                esp_err_t er = save_string(CMD_PORT, (char *)rcvMsg.pRcvMsgBuff);
                if (er != ESP_OK)
                    ESP_LOGE(TAG, "save port error");
                goto set_end;
            }

            if (strncmp((const char *)rcvMsg.pRcvMsgBuff, CMD_SSID, length) == 0)
            {
                ESP_LOGI(TAG, "input your ssid [less than 32byte]:");
                status = INPUT_SSID;
            }
            else if (strncmp((const char *)rcvMsg.pRcvMsgBuff, CMD_PASS, length) == 0)
            {
                ESP_LOGI(TAG, "input your password [less than 32byte]:");
                status = INPUT_PASS;
            }
            else if (strncmp((const char *)rcvMsg.pRcvMsgBuff, CMD_IP, length) == 0)
            {
                ESP_LOGI(TAG, "input server IP:");
                status = INPUT_IP;
            }
            else if (strncmp((const char *)rcvMsg.pRcvMsgBuff, CMD_PORT, length) == 0)
            {
                ESP_LOGI(TAG, "input server port:");
                status = INPUT_PORT;
            }
            else if (strncmp((const char *)rcvMsg.pRcvMsgBuff, CMD_HELP, length) == 0)
            {
                show_help_info();
            }
            else if (strncmp((const char *)rcvMsg.pRcvMsgBuff, CMD_WIFI, length) == 0)
            {
                ESP_LOGI(TAG, "BLE scanning ......");
                close_nvs();
                ble_client_app_register();
                // initialise_wifi();
            }
            else if (strncmp((const char *)rcvMsg.pRcvMsgBuff, CMD_QUIT, length) == 0)
            {
                ESP_LOGI(TAG, "reset wifi info:");
                clear_storage();
                close_nvs();
                system_restart();
            }
            else
            {
                ESP_LOGI(TAG, "invalid command %s", rcvMsg.pRcvMsgBuff);
                show_help_info();
            }
        }
    set_end:
        rcvMsg.pWritePos = rcvMsg.pRcvMsgBuff;
        memset(rcvMsg.pRcvMsgBuff, 0, RECV_BUF_SIZE);
        return;
    }

    *(rcvMsg.pWritePos) = data[0];
    uart_write_bytes(UART_NUM_0, &data[0], 1);
    rcvMsg.pWritePos += 1;

    if (rcvMsg.pWritePos - rcvMsg.pRcvMsgBuff >= RECV_BUF_SIZE - 1)
    {
        rcvMsg.pWritePos = rcvMsg.pRcvMsgBuff;  // overflow, restart
        ESP_LOGI(TAG, "overflow");
    }
}

static void uart_task(void *pvParameters)
{
    int uart_num = UART_NUM_0;
    uart_event_t event;
    size_t buffered_size;
    uint8_t *dtmp = (uint8_t *)malloc(BUF_SIZE);
    for (;;)
    {
        // Waiting for UART event.
        if (xQueueReceive(uart0_queue, (void *)&event, (portTickType)portMAX_DELAY))
        {
            // ESP_LOGI(TAG, "uart[%d] event:", uart_num);
            switch (event.type)
            {
                // Event of UART receving data
                /*We'd better handler data event fast, there would be much more
                data events than other types of events. If we take too much time
                on data event, the queue might be full. in this example, we
                don't process data in event, but read data outside.*/
                case UART_DATA:
                    uart_get_buffered_data_len(uart_num, &buffered_size);
                    // ESP_LOGI(TAG, "data, len: %d; buffered len: %d",
                    // event.size, buffered_size);
                    break;
                // Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow\n");
                    // If fifo overflow happened, you should consider adding
                    // flow control for your application. We can read data out
                    // out the buffer, or directly flush the rx buffer.
                    uart_flush(uart_num);
                    break;
                // Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full\n");
                    // If buffer full happened, you should consider encreasing
                    // your buffer size We can read data out out the buffer, or
                    // directly flush the rx buffer.
                    uart_flush(uart_num);
                    break;
                // Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break\n");
                    break;
                // Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error\n");
                    break;
                // Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error\n");
                    break;
                // UART_PATTERN_DET
                case UART_PATTERN_DET:
                    ESP_LOGI(TAG, "uart pattern detected\n");
                    break;
                // Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d\n", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

static void uart_evt_task(void *pvParameters)
{
    int uart_num = UART_NUM_0;
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };
    // Set UART parameters
    uart_param_config(uart_num, &uart_config);
    // Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    // Install UART driver, and get the queue.
    uart_driver_install(uart_num, BUF_SIZE * 2, BUF_SIZE * 2, 10, &uart0_queue, 0);
    // Set UART pins,(-1: default pin, no change.)
    // For UART0, we can just use the default pins.
    // uart_set_pin(uart_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
    // UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); Set uart pattern detect
    // function.
    uart_enable_pattern_det_intr(uart_num, '+', 3, 10000, 10, 10);
    // Create a task to handler UART event from ISR
    xTaskCreate(uart_task, "uart_task", UART_TASK_STACK_SIZE, NULL, UART_TASK_PRIO, NULL);
    // process data
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
    do
    {
        int len = uart_read_bytes(uart_num, data, BUF_SIZE, 100 / portTICK_RATE_MS);
        if (len > 0)
        {
            // ESP_LOGI(TAG, "uart read : %d", len);
            // uart_write_bytes(uart_num, (const char*)data, len);
            uart_input_handler((const char *)data, len);
        }
    } while (1);
}

void uart_init(void)
{
    memset(&rcvMsg, 0, sizeof(rcvMsg));
    rcvMsg.RcvBuffSize = RECV_BUF_SIZE;
    rcvMsg.pRcvMsgBuff = NULL;
    rcvMsg.pRcvMsgBuff = (uint8_t *)malloc(RECV_BUF_SIZE);
    if (rcvMsg.pRcvMsgBuff == NULL)
    {
        ESP_LOGE(TAG, "receive buffer alloc failed");
        return;
    }
    rcvMsg.pReadPos = rcvMsg.pRcvMsgBuff;
    rcvMsg.pWritePos = rcvMsg.pRcvMsgBuff;

    xTaskCreate(uart_evt_task, "uart_evt_task", UART_EVT_TASK_STACK_SIZE, NULL, UART_EVT_TASK_PRIO, NULL);
}