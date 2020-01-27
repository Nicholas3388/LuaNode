#ifndef _ESP_INTR_EXT_H_
#define _ESP_INTR_EXT_H_

#include "esp_intr.h"
#include "soc_ext.h"

#define ESP_UART0_INTR_ENABLE() \
    ESP_INTR_ENABLE(ETS_UART0_INUM)

#define ESP_UART0_INTR_DISABLE() \
    ESP_INTR_DISABLE(ETS_UART0_INUM)

#endif