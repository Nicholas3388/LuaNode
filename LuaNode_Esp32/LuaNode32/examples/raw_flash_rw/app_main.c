#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "flash.h"
#include "nvs_flash.h"
#include "platform.h"
#include "tmr.h"

#define START_ADDR 0x110000
#define USED_FLASH_SIZE 2048
#define BUFF_SIZE 12

void led_blink(void)
{
    int res = platform_gpio_mode(2, 2);  // pin mode: OUTPUT
    if (res < 0)
    {
        printf("Led lightup failed\n");
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

void app_main()
{
    nvs_flash_init();
    flash_init();
    led_blink();

    flash_config *fc = flash_get_config();
    fc->hal_erase_f(START_ADDR, USED_FLASH_SIZE);
    uint8_t str[10] = "helloworld";
    fc->hal_write_f(START_ADDR, strlen(str), str);

    tmr_delay_msec(1000);
    uint8_t buff[BUFF_SIZE];
    memset(buff, 0, BUFF_SIZE);
    fc->hal_read_f(START_ADDR, strlen(str), buff);
    printf("==> Read content: %s\n", buff);

    return 0;
}
