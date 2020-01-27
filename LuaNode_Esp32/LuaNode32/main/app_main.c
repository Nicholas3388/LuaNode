
#include <stdio.h>
#include <string.h>

#include "esp_spi_flash.h"
#include "esp_system.h"
#include "flash_api.h"
#include "flash_fs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lua.h"
#include "my_uart.h"
#include "nodemcu_esp_event.h"
#include "nvs_flash.h"
#include "platform.h"
#include "task/task.h"
#include "vfs.h"
#ifdef USE_THREAD_MODULE
#include "pthreadx.h"
#endif
#include "tmr.h"

extern nodemcu_esp_event_reg_t esp_event_cb_table;

static task_handle_t esp_event_task;
static QueueHandle_t esp_event_queue;

void led_blink(void)
{
    int res = platform_gpio_mode(2, PLATFORM_GPIO_OUTPUT, 0);  // pin mode: OUTPUT
    if (res < 0)
    {
        printf("Led lightup failed\n");
        return;
    }
    platform_gpio_write(2, 1);  // led on
    vTaskDelay(250 / portTICK_RATE_MS);
    platform_gpio_write(2, 0);  // led off
    tmr_delay_msec(250);
    platform_gpio_write(2, 1);
    tmr_delay_msec(250);
    platform_gpio_write(2, 0);
    tmr_delay_msec(250);
    platform_gpio_write(2, 1);
}

esp_err_t esp_event_send(system_event_t *event)
{
    if (!event)
        return ESP_ERR_INVALID_ARG;

    if (!esp_event_task || !esp_event_queue)
        return ESP_ERR_INVALID_STATE;  // too early!

    portBASE_TYPE ret = xQueueSendToBack(esp_event_queue, event, 0);
    if (ret != pdPASS)
    {
        NODE_ERR("failed to queue esp event %d", event->event_id);
        return ESP_FAIL;
    }

    // If the task_post() fails, it only means the event gets delayed, hence
    // we claim OK regardless.
    task_post_medium(esp_event_task, 0);
    return ESP_OK;
}

static void handle_esp_event(task_param_t param, task_prio_t prio)
{
    (void)param;
    (void)prio;

    system_event_t evt;
    while (xQueueReceive(esp_event_queue, &evt, 0) == pdPASS)
    {
        esp_err_t ret = esp_event_process_default(&evt);
        if (ret != ESP_OK)
            NODE_ERR("default event handler failed for %d", evt.event_id);

        nodemcu_esp_event_reg_t *evregs;
        for (evregs = &esp_event_cb_table; evregs->callback; ++evregs)
        {
            if (evregs->event_id == evt.event_id)
                evregs->callback(&evt);
        }
    }
}

void app_main()
{
    uart_init();

    esp_event_queue = xQueueCreate(CONFIG_SYSTEM_EVENT_QUEUE_SIZE, sizeof(system_event_t));
    esp_event_task = task_get_id(handle_esp_event);

    if (flash_safe_get_size_byte() != flash_rom_get_size_byte())
    {
        printf("Incorrect flash size reported, adjusting...\n");
        flash_rom_set_size_byte(flash_safe_get_size_byte());
        esp_restart();
        return;
    }

    printf("Mounting flash filesystem...\n");
    if (!vfs_mount("/FLASH", 0))
    {
        // Failed to mount -- try reformat
        printf("Formatting file system. Please wait...\n");
        if (!vfs_format())
        {
            printf("*** ERROR ***: unable to format. FS might be compromised.\n");
            printf("It is advised to re-flash the NodeMCU image.\n");
        }
        // Note that fs_format leaves the file system mounted
    }

    nvs_flash_init();
    tcpip_adapter_init();
    platform_init();
#ifdef USE_TMR_MODULE
    tmr_init();
#endif

    printf("\n=======================================\n");
    printf("LuaNode: https://github.com/Nicholas3388/LuaNode\n");
    printf("Version: %s\n", VERSION);
    printf("=======================================\n\n");

    char *lua_argv[] = {(char *)"lua", (char *)"-i", NULL};
    lua_main(2, lua_argv);

#ifdef USE_THREAD_MODULE
    _pthread_init();
#endif

    // led_blink();	// led flashing
    task_pump_messages();
}
