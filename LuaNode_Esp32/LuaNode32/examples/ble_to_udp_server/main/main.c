/**
 * ESP32 beacon scan and udp send demo
 *
 * Nicholas3388
 * 2017.4.12
 */

#include <string.h>

#include "ble.h"
#include "driver/gpio.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "my_list.h"
#include "my_uart.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "udp_task.h"
#include "user_config.h"
#include "utils.h"

#define LED_GPIO 2

static const char *TAG = "main";
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static tcpip_adapter_ip_info_t ip_info;

static bool panic = false;
static bool wifi_check = false;
static int wifi_check_count = 0;
uint8_t wifi_mac[6] = {0};
bool toggle = false;
unsigned char wifi_ssid[WIFI_SSID_MAX_LEN] = {0};
unsigned char wifi_pass[WIFI_PASS_MAX_LEN] = {0};
unsigned char srv_ip[SRV_IP_MAX_LEN] = {0};
int srv_port = 0;
static bool is_task_running = false;
static nvs_handle my_nvs_handle;

extern void connect_server(const char *ip, int port);

static void show_copyright(void)
{
    ESP_LOGI(TAG, "################################");
    ESP_LOGI(TAG, "##         Test app           ##");
    ESP_LOGI(TAG, "##         2017.4.12          ##");
    ESP_LOGI(TAG, "################################");
}

void set_panic(void) { panic = true; }

void clear_panic(void) { panic = false; }

esp_err_t save_string(const char *key, char *str)
{
    esp_err_t err = nvs_set_str(my_nvs_handle, key, str);
    nvs_commit(my_nvs_handle);
    return err;
}

esp_err_t save_int32(const char *key, int data)
{
    esp_err_t err = nvs_set_i32(my_nvs_handle, key, data);
    nvs_commit(my_nvs_handle);
    return err;
}

int get_int32(const char *key)
{
    int out_value;
    nvs_get_i32(my_nvs_handle, key, &out_value);
    return out_value;
}

void get_string(const char *key, char *out_value, size_t *length)
{
    nvs_get_str(my_nvs_handle, key, out_value, length);
}

void clear_storage(void)
{
    nvs_erase_all(my_nvs_handle);
    nvs_commit(my_nvs_handle);
}

void close_nvs(void) { nvs_close(my_nvs_handle); }

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

void initialise_wifi(void)
{
    if (is_task_running)
    {
#ifdef ENABLE_SCAN_OUTPUT
        ESP_LOGE(TAG, "task was running, no need to init again");
#endif
        send_all_data();

        // restart scanning
#ifdef ENABLE_SCAN_OUTPUT
        ESP_LOGI(TAG, "###############################################");
        ESP_LOGI(TAG, "restart scanning ......");
#endif
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ble_start_scanning();
        return;
    }

    if (strlen((const char *)wifi_ssid) == 0)
    {
        ESP_LOGE(TAG, "expected ssid");
    }

    if (strlen((const char *)wifi_pass) == 0)
    {
        ESP_LOGE(TAG, "expected password");
    }

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = {0},
                .password = {0},
            },
    };
    memcpy(wifi_config.sta.ssid, wifi_ssid, strlen((const char *)wifi_ssid));
    memcpy(wifi_config.sta.password, wifi_pass, strlen((const char *)wifi_pass));
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...PASS %s", wifi_config.sta.ssid, wifi_config.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    esp_err_t er = esp_wifi_get_mac(ESP_IF_WIFI_STA, wifi_mac);
    if (er != ESP_OK)
    {
        ESP_LOGE(TAG, "get wifi MAC failed");
    }
    else
    {
        ESP_LOGI(TAG, "WiFi MAC: %02x%02x%02x%02x%02x%02x", wifi_mac[0], wifi_mac[1], wifi_mac[2], wifi_mac[3],
                 wifi_mac[4], wifi_mac[5]);
    }

    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_check = true;  // start wifi checking
}

