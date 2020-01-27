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

#include "alexa_device.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lwip/api.h"
#include "lwip/igmp.h"
#include "lwip/sockets.h"
#include "upnp_broadcast_responder.h"
#include "user_config.h"
#include "utils.h"

static const char *TAG = "alexa";
static const char *relay_name = "receptacle";
static const char *backlight_name = "backlight";

void turn_on_relay(void) { ESP_LOGI(TAG, "turn on relay"); }

void turn_off_relay(void) { ESP_LOGI(TAG, "turn off relay"); }

void turn_on_backlight(void) { ESP_LOGI(TAG, "turn on backlight"); }

void turn_off_backlight(void) { ESP_LOGI(TAG, "turn off backlight"); }

void suspend_device(alexa_dev_t *dev) { vTaskSuspend(dev->task_handle); }

void resume_device(alexa_dev_t *dev) { vTaskResume(dev->task_handle); }

void alexa_init(void)
{
    alexa_device_list_init();
    upnp_broadcast_responder_init();

    alexa_dev_t *relay = create_new_device(relay_name, 80, turn_on_relay, turn_off_relay);
    if (relay == NULL)
    {
        ESP_LOGE(TAG, "create relay device failed!");
        return;
    }
    delay_ms(2000);

    alexa_dev_t *backlight = create_new_device(backlight_name, 81, turn_on_backlight, turn_off_backlight);
    if (backlight == NULL)
    {
        ESP_LOGE(TAG, "create backlight device failed!");
        return;
    }
}

void alexa_suspend(void) { alexa_device_list_traverse(suspend_device); }

void alexa_resume(void) { alexa_device_list_traverse(resume_device); }
