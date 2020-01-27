#include "my_uart.h"

#include <stdio.h>
#include <stdlib.h>

#include "c_string.h"
#include "c_types.h"
#include "driver/uart.h"
#include "esp_intr.h"
#include "esp_log.h"
#include "extras/esp_intr_ext.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lua.h"
#include "rom/uart.h"
#include "soc/uart_register.h"

#define UART_FRM_ERR_INT_ST (BIT(3))
#define UART_FRM_ERR_INT_CLR (BIT(3))
#define UART_INT_CLR(i) (REG_UART_BASE(i) + 0x10)
#define UART_FIFO(i) (REG_UART_BASE(i) + 0x0)
#define UART_RXFIFO_FULL_INT_ST (BIT(0))
#define UART_RXFIFO_FULL_INT_CLR (BIT(0))
#define UART_RXFIFO_TOUT_INT_ST (BIT(8))
#define UART_RXFIFO_TOUT_INT_CLR (BIT(8))
#define UART_TXFIFO_EMPTY_INT_CLR (BIT(1))
#define UART_TXFIFO_EMPTY_INT_ENA (BIT(1))
#define UART_INT_ENA(i) (REG_UART_BASE(i) + 0xC)
#define UART_TXFIFO_EMPTY_INT_ST (BIT(1))
#define UART_RXFIFO_OVF_INT_CLR (BIT(4))
#define UART_RXFIFO_OVF_INT_ST (BIT(4))

#define EX_UART_NUM UART_NUM_0

#define TAG "my_uart"

enum
{
    UART_EVENT_RX_CHAR,
    // UART_EVENT_MAX
};

typedef struct _os_event_
{
    uint32 event;
    uint32 param;
} os_event_t;

xTaskHandle xUartTaskHandle;
xQueueHandle xQueueUart;
RcvMsgBuff rcvMsgBuff;

static uint8_t pbuff[RX_BUFF_SIZE] = {0};

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    memset(pbuff, 0, RX_BUFF_SIZE);
    while (1)
    {
        /* Waiting for UART event.
           If it happens then print out information what is it */
        if (xQueueReceive(xQueueUart, (void *)&event, (portTickType)portMAX_DELAY))
        {
            // ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch (event.type)
            {
                case UART_DATA:
                {
                    /* Event of UART receiving data
                     * We'd better handler data event fast, there would be much
                     * more data events than other types of events. If we take
                     * too much time on data event, the queue might be full. In
                     * this example, we don't process data in event, but read
                     * data outside.
                     */
                    uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                    // ESP_LOGI(TAG, "data, len: %d; buffered len: %d",
                    // event.size, buffered_size);
                    int len = uart_read_bytes(EX_UART_NUM, pbuff, RX_BUFF_SIZE, 100 / portTICK_RATE_MS);
                    if (len > 0)
                    {
                        // ESP_LOGI(TAG, "data: %s", pbuff);
                        uint8 RcvChar;
                        for (int i = 0; i < len; i++)
                        {
                            RcvChar = pbuff[i];
                            *(rcvMsgBuff.pWritePos) = RcvChar;
                            if (RcvChar == '\r' || RcvChar == '\n')
                            {
                                rcvMsgBuff.BuffState = WRITE_OVER;
                            }

                            if (rcvMsgBuff.pWritePos == (rcvMsgBuff.pRcvMsgBuff + RX_BUFF_SIZE))
                            {
                                // overflow ...we may need more error handle
                                // here.
                                rcvMsgBuff.pWritePos = rcvMsgBuff.pRcvMsgBuff;
                            }
                            else
                            {
                                rcvMsgBuff.pWritePos++;
                            }

                            if (rcvMsgBuff.pWritePos == rcvMsgBuff.pReadPos)
                            {  // overflow one byte, need push pReadPos one byte
                               // ahead
                                if (rcvMsgBuff.pReadPos == (rcvMsgBuff.pRcvMsgBuff + RX_BUFF_SIZE))
                                {
                                    rcvMsgBuff.pReadPos = rcvMsgBuff.pRcvMsgBuff;
                                }
                                else
                                {
                                    rcvMsgBuff.pReadPos++;
                                }
                            }
                        }

                        lua_handle_input(false);
                    }
                    break;
                }
                case UART_FIFO_OVF:
                {
                    ESP_LOGE(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding
                    // flow control for your application. We can read data out
                    // out the buffer, or directly flush the Rx buffer.
                    uart_flush(EX_UART_NUM);
                    break;
                }
                case UART_BUFFER_FULL:
                {
                    ESP_LOGE(TAG, "ring buffer full");
                    // If buffer full happened, you should consider increasing
                    // your buffer size We can read data out out the buffer, or
                    // directly flush the Rx buffer.
                    uart_flush(EX_UART_NUM);
                    break;
                }
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break detected");
                    break;
                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "uart parity error");
                    break;
                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "uart frame error");
                    break;
                case UART_PATTERN_DET:
                    ESP_LOGI(TAG, "uart pattern detected");
                    break;
                default:
                    ESP_LOGE(TAG, "not serviced uart event type: %d\n", event.type);
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

void uart_sendStr(const char *str)
{
    while (*str) { uart_tx_one_char(*str++); }
}

void uart_putc(uint8_t TxChar)
{
    while (true)
    {
        uint32 fifo_cnt = READ_PERI_REG(UART_STATUS_REG(0)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);

        if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126)
        {
            break;
        }
    }

    WRITE_PERI_REG(UART_FIFO(0), TxChar);
}

void uart_init(void)
{
    printf("UART init ...\r\n");
    rcvMsgBuff.pRcvMsgBuff = malloc(RX_BUFF_SIZE);
    memset(rcvMsgBuff.pRcvMsgBuff, 0, RX_BUFF_SIZE);
    rcvMsgBuff.pWritePos = rcvMsgBuff.pRcvMsgBuff;
    rcvMsgBuff.pReadPos = rcvMsgBuff.pRcvMsgBuff;

    uart_config_t uart_config = {.baud_rate = 115200,
                                 .data_bits = UART_DATA_8_BITS,
                                 .parity = UART_PARITY_DISABLE,
                                 .stop_bits = UART_STOP_BITS_1,
                                 .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(EX_UART_NUM, &uart_config);
    // Set UART pins using UART0 default pins i.e. no changes
    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(EX_UART_NUM, RX_BUFF_SIZE * 2, RX_BUFF_SIZE * 2, 10, &xQueueUart, 0);

    // Set uart pattern detection function
    uart_enable_pattern_det_intr(EX_UART_NUM, '+', 3, 10000, 10, 10);

    // Create a task to handle uart event from ISR
    xTaskCreate(uart_event_task, "uart_event_task", 8192, NULL, 12, &xUartTaskHandle);
}
