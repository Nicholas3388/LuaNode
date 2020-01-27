#ifndef MY_UART_H
#define MY_UART_H

#include "c_types.h"

//#define REG_UART_BASE( i )  (0x60000000+(i)*0x10000)
#define UART_INT_ST( i )                        (REG_UART_BASE( i ) + 0x8)

typedef enum {
    UART_WordLength_5b = 0x0,
    UART_WordLength_6b = 0x1,
    UART_WordLength_7b = 0x2,
    UART_WordLength_8b = 0x3
} UART_WordLength;

typedef enum {
    USART_StopBits_1   = 0x1,
    USART_StopBits_1_5 = 0x2,
    USART_StopBits_2   = 0x3,
} UART_StopBits;

void uart_init(void);
void uart_sendStr(const char *str);
void uart_putc(uint8_t TxChar);

#endif