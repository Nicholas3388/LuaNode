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

#include "utils.h"

#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "utils";

void printHex(char c)
{
    char buff[3] = {0x0};
    sprintf(buff, "%02X ", c);
    ESP_LOGI(TAG, "%s", buff);
}

void delay_ms(int ms) { vTaskDelay(ms / portTICK_RATE_MS); }

void ip_addr_to_num(unsigned char *res, ip_addr_t *addr)
{
    unsigned int ip = (addr->u_addr).ip4.addr;
    res[0] = (unsigned char)(ip & 0xFF);
    res[1] = (unsigned char)((ip >> 8) & 0xFF);
    res[2] = (unsigned char)((ip >> 16) & 0xFF);
    res[3] = (unsigned char)((ip >> 24) & 0xFF);
}