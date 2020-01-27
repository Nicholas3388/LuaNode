/*
 * Portions of this file come from OpenMV project (see sensor_* functions in the
 * end of file) Here is the copyright for these parts: This file is part of the
 * OpenMV project. Copyright (c) 2013/2014 Ibrahim Abdelkader
 * <i.abdalkader@gmail.com> This work is licensed under the MIT license, see the
 * file LICENSE for details.
 *
 *
 * Rest of the functions are licensed under Apache license as found below:
 */

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

#include "camera.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/periph_ctrl.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ov7725.h"
#include "rom/lldesc.h"
#include "sccb.h"
#include "sensor.h"
#include "soc/gpio_sig_map.h"
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
#include "soc/io_mux_reg.h"
#include "soc/soc.h"
#include "wiring.h"

#define ENABLE_TEST_PATTERN CONFIG_ENABLE_TEST_PATTERN

static const char* TAG = "camera";

static camera_config_t s_config;
static uint8_t* s_fb;
static sensor_t s_sensor;
static bool s_initialized = false;
static int s_fb_w;
static int s_fb_h;
static size_t s_fb_size;
static volatile bool s_i2s_running = 0;

const int resolution[][2] = {
    {40, 30},     /* 40x30 */
    {64, 32},     /* 64x32 */
    {64, 64},     /* 64x64 */
    {88, 72},     /* QQCIF */
    {160, 120},   /* QQVGA */
    {128, 160},   /* QQVGA2*/
    {176, 144},   /* QCIF  */
    {240, 160},   /* HQVGA */
    {320, 240},   /* QVGA  */
    {352, 288},   /* CIF   */
    {640, 480},   /* VGA   */
    {800, 600},   /* SVGA  */
    {1280, 1024}, /* SXGA  */
    {1600, 1200}, /* UXGA  */
};

static void output_pin_setting();
static void input_pin_setting();
static void get_frame(size_t line_width, int height);

