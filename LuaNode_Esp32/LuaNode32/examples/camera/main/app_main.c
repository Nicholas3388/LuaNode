// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char* TAG = "camera_demo";

void app_main()
{
    camera_config_t config = {.ledc_channel = LEDC_CHANNEL_0,
                              .ledc_timer = LEDC_TIMER_0,
                              .pin_d0 = 4,
                              .pin_d1 = 5,
                              .pin_d2 = 18,
                              .pin_d3 = 19,
                              .pin_d4 = 36,
                              .pin_d5 = 39,
                              .pin_d6 = 34,
                              .pin_d7 = 35,
                              .pin_rclk = 21,
                              .pin_vsync = 25,
                              .pin_sscb_sda = 26,
                              .pin_sscb_scl = 27,
                              .pin_rrst = 2,
                              .pin_wrst = 22,
                              .pin_wen = 23,
                              .pin_oe = 12};

    esp_err_t err = camera_init(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera init failed with error = %d", err);
        return;
    }

    while (true)
    {
        err = camera_run();
        if (err != ESP_OK)
        {
            ESP_LOGD(TAG, "Camera capture failed with error = %d", err);
        }
        else
        {
            ESP_LOGD(TAG, "Done");
            camera_print_fb();
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}
