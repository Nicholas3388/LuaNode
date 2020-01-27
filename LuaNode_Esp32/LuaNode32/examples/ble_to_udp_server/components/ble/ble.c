#include "ble.h"

#include <string.h>

#include "bt.h"
#include "bt_types.h"
#include "bta_api.h"
#include "bta_gatt_api.h"
#include "btm_api.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include "my_list.h"
#include "user_config.h"

#define SCAN_DURATION 5

// static const char *TAG = "ble";

static uint16_t client_id = 0xEE;
static esp_gatt_if_t client_if;
static esp_gatt_status_t status = ESP_GATT_ERROR;

static esp_ble_scan_params_t ble_scan_params = {.scan_type = BLE_SCAN_TYPE_PASSIVE,
                                                .own_addr_type = ESP_PUBLIC_ADDR,
                                                .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
                                                .scan_interval = 0x640,
                                                .scan_window = 0x640};

extern void wifi_init_task(void);
extern void initialise_wifi(void);

static void get_uuid(const char *data, char *res)
{
    int j = 0;
    char k[4] = {0x0};
    k[3] = 0x00;
    int uuid_len = 16;
    /*for (j = 9; j < 9+uuid_len; j++) {
            sprintf(k, "%02X ", data[j]);
            strcat(res, k);
    }*/
    for (j = 9; j < 9 + 4; j++)
    {
        sprintf(k, "%02X", data[j]);
        strcat(res, k);
    }
    strcat(res, "-");
    for (j = 9 + 4; j < 9 + 4 + 2; j++)
    {
        sprintf(k, "%02X", data[j]);
        strcat(res, k);
    }
    strcat(res, "-");
    for (j = 9 + 4 + 2; j < 9 + 4 + 2 + 2; j++)
    {
        sprintf(k, "%02X", data[j]);
        strcat(res, k);
    }
    strcat(res, "-");
    for (j = 9 + 4 + 2 + 2; j < 9 + 4 + 2 + 2 + 2; j++)
    {
        sprintf(k, "%02X", data[j]);
        strcat(res, k);
    }
    strcat(res, "-");
    for (j = 9 + 4 + 2 + 2 + 2; j < 9 + uuid_len; j++)
    {
        sprintf(k, "%02X", data[j]);
        strcat(res, k);
    }
}

static bool is_valid_ibeacon(const char *package)
{
    if (package[0] != 0x02)
    {
        return false;
    }
    return true;
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
            uint32_t duration = SCAN_DURATION;
            esp_ble_gap_start_scanning(duration);
            break;
        }
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
        {
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
            switch (scan_result->scan_rst.search_evt)
            {
                case ESP_GAP_SEARCH_INQ_RES_EVT:
#ifdef ENABLE_SCAN_OUTPUT
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
                    ESP_LOGI(TAG, "BDA %x,%x,%x,%x,%x,%x:", scan_result->scan_rst.bda[0], scan_result->scan_rst.bda[1],
                             scan_result->scan_rst.bda[2], scan_result->scan_rst.bda[3], scan_result->scan_rst.bda[4],
                             scan_result->scan_rst.bda[5]);
#endif
                    /*for (int i = 0; i < 6; i++) {
                        server_dba[i]=scan_result->scan_rst.bda[i];
                    }*/
                    if (!is_valid_ibeacon((const char *)scan_result->scan_rst.ble_adv))
                    {
#ifdef ENABLE_SCAN_OUTPUT
                        ESP_LOGI(TAG, "not an ibeacon package");
#endif
                        break;
                    }

                    adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL,
                                                        &adv_name_len);
#ifdef ENABLE_SCAN_OUTPUT
                    ESP_LOGI(TAG, "adv_name_len=%x\n", adv_name_len);
                    ESP_LOGI(TAG, "rssi: %d", scan_result->scan_rst.rssi);
#endif
                    char uuid[64] = {0};
                    get_uuid((const char *)scan_result->scan_rst.ble_adv, uuid);
#ifdef ENABLE_SCAN_OUTPUT
                    ESP_LOGI(TAG, "%s", uuid);
#endif

                    char buff[BDA_SIZE] = {0};
                    scan_list_t *tmp = list_new_item();
                    if (tmp == NULL)
                    {
                        ESP_LOGE(TAG, "cannot alloc for list item!");
                        break;
                    }
                    sprintf(buff, "%02x%02x%02x%02x%02x%02x", scan_result->scan_rst.bda[0],
                            scan_result->scan_rst.bda[1], scan_result->scan_rst.bda[2], scan_result->scan_rst.bda[3],
                            scan_result->scan_rst.bda[4], scan_result->scan_rst.bda[5]);
                    memcpy(tmp->bda, buff, 12);
                    tmp->bda[12] = 0;
                    memcpy(tmp->uuid, uuid, 48);
                    tmp->uuid[48] = 0;
                    tmp->rssi = scan_result->scan_rst.rssi;
                    list_insert_to_head(tmp);

                    // ESP_LOGI(TAG, "dev name: %s", adv_name);
                    /*for (int j = 0; j < adv_name_len; j++) {
                        LOG_INFO("a%d %x %c = d%d %x %c",j, adv_name[j],
                    adv_name[j],j, device_name[j], device_name[j]);
                    }*/

                    /*if (strncmp((char *)adv_name, device_name,adv_name_len) ==
            0 && connet == false) { connet = true; ESP_LOGI(TAG, "==> address
            type: %d, dev name: %s", scan_result->scan_rst.ble_addr_type,
            adv_name); LOG_INFO("Connect to the remote device.");
            esp_ble_gap_stop_scanning();
            esp_ble_gattc_open(client_if, scan_result->scan_rst.bda, true);
                            memcpy(tar_dev_mac, scan_result->scan_rst.bda, 6);
                    }*/
                    break;
                case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                    // esp_ble_gap_cb_param_t *scan_result =
                    // (esp_ble_gap_cb_param_t *)param;
#ifdef ENABLE_SCAN_OUTPUT
                    ESP_LOGI(TAG, "=========================");
                    ESP_LOGI(TAG, "Scan over");
                    ESP_LOGI(TAG, "=========================");
#endif
                    esp_ble_gap_stop_scanning();
                    wifi_init_task();
                    initialise_wifi();

#ifdef ENABLE_SCAN_OUTPUT
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
#endif
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

void ble_client_app_register(void)
{
    ESP_LOGI(TAG, "register gap callback");

    // register the scan callback function to the Generic Access Profile (GAP)
    // module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(TAG, "gap register error, error code = %x", status);
        return;
    }

    esp_ble_gap_set_scan_params(&ble_scan_params);
}

void ble_init(void)
{
    esp_bt_controller_init();
    esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    int ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(TAG, "init bluedroid failed\n");
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(TAG, "init bluedroid failed2\n");
        return;
    }

    // ble_client_app_register();
}

void ble_start_scanning(void) { esp_ble_gap_start_scanning(SCAN_DURATION); }