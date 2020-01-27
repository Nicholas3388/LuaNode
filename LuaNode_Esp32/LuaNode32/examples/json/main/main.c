/**
 * Nicholas3388
 * 2017.05.21
 */

#include <stddef.h>
#include <stdlib.h>

#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "main";

// return unformated json string
static char *json_encode(void)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "name", "Jack");

    cJSON *subArray = cJSON_CreateArray();

    cJSON *subItem1 = cJSON_CreateObject();
    cJSON_AddStringToObject(subItem1, "sex", "male");
    cJSON *subItem2 = cJSON_CreateObject();
    cJSON_AddStringToObject(subItem2, "address", "Beijing");
    cJSON *subItem3 = cJSON_CreateObject();
    cJSON_AddBoolToObject(subItem3, "has_children", true);
    cJSON *subItem4 = cJSON_CreateObject();
    cJSON_AddItemToObject(subItem4, "weight",
                          cJSON_CreateNumber(62));  // or cJSON_AddNumberToObject(subItem4, 62)
    cJSON *subItem5 = cJSON_CreateObject();
    cJSON_AddItemToObject(subItem5, "height", cJSON_CreateDouble(1.80, 2));

    cJSON_AddItemToArray(subArray, subItem1);
    cJSON_AddItemToArray(subArray, subItem2);
    cJSON_AddItemToArray(subArray, subItem3);
    cJSON_AddItemToArray(subArray, subItem4);
    cJSON_AddItemToArray(subArray, subItem5);

    cJSON_AddItemToObject(json, "subArray", subArray);

    char *formated_str = cJSON_Print(json);
    char *unformated_str = cJSON_PrintUnformatted(json);
    ESP_LOGI(TAG, "-----------  formated  -----------");
    ESP_LOGI(TAG, "%s", formated_str);
    ESP_LOGI(TAG, "----------- unformated -----------");
    ESP_LOGI(TAG, "%s", unformated_str);

    free(formated_str);  // do not forget to free string buffer

    if (NULL != json)
    {
        cJSON_Delete(json);
        json = NULL;
    }

    return unformated_str;
}

static void json_decode(char *unformated_string)
{
    int i;
    cJSON *json = cJSON_Parse(unformated_string);
    if (NULL != json)
    {
        cJSON *temp = cJSON_GetObjectItem(json, "name");
        if (NULL != temp)
        {
            ESP_LOGI(TAG, "name : %s", temp->valuestring);
        }

        temp = cJSON_GetObjectItem(json, "subArray");
        if (NULL != temp)
        {
            ESP_LOGI(TAG, "subArray : [");
            int len = cJSON_GetArraySize(temp);
            for (i = 0; i < len; i++)
            {
                cJSON *subItem = cJSON_GetArrayItem(temp, i);
                if (NULL != subItem)
                {
                    cJSON *sex = cJSON_GetObjectItem(subItem, "sex");
                    if (NULL != sex)
                    {
                        ESP_LOGI(TAG, "\tsex : %s", sex->valuestring);
                    }
                    cJSON *addr = cJSON_GetObjectItem(subItem, "address");
                    if (NULL != addr)
                    {
                        ESP_LOGI(TAG, "\taddress : %s", addr->valuestring);
                    }
                    cJSON *child = cJSON_GetObjectItem(subItem, "has_children");
                    if (NULL != child)
                    {
                        switch (child->type)
                        {
                            case cJSON_False:
                                ESP_LOGI(TAG, "\thas_children : false");
                                break;
                            case cJSON_True:
                                ESP_LOGI(TAG, "\thas_children : true");
                                break;
                        }
                    }
                    cJSON *weight = cJSON_GetObjectItem(subItem, "weight");
                    if (NULL != weight)
                    {
                        ESP_LOGI(TAG, "\tweight : %d", weight->valueint);
                    }
                    cJSON *height = cJSON_GetObjectItem(subItem, "height");
                    if (NULL != height)
                    {
                        ESP_LOGI(TAG, "\theight : %lf", height->valuedouble);
                    }
                }
            }
            ESP_LOGI(TAG, "]");
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "enter main");

    char *json_str = json_encode();
    ESP_LOGI(TAG, "==================================");
    json_decode(json_str);

    free(json_str);  // do not forget to free string buffer
}