esp_err_t camera_init(const camera_config_t* config)
{
    if (s_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(&s_config, config, sizeof(s_config));
    ESP_LOGD(TAG, "Initializing SSCB");
    SCCB_Init(s_config.pin_sscb_sda, s_config.pin_sscb_scl);
    ESP_LOGD(TAG, "lock FIFO");
    output_pin_setting();
    delay(5);
    gpio_set_level(s_config.pin_wen, 0);
    gpio_set_level(s_config.pin_oe, 1);
    delay(12);
    ESP_LOGD(TAG, "Searching for camera address");
    uint8_t addr = SCCB_Probe();
    if (addr == 0)
    {
        ESP_LOGE(TAG, "Camera address not found");
        return ESP_ERR_CAMERA_NOT_DETECTED;
    }
    ESP_LOGD(TAG, "Detected camera at address=0x%02x", addr);
    s_sensor.slv_addr = addr;

    ov7725_init(&s_sensor);
    ESP_LOGD(TAG, "Camera PID=0x%02x VER=0x%02x MIDL=0x%02x MIDH=0x%02x", s_sensor.id.pid, s_sensor.id.ver,
             s_sensor.id.midh, s_sensor.id.midl);

    ESP_LOGD(TAG, "Doing SW reset of sensor");
    // s_sensor.reset(&s_sensor);
    s_sensor.set_pixformat(&s_sensor, PIXFORMAT_RGB565);

#if ENABLE_TEST_PATTERN
    /* Test pattern may get handy
       if you are unable to get the live image right.
       Once test pattern is enable, sensor will output
       vertical shaded bars instead of live image.
    */
    s_sensor.set_colorbar(&s_sensor, 1);
    ESP_LOGD(TAG, "Test pattern enabled");
#endif

    framesize_t framesize = FRAMESIZE_QVGA;
    s_fb_w = resolution[framesize][0];
    s_fb_h = resolution[framesize][1];
    ESP_LOGD(TAG, "Setting frame size at %dx%d", s_fb_w, s_fb_h);
    if (s_sensor.set_framesize(&s_sensor, framesize) != 0)
    {
        ESP_LOGE(TAG, "Failed to set frame size");
        return ESP_ERR_CAMERA_FAILED_TO_SET_FRAME_SIZE;
    }

    s_fb_size = s_fb_w * s_fb_h * 2;
    ESP_LOGD(TAG, "Allocating frame buffer (%dx%d, %d bytes)", s_fb_w, s_fb_h, s_fb_size);
    s_fb = (uint8_t*)malloc(s_fb_size);
    if (s_fb == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate frame buffer");
        return ESP_ERR_NO_MEM;
    }

    input_pin_setting();

    // skip at least one frame after changing camera settings
    while (gpio_get_level(s_config.pin_vsync) == 0) { ; }
    while (gpio_get_level(s_config.pin_vsync) != 0) { ; }
    while (gpio_get_level(s_config.pin_vsync) == 0) { ; }

    ESP_LOGD(TAG, "Init done");
    s_initialized = true;
    return ESP_OK;
}

uint8_t* camera_get_fb()
{
    if (!s_initialized)
    {
        return NULL;
    }
    return s_fb;
}

int camera_get_fb_width()
{
    if (!s_initialized)
    {
        return 0;
    }
    return s_fb_w;
}

int camera_get_fb_height()
{
    if (!s_initialized)
    {
        return 0;
    }
    return s_fb_h;
}

esp_err_t camera_run()
{
    if (!s_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }
    get_frame(s_fb_w, s_fb_h);
    return ESP_OK;
}

void camera_print_fb()
{
    /* Number of pixels to skip
       in order to fit into terminal screen.
       Assumed picture to be 80 columns wide.
       Skip twice as more rows as they look higher.
     */
    int pixels_to_skip = s_fb_w / 80;

    for (int ih = 0; ih < s_fb_h; ih += pixels_to_skip * 2)
    {
        for (int iw = 0; iw < s_fb_w * 2; iw += pixels_to_skip)
        {
            uint8_t px = (s_fb[iw + (ih * s_fb_w * 2)]);
            if (px < 26)
                printf(" ");
            else if (px < 51)
                printf(".");
            else if (px < 77)
                printf(":");
            else if (px < 102)
                printf("-");
            else if (px < 128)
                printf("=");
            else if (px < 154)
                printf("+");
            else if (px < 179)
                printf("*");
            else if (px < 205)
                printf("#");
            else if (px < 230)
                printf("%%");
            else
                printf("@");
        }
        printf("\n");
    }
}

static void output_pin_setting()
{
    gpio_num_t pins[] = {s_config.pin_wen, s_config.pin_rrst, s_config.pin_wrst, s_config.pin_rclk, s_config.pin_oe};
    gpio_config_t conf = {.mode = GPIO_MODE_OUTPUT,
                          .pull_up_en = GPIO_PULLUP_DISABLE,
                          .pull_down_en = GPIO_PULLDOWN_DISABLE,
                          .intr_type = GPIO_INTR_DISABLE};
    for (int i = 0; i < sizeof(pins) / sizeof(gpio_num_t); ++i)
    {
        conf.pin_bit_mask = 1LL << pins[i];
        gpio_config(&conf);
    }
}

static void input_pin_setting()
{
    // Configure input GPIOs
    gpio_num_t pins[] = {s_config.pin_d7, s_config.pin_d6, s_config.pin_d5, s_config.pin_d4,   s_config.pin_d3,
                         s_config.pin_d2, s_config.pin_d1, s_config.pin_d0, s_config.pin_vsync};
    gpio_config_t conf = {.mode = GPIO_MODE_INPUT,
                          .pull_up_en = GPIO_PULLUP_DISABLE,
                          .pull_down_en = GPIO_PULLDOWN_DISABLE,
                          .intr_type = GPIO_INTR_DISABLE};
    for (int i = 0; i < sizeof(pins) / sizeof(gpio_num_t); ++i)
    {
        conf.pin_bit_mask = 1LL << pins[i];
        gpio_config(&conf);
    }
}

static void get_frame(size_t line_width, int height)
{
    while (gpio_get_level(s_config.pin_vsync) == 0) {}
    while (gpio_get_level(s_config.pin_vsync) != 0) {}
    ESP_LOGD(TAG, "Got VSYNC, start writing new frame");
    gpio_set_level(s_config.pin_wrst, 0);  // reset write point
    gpio_set_level(s_config.pin_wen, 1);
    gpio_set_level(s_config.pin_wen, 1);
    gpio_set_level(s_config.pin_wrst, 1);

    while (gpio_get_level(s_config.pin_vsync) == 0) {}
    while (gpio_get_level(s_config.pin_vsync) != 0) {}
    ESP_LOGD(TAG, "Got VSYNC, start saving a frame");
    gpio_set_level(s_config.pin_wen, 0);   // lock FIFO
    gpio_set_level(s_config.pin_rrst, 0);  // reset read point
    gpio_set_level(s_config.pin_rclk, 0);
    gpio_set_level(s_config.pin_rclk, 1);
    gpio_set_level(s_config.pin_rrst, 1);
    gpio_set_level(s_config.pin_wen, 0);  // lock FIFO
    gpio_set_level(s_config.pin_oe, 0);

    int i, j;
    unsigned char dat = 0x0;
    int len = line_width * height * 2;
    for (i = 0; i < len; i++)
    {
        gpio_set_level(s_config.pin_rclk, 0);

        dat |= (gpio_get_level(s_config.pin_d7) << 7);
        dat |= (gpio_get_level(s_config.pin_d6) << 6);
        dat |= (gpio_get_level(s_config.pin_d5) << 5);
        dat |= (gpio_get_level(s_config.pin_d4) << 4);
        dat |= (gpio_get_level(s_config.pin_d3) << 3);
        dat |= (gpio_get_level(s_config.pin_d2) << 2);
        dat |= (gpio_get_level(s_config.pin_d1) << 1);
        dat |= gpio_get_level(s_config.pin_d0);
        s_fb[i] = dat;
        dat = 0x0;

        gpio_set_level(s_config.pin_rclk, 1);
    }
    gpio_set_level(s_config.pin_oe, 1);
    ESP_LOGD(TAG, "Frame done");
}