static void tmp_task(void *pvParameters)
{
    is_task_running = true;
    while (1)
    {
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Got IP");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        if (tcpip_adapter_get_ip_info(ESP_IF_WIFI_STA, &ip_info) == 0)
        {
            ESP_LOGI(TAG, "~~~~~~~~~~~");
            ESP_LOGI(TAG, "IP:" IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, "MASK:" IPSTR, IP2STR(&ip_info.netmask));
            ESP_LOGI(TAG, "GW:" IPSTR, IP2STR(&ip_info.gw));
            ESP_LOGI(TAG, "~~~~~~~~~~~");
        }
        wifi_check = false;  // stop wifi checking

        if (strlen((const char *)srv_ip) == 0)
        {
            ESP_LOGE(TAG, "expected server IP, reset board to setup again!");
            break;
        }

        if (srv_port == 0)
        {
            ESP_LOGE(TAG, "expected server port, reset board to setup again!");
            break;
        }

        // start espconn
        connect_server((const char *)srv_ip, srv_port);

        while (1)
        {
            vTaskDelay(3000 / portTICK_RATE_MS);  // prevent trigger watch dog
        }
    }
    vTaskDelete(NULL);
    is_task_running = false;
}

void wifi_init_task(void)
{
    if (is_task_running)
    {
#ifdef ENABLE_SCAN_OUTPUT
        ESP_LOGE(TAG, "task was running");
#endif
        return;
    }
    xTaskCreate(&tmp_task, "tmp_task", TMP_TASK_STACK_SIZE, NULL, TMP_TASK_PRIO, NULL);
}

static void led_task(void *pvParameters)
{
    while (1)
    {
        if (panic)
        {
            if (toggle)
            {
                gpio_set_level(LED_GPIO, 1);
            }
            else
            {
                gpio_set_level(LED_GPIO, 0);
            }
            toggle = !toggle;
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
        else
        {
            // ESP_LOGI(TAG, "tmp_task loop");
            vTaskDelay(1000 / portTICK_RATE_MS);  // prevent trigger watch dog
        }

        if (wifi_check)
        {
            wifi_check_count++;
            if (wifi_check_count >= WIFI_CHECK_TIMEOUT)
            {
                set_panic();
                wifi_check_count = 0;
            }
        }
    }
    vTaskDelete(NULL);
}

static void led_task_init(void)
{
    gpio_pad_select_gpio(LED_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 1);

    xTaskCreate(&led_task, "led_task", LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIO, NULL);
}

static void nvs_init(void)
{
    nvs_flash_init();
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%d) opening NVS!\n", err);
    }
    else
    {
        ESP_LOGI(TAG, "NVS open OK");
    }
}

static void load_config(void)
{
    size_t length = WIFI_SSID_MAX_LEN;
    esp_err_t er;
    er = nvs_get_str(my_nvs_handle, CMD_SSID, (char *)wifi_ssid, &length);
    ESP_LOGI(TAG, "load ssid: %s", wifi_ssid);
    if (er != ESP_OK)
    {
        ESP_LOGE(TAG, "SSID not saved before");
        goto load_config_err;
    }
    else
    {
        ESP_LOGI(TAG, "preset SSID: %s", wifi_ssid);
    }

    er = nvs_get_str(my_nvs_handle, CMD_PASS, (char *)wifi_pass, &length);
    ESP_LOGI(TAG, "load password: %s", wifi_pass);
    if (er != ESP_OK)
    {
        ESP_LOGE(TAG, "password not saved before");
        goto load_config_err;
    }
    else
    {
        ESP_LOGI(TAG, "preset password: %s", wifi_pass);
    }

    length = SRV_IP_MAX_LEN;
    er = nvs_get_str(my_nvs_handle, CMD_IP, (char *)srv_ip, &length);
    ESP_LOGI(TAG, "load ip: %s", srv_ip);
    if (length == 0 || er != ESP_OK)
    {
        ESP_LOGE(TAG, "ip not saved before");
        goto load_config_err;
    }
    else
    {
        ESP_LOGI(TAG, "preset IP: %s", srv_ip);
    }

    char port[SRV_IP_MAX_LEN] = {0};
    er = nvs_get_str(my_nvs_handle, CMD_PORT, (char *)port, &length);
    if (er != ESP_OK)
    {
        ESP_LOGE(TAG, "port get failed");
        goto load_config_err;
    }
    else
    {
        ESP_LOGI(TAG, "preset port: %s", port);
    }
    srv_port = str2num((const char *)port, strlen(port));
    ESP_LOGI(TAG, "preset port in num: %d", srv_port);

    // ble scan and connect to wifi directly
    ESP_LOGI(TAG, "BLE scanning ......");
    ble_client_app_register();
    return;
load_config_err:
    ESP_LOGE(TAG, "some parameters not set");
    // set_panic();
    return;
}

void app_main()
{
    ESP_LOGI(TAG, "App started");
    nvs_init();

    show_copyright();

    list_init();
    ble_init();
    uart_init();

    led_task_init();

    wifi_event_group = xEventGroupCreate();

    show_help_info();
    load_config();
}