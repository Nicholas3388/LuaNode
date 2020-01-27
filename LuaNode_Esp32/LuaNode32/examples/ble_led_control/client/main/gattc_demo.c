// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
// Additions Copyright (C) Copyright 2016 pcbreflux, Apache 2.0 License.
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

/****************************************************************************
 *
 * This file is for gatt client. It can scan ble device, connect one device,
 *
 ****************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bt.h"
#include "bt_trace.h"
#include "bt_types.h"
#include "bta_api.h"
#include "bta_gatt_api.h"
#include "btm_api.h"
#include "controller.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BT_BD_ADDR_STR "%02x:%02x:%02x:%02x:%02x:%02x"
#define BT_BD_ADDR_HEX(addr) addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]

#define TEST_SRV_UUID 0x00FF
#define TEST_CHAR_UUID 0xFF01

static esp_gatt_if_t client_if;
static uint16_t client_conn = 0;
esp_gatt_status_t status = ESP_GATT_ERROR;
bool connet = false;
uint16_t simpleClient_id = 0xEE;

const char device_name[] = "GATTS_LED_DEMO";
static char tar_dev_mac[6] = {0xff, 0xb5, 0x30, 0x4e, 0x0a, 0xcb};
static const char tar_char[] = {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                                0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e};
static const char write_char[] = {0x6e, 0x40, 0x00, 0x03, 0xb5, 0xa3, 0xf3, 0x93,
                                  0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e};

static esp_bd_addr_t server_dba;

static esp_ble_scan_params_t ble_scan_params = {.scan_type = BLE_SCAN_TYPE_ACTIVE,
                                                .own_addr_type = ESP_PUBLIC_ADDR,
                                                .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
                                                .scan_interval = 0x50,
                                                .scan_window = 0x30};

static esp_gatt_srvc_id_t test_service_id = {
    .id =
        {
            .uuid =
                {
                    .len = ESP_UUID_LEN_16,
                    .uuid =
                        {
                            .uuid16 = TEST_SRV_UUID,
                        },
                },
            .inst_id = 0,
        },
    .is_primary = true,
};

static esp_gatt_id_t test_char_id = {
    .uuid =
        {
            .len = ESP_UUID_LEN_16,
            .uuid =
                {
                    .uuid16 = TEST_CHAR_UUID,
                },
        },
    .inst_id = 0,
};

static esp_gatt_id_t notify_descr_id = {
    .uuid =
        {
            .len = ESP_UUID_LEN_16,
            .uuid =
                {
                    .uuid16 = GATT_UUID_CHAR_CLIENT_CONFIG,
                },
        },
    .inst_id = 0,
};

static esp_gatt_srvc_id_t nrf51_service_id = {
    .id =
        {
            .uuid =
                {
                    .len = ESP_UUID_LEN_128,
                    .uuid =
                        {
                            .uuid128 = {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01,
                                        0x00, 0x40, 0x6e},
                        },
                },
            .inst_id = 0,
        },
    .is_primary = true,
};

static esp_gatt_id_t nrf51_char_id = {
    .uuid =
        {
            .len = ESP_UUID_LEN_128,
            .uuid =
                {
                    .uuid128 = {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00,
                                0x40, 0x6e},
                },
        },
    .inst_id = 0,
};

static bool task_run = false;
static TaskHandle_t ledTaskHandle = NULL;
static uint8_t toggle = 0;

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static void get_characteristic_property(uint32_t flag)
{
    int i;
    ESP_LOGI(TAG, "=======================================");
    for (i = 0; i < 8; i++)
    {
        switch (flag & (2 << i))
        {
            case ESP_GATT_CHAR_PROP_BIT_BROADCAST:
                ESP_LOGI(TAG, "==> CHAR PROP ESP_GATT_CHAR_PROP_BIT_BROADCAST");
                break;
            case ESP_GATT_CHAR_PROP_BIT_READ:
                ESP_LOGI(TAG, "==> CHAR PROP ESP_GATT_CHAR_PROP_BIT_READ");
                break;
            case ESP_GATT_CHAR_PROP_BIT_WRITE_NR:
                ESP_LOGI(TAG, "==> CHAR PROP ESP_GATT_CHAR_PROP_BIT_WRITE_NR");
                break;
            case ESP_GATT_CHAR_PROP_BIT_WRITE:
                ESP_LOGI(TAG, "==> CHAR PROP ESP_GATT_CHAR_PROP_BIT_WRITE");
                break;
            case ESP_GATT_CHAR_PROP_BIT_NOTIFY:
                ESP_LOGI(TAG, "==> CHAR PROP ESP_GATT_CHAR_PROP_BIT_NOTIFY");
                break;
            case ESP_GATT_CHAR_PROP_BIT_INDICATE:
                ESP_LOGI(TAG, "==> CHAR PROP ESP_GATT_CHAR_PROP_BIT_INDICATE");
                break;
            case ESP_GATT_CHAR_PROP_BIT_AUTH:
                ESP_LOGI(TAG, "==> CHAR PROP ESP_GATT_CHAR_PROP_BIT_AUTH");
                break;
            case ESP_GATT_CHAR_PROP_BIT_EXT_PROP:
                ESP_LOGI(TAG, "==> CHAR PROP ESP_GATT_CHAR_PROP_BIT_EXT_PROP");
                break;
            default:
                break;
        }
    }
    ESP_LOGI(TAG, "=======================================");
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    switch (event)
    {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        {
            // the unit of the duration is second
            uint32_t duration = 10;
            esp_ble_gap_start_scanning(duration);
            break;
        }
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
        {
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
            switch (scan_result->scan_rst.search_evt)
            {
                case ESP_GAP_SEARCH_INQ_RES_EVT:
                    switch (scan_result->scan_rst.dev_type)
                    {
                        case ESP_BT_DEVICE_TYPE_BREDR:
                            ESP_LOGI(TAG, "==> Connected Device type BREDR");
                            break;
                        case ESP_BT_DEVICE_TYPE_BLE:
                            ESP_LOGI(TAG, "==> Connected Device type BLE");
                            break;
                        case ESP_BT_DEVICE_TYPE_DUMO:
                            ESP_LOGI(TAG, "==> Connected Device type DUMO");
                            break;
                        default:
                            break;
                    }
                    LOG_INFO("BDA %x,%x,%x,%x,%x,%x:", scan_result->scan_rst.bda[0], scan_result->scan_rst.bda[1],
                             scan_result->scan_rst.bda[2], scan_result->scan_rst.bda[3], scan_result->scan_rst.bda[4],
                             scan_result->scan_rst.bda[5]);
                    for (int i = 0; i < 6; i++) { server_dba[i] = scan_result->scan_rst.bda[i]; }
                    adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL,
                                                        &adv_name_len);
                    LOG_INFO("adv_name_len=%x\n", adv_name_len);
                    /*for (int j = 0; j < adv_name_len; j++) {
                        LOG_INFO("a%d %x %c = d%d %x %c",j, adv_name[j],
                    adv_name[j],j, device_name[j], device_name[j]);
                    }*/

                    if (strncmp((char *)adv_name, device_name, adv_name_len) == 0 && connet == false)
                    {
                        connet = true;
                        ESP_LOGI(TAG, "==> address type: %d, dev name: %s", scan_result->scan_rst.ble_addr_type,
                                 adv_name);
                        LOG_INFO("Connect to the remote device.");
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(client_if, scan_result->scan_rst.bda, true);
                        memcpy(tar_dev_mac, scan_result->scan_rst.bda, 6);
                    }
                    break;
                case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                    // esp_ble_gap_cb_param_t *scan_result =
                    // (esp_ble_gap_cb_param_t *)param;
                    switch (scan_result->scan_rst.ble_evt_type)
                    {
                        case ESP_BLE_EVT_CONN_ADV:
                            ESP_LOGI(TAG, "==> CONN_ADV");
                            ESP_LOGI(TAG, "BDA %x,%x,%x,%x,%x,%x:", scan_result->scan_rst.bda[0],
                                     scan_result->scan_rst.bda[1], scan_result->scan_rst.bda[2],
                                     scan_result->scan_rst.bda[3], scan_result->scan_rst.bda[4],
                                     scan_result->scan_rst.bda[5]);
                            ESP_LOGI(TAG, "==> RSSI: %d", scan_result->scan_rst.rssi);
                            ESP_LOGI(TAG, "==> address type: %d", scan_result->scan_rst.ble_addr_type);
                            break;
                        case ESP_BLE_EVT_CONN_DIR_ADV:
                            ESP_LOGI(TAG, "==> CONN_DIR_ADV");
                            break;
                        case ESP_BLE_EVT_DISC_ADV:
                            ESP_LOGI(TAG, "==> DISC_ADV");
                            break;
                        case ESP_BLE_EVT_NON_CONN_ADV:
                            ESP_LOGI(TAG, "==> NON_CONN_ADV");
                            break;
                        case ESP_BLE_EVT_SCAN_RSP:
                            ESP_LOGI(TAG, "==> receive scan response");
                            LOG_INFO("BDA %x,%x,%x,%x,%x,%x:", scan_result->scan_rst.bda[0],
                                     scan_result->scan_rst.bda[1], scan_result->scan_rst.bda[2],
                                     scan_result->scan_rst.bda[3], scan_result->scan_rst.bda[4],
                                     scan_result->scan_rst.bda[5]);
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    uint16_t conn_id = 0;
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    LOG_INFO("esp_gattc_cb, event = %x", event);
    switch (event)
    {
        case ESP_GATTC_REG_EVT:
            status = p_data->reg.status;
            LOG_INFO("ESP_GATTC_REG_EVT status = %x, client_if = %x", status, gattc_if);
            if (status == ESP_GATT_OK)
            {
                client_if = gattc_if;
            }
            else
            {
                LOG_INFO("ESP_GATTC_REG failed!");
            }
            break;
        case ESP_GATTC_OPEN_EVT:
            conn_id = p_data->open.conn_id;
            LOG_INFO("ESP_GATTC_OPEN_EVT conn_id %d, if %d, status %d", conn_id, gattc_if, p_data->open.status);
            if (p_data->open.status != 0)
            {
                LOG_INFO("==> open GATTC error, closed");
                esp_ble_gattc_close(gattc_if, conn_id);
                esp_ble_gattc_open(gattc_if, (uint8_t *)tar_dev_mac, true);
                break;
            }
            esp_ble_gattc_search_service(gattc_if, conn_id, NULL);
            client_conn = conn_id;
            break;
        case ESP_GATTC_CLOSE_EVT:
            LOG_INFO("==> GATTC closed");
            task_run = false;
            client_conn = 0;
            break;
        case ESP_GATTC_READ_CHAR_EVT:
        {
            // esp_gatt_srvc_id_t *srvc_id = &p_data->read.srvc_id;
            esp_gatt_id_t *char_id = &p_data->read.char_id;
            conn_id = p_data->open.conn_id;
            LOG_INFO(
                "READ CHAR: open.conn_id = %x search_res.conn_id = %x  "
                "read.conn_id = %x",
                conn_id, p_data->search_res.conn_id, p_data->read.conn_id);
            LOG_INFO("READ CHAR: read.status = %x inst_id = %x", p_data->read.status, char_id->inst_id);
            if (p_data->read.status == 0)
            {
                if (char_id->uuid.len == ESP_UUID_LEN_16)
                {
                    LOG_INFO("Char UUID16: %x", char_id->uuid.uuid.uuid16);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_32)
                {
                    LOG_INFO("Char UUID32: %x", char_id->uuid.uuid.uuid32);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_128)
                {
                    LOG_INFO(
                        "Char UUID128: "
                        "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
                        char_id->uuid.uuid.uuid128[0], char_id->uuid.uuid.uuid128[1], char_id->uuid.uuid.uuid128[2],
                        char_id->uuid.uuid.uuid128[3], char_id->uuid.uuid.uuid128[4], char_id->uuid.uuid.uuid128[5],
                        char_id->uuid.uuid.uuid128[6], char_id->uuid.uuid.uuid128[7], char_id->uuid.uuid.uuid128[8],
                        char_id->uuid.uuid.uuid128[9], char_id->uuid.uuid.uuid128[10], char_id->uuid.uuid.uuid128[11],
                        char_id->uuid.uuid.uuid128[12], char_id->uuid.uuid.uuid128[13], char_id->uuid.uuid.uuid128[14],
                        char_id->uuid.uuid.uuid128[15]);
                }
                else
                {
                    LOG_ERROR("Char UNKNOWN LEN %d\n", char_id->uuid.len);
                }
                for (int i = 0; i < p_data->read.value_len; i++) { LOG_INFO("%x:", p_data->read.value[i]); }
            }
            break;
        }
        case ESP_GATTC_WRITE_CHAR_EVT:
        {
            // esp_gatt_srvc_id_t *srvc_id = &p_data->write.srvc_id;
            esp_gatt_id_t *char_id = &p_data->write.char_id;
            esp_gatt_id_t *descr_id = &p_data->write.descr_id;
            conn_id = p_data->open.conn_id;
            LOG_INFO(
                "WRITE CHAR: open.conn_id = %x search_res.conn_id = %x  "
                "write.conn_id = %x",
                conn_id, p_data->search_res.conn_id, p_data->write.conn_id);
            LOG_INFO("WRITE CHAR: write.status = %x inst_id = %x", p_data->write.status, char_id->inst_id);
            if (p_data->write.status == 0)
            {
                if (char_id->uuid.len == ESP_UUID_LEN_16)
                {
                    LOG_INFO("Char UUID16: %x", char_id->uuid.uuid.uuid16);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_32)
                {
                    LOG_INFO("Char UUID32: %x", char_id->uuid.uuid.uuid32);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_128)
                {
                    LOG_INFO(
                        "Char UUID128: "
                        "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
                        char_id->uuid.uuid.uuid128[0], char_id->uuid.uuid.uuid128[1], char_id->uuid.uuid.uuid128[2],
                        char_id->uuid.uuid.uuid128[3], char_id->uuid.uuid.uuid128[4], char_id->uuid.uuid.uuid128[5],
                        char_id->uuid.uuid.uuid128[6], char_id->uuid.uuid.uuid128[7], char_id->uuid.uuid.uuid128[8],
                        char_id->uuid.uuid.uuid128[9], char_id->uuid.uuid.uuid128[10], char_id->uuid.uuid.uuid128[11],
                        char_id->uuid.uuid.uuid128[12], char_id->uuid.uuid.uuid128[13], char_id->uuid.uuid.uuid128[14],
                        char_id->uuid.uuid.uuid128[15]);
                }
                else
                {
                    LOG_ERROR("Char UNKNOWN LEN %d", char_id->uuid.len);
                }
                if (descr_id->uuid.len == ESP_UUID_LEN_16)
                {
                    LOG_INFO("Decr UUID16: %x", descr_id->uuid.uuid.uuid16);
                }
                else if (descr_id->uuid.len == ESP_UUID_LEN_32)
                {
                    LOG_INFO("Decr UUID32: %x", descr_id->uuid.uuid.uuid32);
                }
                else if (descr_id->uuid.len == ESP_UUID_LEN_128)
                {
                    LOG_INFO(
                        "Decr UUID128: "
                        "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
                        descr_id->uuid.uuid.uuid128[0], descr_id->uuid.uuid.uuid128[1], descr_id->uuid.uuid.uuid128[2],
                        descr_id->uuid.uuid.uuid128[3], descr_id->uuid.uuid.uuid128[4], descr_id->uuid.uuid.uuid128[5],
                        descr_id->uuid.uuid.uuid128[6], descr_id->uuid.uuid.uuid128[7], descr_id->uuid.uuid.uuid128[8],
                        descr_id->uuid.uuid.uuid128[9], descr_id->uuid.uuid.uuid128[10],
                        descr_id->uuid.uuid.uuid128[11], descr_id->uuid.uuid.uuid128[12],
                        descr_id->uuid.uuid.uuid128[13], descr_id->uuid.uuid.uuid128[14],
                        descr_id->uuid.uuid.uuid128[15]);
                }
                else
                {
                    LOG_ERROR("Decr UNKNOWN LEN %d", descr_id->uuid.len);
                }
            }
            break;
        }
        case ESP_GATTC_SEARCH_RES_EVT:
        {
            esp_gatt_srvc_id_t *srvc_id = &p_data->search_res.srvc_id;
            conn_id = p_data->open.conn_id;
            LOG_INFO("SEARCH RES: open.conn_id = %x search_res.conn_id = %x", conn_id, p_data->search_res.conn_id);
            if (srvc_id->id.uuid.len == ESP_UUID_LEN_16)
            {
                LOG_INFO("UUID16: %x", srvc_id->id.uuid.uuid.uuid16);
            }
            else if (srvc_id->id.uuid.len == ESP_UUID_LEN_32)
            {
                LOG_INFO("UUID32: %x", srvc_id->id.uuid.uuid.uuid32);
            }
            else if (srvc_id->id.uuid.len == ESP_UUID_LEN_128)
            {
                LOG_INFO("UUID128: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x", srvc_id->id.uuid.uuid.uuid128[0],
                         srvc_id->id.uuid.uuid.uuid128[1], srvc_id->id.uuid.uuid.uuid128[2],
                         srvc_id->id.uuid.uuid.uuid128[3], srvc_id->id.uuid.uuid.uuid128[4],
                         srvc_id->id.uuid.uuid.uuid128[5], srvc_id->id.uuid.uuid.uuid128[6],
                         srvc_id->id.uuid.uuid.uuid128[7], srvc_id->id.uuid.uuid.uuid128[8],
                         srvc_id->id.uuid.uuid.uuid128[9], srvc_id->id.uuid.uuid.uuid128[10],
                         srvc_id->id.uuid.uuid.uuid128[11], srvc_id->id.uuid.uuid.uuid128[12],
                         srvc_id->id.uuid.uuid.uuid128[13], srvc_id->id.uuid.uuid.uuid128[14],
                         srvc_id->id.uuid.uuid.uuid128[15]);
            }
            else
            {
                LOG_ERROR("UNKNOWN LEN %d", srvc_id->id.uuid.len);
            }
            esp_ble_gattc_get_characteristic(gattc_if, p_data->search_res.conn_id, srvc_id, NULL);
            break;
        }
        case ESP_GATTC_WRITE_DESCR_EVT:
        {
            esp_gatt_srvc_id_t *srvc_id = &p_data->write.srvc_id;
            esp_gatt_id_t *char_id = &p_data->write.char_id;
            esp_gatt_id_t *descr_id = &p_data->write.descr_id;
            conn_id = p_data->open.conn_id;
            LOG_INFO(
                "WRITE DESCR: open.conn_id = %x search_res.conn_id = %x  "
                "write.conn_id = %x",
                conn_id, p_data->search_res.conn_id, p_data->write.conn_id);
            LOG_INFO("WRITE DESCR: write.status = %x inst_id = %x open.gatt_if = %x", p_data->write.status,
                     char_id->inst_id, gattc_if);
            if (p_data->write.status == 0)
            {
                if (char_id->uuid.len == ESP_UUID_LEN_16)
                {
                    LOG_INFO("Char UUID16: %x", char_id->uuid.uuid.uuid16);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_32)
                {
                    LOG_INFO("Char UUID32: %x", char_id->uuid.uuid.uuid32);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_128)
                {
                    LOG_INFO(
                        "Char UUID128: "
                        "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
                        char_id->uuid.uuid.uuid128[0], char_id->uuid.uuid.uuid128[1], char_id->uuid.uuid.uuid128[2],
                        char_id->uuid.uuid.uuid128[3], char_id->uuid.uuid.uuid128[4], char_id->uuid.uuid.uuid128[5],
                        char_id->uuid.uuid.uuid128[6], char_id->uuid.uuid.uuid128[7], char_id->uuid.uuid.uuid128[8],
                        char_id->uuid.uuid.uuid128[9], char_id->uuid.uuid.uuid128[10], char_id->uuid.uuid.uuid128[11],
                        char_id->uuid.uuid.uuid128[12], char_id->uuid.uuid.uuid128[13], char_id->uuid.uuid.uuid128[14],
                        char_id->uuid.uuid.uuid128[15]);
                }
                else
                {
                    LOG_ERROR("Char UNKNOWN LEN %d", char_id->uuid.len);
                }
                if (descr_id->uuid.len == ESP_UUID_LEN_16)
                {
                    LOG_INFO("Decr UUID16: %x", descr_id->uuid.uuid.uuid16);
                }
                else if (descr_id->uuid.len == ESP_UUID_LEN_32)
                {
                    LOG_INFO("Decr UUID32: %x", descr_id->uuid.uuid.uuid32);
                }
                else if (descr_id->uuid.len == ESP_UUID_LEN_128)
                {
                    LOG_INFO(
                        "Decr UUID128: "
                        "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
                        descr_id->uuid.uuid.uuid128[0], descr_id->uuid.uuid.uuid128[1], descr_id->uuid.uuid.uuid128[2],
                        descr_id->uuid.uuid.uuid128[3], descr_id->uuid.uuid.uuid128[4], descr_id->uuid.uuid.uuid128[5],
                        descr_id->uuid.uuid.uuid128[6], descr_id->uuid.uuid.uuid128[7], descr_id->uuid.uuid.uuid128[8],
                        descr_id->uuid.uuid.uuid128[9], descr_id->uuid.uuid.uuid128[10],
                        descr_id->uuid.uuid.uuid128[11], descr_id->uuid.uuid.uuid128[12],
                        descr_id->uuid.uuid.uuid128[13], descr_id->uuid.uuid.uuid128[14],
                        descr_id->uuid.uuid.uuid128[15]);
                }
                else
                {
                    LOG_ERROR("Decr UNKNOWN LEN %d", descr_id->uuid.len);
                }
                if (srvc_id->id.uuid.len == ESP_UUID_LEN_16)
                {
                    LOG_INFO("SRVC UUID16: %x", srvc_id->id.uuid.uuid.uuid16);
                }
                else if (srvc_id->id.uuid.len == ESP_UUID_LEN_32)
                {
                    LOG_INFO("SRVC UUID32: %x", srvc_id->id.uuid.uuid.uuid32);
                }
                else if (srvc_id->id.uuid.len == ESP_UUID_LEN_128)
                {
                    LOG_INFO(
                        "SRVC UUID128: "
                        "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
                        srvc_id->id.uuid.uuid.uuid128[0], srvc_id->id.uuid.uuid.uuid128[1],
                        srvc_id->id.uuid.uuid.uuid128[2], srvc_id->id.uuid.uuid.uuid128[3],
                        srvc_id->id.uuid.uuid.uuid128[4], srvc_id->id.uuid.uuid.uuid128[5],
                        srvc_id->id.uuid.uuid.uuid128[6], srvc_id->id.uuid.uuid.uuid128[7],
                        srvc_id->id.uuid.uuid.uuid128[8], srvc_id->id.uuid.uuid.uuid128[9],
                        srvc_id->id.uuid.uuid.uuid128[10], srvc_id->id.uuid.uuid.uuid128[11],
                        srvc_id->id.uuid.uuid.uuid128[12], srvc_id->id.uuid.uuid.uuid128[13],
                        srvc_id->id.uuid.uuid.uuid128[14], srvc_id->id.uuid.uuid.uuid128[15]);
                }
                else
                {
                    LOG_ERROR("SRVC UNKNOWN LEN %d", srvc_id->id.uuid.len);
                }
                LOG_INFO("WRITE DESCR: gattc_if = %x", gattc_if);
                LOG_INFO("remote_bda %x,%x,%x,%x,%x,%x:", p_data->open.remote_bda[0], p_data->open.remote_bda[1],
                         p_data->open.remote_bda[2], p_data->open.remote_bda[3], p_data->open.remote_bda[4],
                         p_data->open.remote_bda[5]);
                LOG_INFO("server_dba %x,%x,%x,%x,%x,%x:", server_dba[0], server_dba[1], server_dba[2], server_dba[3],
                         server_dba[4], server_dba[5]);
            }
            break;
        }
        case ESP_GATTC_NOTIFY_EVT:
        {
            // esp_gatt_srvc_id_t *srvc_id = &p_data->read.srvc_id;
            esp_gatt_id_t *char_id = &p_data->notify.char_id;
            conn_id = p_data->open.conn_id;
            LOG_INFO(
                "NOTIFY: open.conn_id = %x search_res.conn_id = %x  "
                "notify.conn_id = %x",
                conn_id, p_data->search_res.conn_id, p_data->notify.conn_id);
            LOG_INFO("NOTIFY: notify.is_notify = %x inst_id = %x", p_data->notify.is_notify, char_id->inst_id);
            if (p_data->notify.is_notify == 1)
            {
                if (char_id->uuid.len == ESP_UUID_LEN_16)
                {
                    LOG_INFO("Char UUID16: %x", char_id->uuid.uuid.uuid16);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_32)
                {
                    LOG_INFO("Char UUID32: %x", char_id->uuid.uuid.uuid32);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_128)
                {
                    LOG_INFO(
                        "Char UUID128: "
                        "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
                        char_id->uuid.uuid.uuid128[0], char_id->uuid.uuid.uuid128[1], char_id->uuid.uuid.uuid128[2],
                        char_id->uuid.uuid.uuid128[3], char_id->uuid.uuid.uuid128[4], char_id->uuid.uuid.uuid128[5],
                        char_id->uuid.uuid.uuid128[6], char_id->uuid.uuid.uuid128[7], char_id->uuid.uuid.uuid128[8],
                        char_id->uuid.uuid.uuid128[9], char_id->uuid.uuid.uuid128[10], char_id->uuid.uuid.uuid128[11],
                        char_id->uuid.uuid.uuid128[12], char_id->uuid.uuid.uuid128[13], char_id->uuid.uuid.uuid128[14],
                        char_id->uuid.uuid.uuid128[15]);
                }
                else
                {
                    LOG_ERROR("Char UNKNOWN LEN %d\n", char_id->uuid.len);
                }
                for (int i = 0; i < p_data->notify.value_len; i++)
                { LOG_INFO("NOTIFY: V%d %x:", i, p_data->notify.value[i]); }
            }
            break;
        }
        case ESP_GATTC_GET_CHAR_EVT:
        {
            esp_gatt_srvc_id_t *srvc_id = &p_data->get_char.srvc_id;
            esp_gatt_id_t *char_id = &p_data->get_char.char_id;
            conn_id = p_data->open.conn_id;
            LOG_INFO(
                "GET CHAR: open.conn_id = %x search_res.conn_id = %x  "
                "get_char.conn_id = %x",
                conn_id, p_data->search_res.conn_id, p_data->get_char.conn_id);
            LOG_INFO(
                "GET CHAR: get_char.char_prop = %x get_char.status = %x "
                "inst_id = %x open.gatt_if = %x",
                p_data->get_char.char_prop, p_data->get_char.status, char_id->inst_id, gattc_if);
            LOG_INFO("remote_bda %x,%x,%x,%x,%x,%x:", p_data->open.remote_bda[0], p_data->open.remote_bda[1],
                     p_data->open.remote_bda[2], p_data->open.remote_bda[3], p_data->open.remote_bda[4],
                     p_data->open.remote_bda[5]);
            if (p_data->get_char.status == 0)
            {
                // print characteristic property
                get_characteristic_property(p_data->get_char.char_prop);

                if (char_id->uuid.len == ESP_UUID_LEN_16)
                {
                    LOG_INFO("UUID16: %x", char_id->uuid.uuid.uuid16);
                    if (char_id->uuid.uuid.uuid16 == TEST_CHAR_UUID)
                    {
                        ESP_LOGI(TAG, "register notify\n");
                        esp_ble_gattc_register_for_notify(gattc_if, p_data->open.remote_bda, &test_service_id,
                                                          &p_data->get_char.char_id);
                    }
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_32)
                {
                    LOG_INFO("UUID32: %x", char_id->uuid.uuid.uuid32);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_128)
                {
                    LOG_INFO(
                        "UUID128: "
                        "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
                        char_id->uuid.uuid.uuid128[0], char_id->uuid.uuid.uuid128[1], char_id->uuid.uuid.uuid128[2],
                        char_id->uuid.uuid.uuid128[3], char_id->uuid.uuid.uuid128[4], char_id->uuid.uuid.uuid128[5],
                        char_id->uuid.uuid.uuid128[6], char_id->uuid.uuid.uuid128[7], char_id->uuid.uuid.uuid128[8],
                        char_id->uuid.uuid.uuid128[9], char_id->uuid.uuid.uuid128[10], char_id->uuid.uuid.uuid128[11],
                        char_id->uuid.uuid.uuid128[12], char_id->uuid.uuid.uuid128[13], char_id->uuid.uuid.uuid128[14],
                        char_id->uuid.uuid.uuid128[15]);

                    esp_ble_gattc_get_descriptor(gattc_if, conn_id, srvc_id, char_id, NULL);
                    esp_ble_gattc_get_characteristic(gattc_if, conn_id, srvc_id, char_id);
                }
                else
                {
                    LOG_ERROR("UNKNOWN LEN %d", char_id->uuid.len);
                }
            }
            else
            {
                ESP_LOGE(TAG, "get characteristic failed");
            }
            break;
        }
        case ESP_GATTC_GET_DESCR_EVT:
        {
            // esp_gatt_srvc_id_t *srvc_id = &p_data->get_descr.srvc_id;
            esp_gatt_id_t *char_id = &p_data->get_descr.char_id;
            esp_gatt_id_t *descr_id = &p_data->get_descr.descr_id;
            conn_id = p_data->open.conn_id;
            LOG_INFO(
                "GET DESCR: open.conn_id = %x search_res.conn_id = %x  "
                "get_descr.conn_id = %x",
                conn_id, p_data->search_res.conn_id, p_data->get_descr.conn_id);
            LOG_INFO(
                "GET DESCR: get_descr.status = %x inst_id = %x open.gatt_if = "
                "%x",
                p_data->get_descr.status, char_id->inst_id, gattc_if);
            // uint8_t value[2];
            // value[0]=0x01;
            // value[1]=0x00;
            if (p_data->get_descr.status == 0)
            {
                if (char_id->uuid.len == ESP_UUID_LEN_16)
                {
                    LOG_INFO("Char UUID16: %x", char_id->uuid.uuid.uuid16);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_32)
                {
                    LOG_INFO("Char UUID32: %x", char_id->uuid.uuid.uuid32);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_128)
                {
                    LOG_INFO(
                        "Char UUID128: "
                        "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
                        char_id->uuid.uuid.uuid128[0], char_id->uuid.uuid.uuid128[1], char_id->uuid.uuid.uuid128[2],
                        char_id->uuid.uuid.uuid128[3], char_id->uuid.uuid.uuid128[4], char_id->uuid.uuid.uuid128[5],
                        char_id->uuid.uuid.uuid128[6], char_id->uuid.uuid.uuid128[7], char_id->uuid.uuid.uuid128[8],
                        char_id->uuid.uuid.uuid128[9], char_id->uuid.uuid.uuid128[10], char_id->uuid.uuid.uuid128[11],
                        char_id->uuid.uuid.uuid128[12], char_id->uuid.uuid.uuid128[13], char_id->uuid.uuid.uuid128[14],
                        char_id->uuid.uuid.uuid128[15]);
                }
                else
                {
                    LOG_ERROR("Char UNKNOWN LEN %d", char_id->uuid.len);
                }
                if (descr_id->uuid.len == ESP_UUID_LEN_16)
                {
                    LOG_INFO("Decr UUID16: %x", descr_id->uuid.uuid.uuid16);
                }
                else if (descr_id->uuid.len == ESP_UUID_LEN_32)
                {
                    LOG_INFO("Decr UUID32: %x", descr_id->uuid.uuid.uuid32);
                }
                else if (descr_id->uuid.len == ESP_UUID_LEN_128)
                {
                    LOG_INFO(
                        "Decr UUID128: "
                        "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
                        descr_id->uuid.uuid.uuid128[0], descr_id->uuid.uuid.uuid128[1], descr_id->uuid.uuid.uuid128[2],
                        descr_id->uuid.uuid.uuid128[3], descr_id->uuid.uuid.uuid128[4], descr_id->uuid.uuid.uuid128[5],
                        descr_id->uuid.uuid.uuid128[6], descr_id->uuid.uuid.uuid128[7], descr_id->uuid.uuid.uuid128[8],
                        descr_id->uuid.uuid.uuid128[9], descr_id->uuid.uuid.uuid128[10],
                        descr_id->uuid.uuid.uuid128[11], descr_id->uuid.uuid.uuid128[12],
                        descr_id->uuid.uuid.uuid128[13], descr_id->uuid.uuid.uuid128[14],
                        descr_id->uuid.uuid.uuid128[15]);
                }
                else
                {
                    LOG_ERROR("Decr UNKNOWN LEN %d", descr_id->uuid.len);
                }
                /*esp_ble_gattc_write_char_descr (
                        gattc_if,
                        conn_id,
                        srvc_id,
                        char_id,
                        descr_id,
                        2,
                        &value[0],
                        ESP_GATT_WRITE_TYPE_NO_RSP,
                        ESP_GATT_AUTH_REQ_NONE);*/
            }
            break;
        }
        case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        {
            LOG_INFO("NOTIFY_EVT: open.conn_id = %x ", p_data->open.conn_id);
            LOG_INFO("NOTIFY_EVT: reg_for_notify.status = %x ", p_data->reg_for_notify.status);
            esp_gatt_id_t *char_id = &p_data->reg_for_notify.char_id;
            if (p_data->reg_for_notify.status == 0)
            {
                // send notify each 2 seconds
                task_run = true;

                if (char_id->uuid.len == ESP_UUID_LEN_16)
                {
                    LOG_INFO("UUID16: %x", char_id->uuid.uuid.uuid16);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_32)
                {
                    LOG_INFO("UUID32: %x", char_id->uuid.uuid.uuid32);
                }
                else if (char_id->uuid.len == ESP_UUID_LEN_128)
                {
                    LOG_INFO(
                        "UUID128: "
                        "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",
                        char_id->uuid.uuid.uuid128[0], char_id->uuid.uuid.uuid128[1], char_id->uuid.uuid.uuid128[2],
                        char_id->uuid.uuid.uuid128[3], char_id->uuid.uuid.uuid128[4], char_id->uuid.uuid.uuid128[5],
                        char_id->uuid.uuid.uuid128[6], char_id->uuid.uuid.uuid128[7], char_id->uuid.uuid.uuid128[8],
                        char_id->uuid.uuid.uuid128[9], char_id->uuid.uuid.uuid128[10], char_id->uuid.uuid.uuid128[11],
                        char_id->uuid.uuid.uuid128[12], char_id->uuid.uuid.uuid128[13], char_id->uuid.uuid.uuid128[14],
                        char_id->uuid.uuid.uuid128[15]);
                }
                else
                {
                    LOG_ERROR("UNKNOWN LEN %d", char_id->uuid.len);
                }

                uint16_t notify_en = 1;
                esp_err_t err = esp_ble_gattc_write_char_descr(
                    gattc_if, conn_id, &test_service_id, &p_data->reg_for_notify.char_id, &notify_descr_id,
                    sizeof(notify_en), (uint8_t *)&notify_en, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
                if (err != 0)
                {
                    ESP_LOGE(TAG, "write char failed");
                }
            }
            else
            {
                ESP_LOGE(TAG, "notify register failed");
            }
            break;
        }
        case ESP_GATTC_SEARCH_CMPL_EVT:
            conn_id = p_data->search_cmpl.conn_id;
            LOG_INFO("SEARCH_CMPL: conn_id = %x, status %d", conn_id, p_data->search_cmpl.status);
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
            switch (scan_result->scan_rst.ble_evt_type)
            {
                case ESP_BLE_EVT_CONN_ADV:
                    ESP_LOGI(TAG, "==> CONN_ADV");
                    ESP_LOGI(TAG, "BDA %x,%x,%x,%x,%x,%x:", scan_result->scan_rst.bda[0], scan_result->scan_rst.bda[1],
                             scan_result->scan_rst.bda[2], scan_result->scan_rst.bda[3], scan_result->scan_rst.bda[4],
                             scan_result->scan_rst.bda[5]);
                    ESP_LOGI(TAG, "==> RSSI: %d", scan_result->scan_rst.rssi);
                    ESP_LOGI(TAG, "==> address type: %d", scan_result->scan_rst.ble_addr_type);
                    /*esp_ble_gap_stop_scanning();
                    esp_err_t err = esp_ble_gattc_open(client_if,
                    scan_result->scan_rst.bda, true); ESP_LOGI(TAG, "==>
                    esp_ble_gattc_open %d", err);*/
                    break;
                case ESP_BLE_EVT_CONN_DIR_ADV:
                    ESP_LOGI(TAG, "==> CONN_DIR_ADV");
                    break;
                case ESP_BLE_EVT_DISC_ADV:
                    ESP_LOGI(TAG, "==> DISC_ADV");
                    break;
                case ESP_BLE_EVT_NON_CONN_ADV:
                    ESP_LOGI(TAG, "==> NON_CONN_ADV");
                    break;
                case ESP_BLE_EVT_SCAN_RSP:
                    ESP_LOGI(TAG, "==> receive scan response");
                    LOG_INFO("BDA %x,%x,%x,%x,%x,%x:", scan_result->scan_rst.bda[0], scan_result->scan_rst.bda[1],
                             scan_result->scan_rst.bda[2], scan_result->scan_rst.bda[3], scan_result->scan_rst.bda[4],
                             scan_result->scan_rst.bda[5]);
                    break;
            }
            // esp_ble_gattc_get_characteristic(gattc_if, conn_id,
            // &p_data->search_res.srvc_id, NULL);
            break;
        default:
            break;
    }
}

static void led_task(void *arg)
{
    esp_err_t err;
    uint16_t notify_en = 0;

    while (1)
    {
        ESP_LOGI(TAG, "led_task is running");
        if (task_run)
        {
            if (toggle == 0)
            {
                notify_en = 0;
                err = esp_ble_gattc_write_char_descr(client_if, client_conn, &test_service_id, &test_char_id,
                                                     &notify_descr_id, sizeof(notify_en), (uint8_t *)&notify_en,
                                                     ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
                if (err != 0)
                {
                    ESP_LOGE(TAG, "BLE charateristic write failed");
                }
                toggle = 1;
                ESP_LOGE(TAG, "BLE charateristic write notify 0");
            }
            else
            {
                notify_en = 1;
                err = esp_ble_gattc_write_char_descr(client_if, client_conn, &test_service_id, &test_char_id,
                                                     &notify_descr_id, sizeof(notify_en), (uint8_t *)&notify_en,
                                                     ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
                if (err != 0)
                {
                    ESP_LOGE(TAG, "BLE charateristic write failed");
                }
                toggle = 0;
                ESP_LOGE(TAG, "BLE charateristic write notify 1");
            }
        }
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void ble_client_appRegister(void)
{
    LOG_INFO("register callback");

    // register the scan callback function to the Generic Access Profile (GAP)
    // module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK)
    {
        LOG_ERROR("gap register error, error code = %x", status);
        return;
    }

    // register the callback function to the Generic Attribute Profile (GATT)
    // Client (GATTC) module
    if ((status = esp_ble_gattc_register_callback(esp_gattc_cb)) != ESP_OK)
    {
        LOG_ERROR("gattc register error, error code = %x", status);
        return;
    }
    esp_ble_gattc_app_register(simpleClient_id);
    esp_ble_gap_set_scan_params(&ble_scan_params);
}

void gattc_client_test(void)
{
    esp_bluedroid_init();
    esp_bluedroid_enable();
    ble_client_appRegister();
}

void app_main()
{
    esp_bt_controller_init();
    esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    gattc_client_test();

    xTaskCreate(&led_task, "led_task", 2048, NULL, 6, &ledTaskHandle);
}
