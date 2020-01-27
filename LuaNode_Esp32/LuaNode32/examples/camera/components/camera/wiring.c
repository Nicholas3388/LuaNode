#include "wiring.h"

#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"

void pinMode(int pin, int mode)
{
    gpio_config_t conf = {0};
    conf.pin_bit_mask = 1LL << pin;
    if (mode == OUTPUT)
    {
        conf.mode = GPIO_MODE_OUTPUT;
    }
    if (mode == INPUT || mode == INPUT_PULLUP)
    {
        conf.mode = GPIO_MODE_INPUT;
    }
    if (mode == INPUT)
    {
        conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        conf.pull_up_en = GPIO_PULLUP_DISABLE;
    }
    else if (mode == INPUT_PULLUP)
    {
        conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        conf.pull_up_en = GPIO_PULLUP_ENABLE;
    }
    gpio_config(&conf);
}

void digitalWrite(int pin, int value) { gpio_set_level(pin, (value) ? 1 : 0); }

void delay(int millis) { vTaskDelay(millis / portTICK_PERIOD_MS); }
