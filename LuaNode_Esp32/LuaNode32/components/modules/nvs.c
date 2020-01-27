/**
 * NVS module for Lua
 *
 * function is not support now
 *
 * Nicholas3388
 * 2017. 04.10
 */
#include "nvs.h"

#include "esp_log.h"
#include "esp_system.h"
#include "lauxlib.h"
#include "lrotable.h"
#include "lua.h"
#include "lualib.h"
#include "nvs_flash.h"
#include "user_config.h"

static const char* TAG = "nvs_lua_mod";
static nvs_handle handle;
static int is_open = 0;

static bool key_exist(lua_State* L, const char* key)
{
    bool res = false;
    int32_t out_value = 4096;
    esp_err_t err = nvs_get_i32(handle, key, &out_value);
    switch (err)
    {
        case ESP_ERR_NVS_NOT_FOUND:
            res = false;
            break;
        case ESP_OK:
            res = true;
            break;
        default:
            break;
    }

    uint8_t out_str[128];
    size_t length = sizeof(out_str);
    err = nvs_get_str(handle, key, out_str, &length);
    switch (err)
    {
        case ESP_ERR_NVS_NOT_FOUND:
            res = false;
            break;
        case ESP_OK:
            res = true;
            break;
        default:
            break;
    }
    return res;
}

// ret = nvs.exist(key)
//	ret=1 if key exist, otherwise, ret=0
static int lnvs_key_exist(lua_State* L)
{
    size_t sl = 0;
    if (is_open != 1)
    {
        return luaL_error(L, "call nvs.open first");
    }

    int argc = lua_gettop(L);
    if (argc < 1)
    {
        return luaL_error(L, "expected 1 args");
    }

    const char* key = luaL_checklstring(L, 1, &sl);
    if (key == NULL)
    {
        return luaL_error(L, "the key is null");
    }

    if (key_exist(L, key))
    {
        lua_pushinteger(L, 1);
        return 1;
    }

    lua_pushinteger(L, 0);
    return 1;
}

// res = nvs.open()
// res=0 if open successfully, otherwise, failed
static int lnvs_open(lua_State* L)
{
    int res = 0;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        res = 1;
        is_open = 0;
    }
    else
    {
        res = 0;
        is_open = 1;
    }
    lua_pushinteger(L, res);
    return 1;
}

// nvs.close()
static int lnvs_close(lua_State* L)
{
    nvs_close(handle);
    is_open = 0;
    return 0;
}

// res = nvs.put(key, value)
//	key: string type
//  value: table/string/data/boolean
static int lnvs_put(lua_State* L)
{
    size_t sl = 0;
    if (is_open != 1)
    {
        return luaL_error(L, "call nvs.open first");
    }

    int argc = lua_gettop(L);
    if (argc < 2)
    {
        return luaL_error(L, "expected 2 args");
    }

    const char* key = luaL_checklstring(L, 1, &sl);
    if (key == NULL)
    {
        return luaL_error(L, "the key is null");
    }

    if (key_exist(L, key))
    {
        return luaL_error(L, "key exist! call nvs.remove to remove it first");
    }

    if (lua_type(L, 2) == LUA_TTABLE)
    {
        ESP_LOGE(TAG, "param is table");
        goto ret_err;
    }
    else if (lua_type(L, 2) == LUA_TSTRING)
    {
        const char* val = luaL_checklstring(L, 2, &sl);
        esp_err_t err = nvs_set_str(handle, key, val);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "nvs set error");
            goto ret_err;
        }
        err = nvs_commit(handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "nvs commit error");
            goto ret_err;
        }
    }
    else if (lua_type(L, 2) == LUA_TNUMBER)
    {
        int val = luaL_checknumber(L, 2);
        esp_err_t err = nvs_set_i32(handle, key, val);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "nvs set error");
            goto ret_err;
        }
        err = nvs_commit(handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "nvs commit error");
            goto ret_err;
        }
    }
    else if (lua_type(L, 2) == LUA_TBOOLEAN)
    {
        ESP_LOGI(TAG, "param is boolean");
        bool val = (lua_toboolean(L, 2) ? true : false);
        int saveVal = (val ? 1 : 0);
        esp_err_t err = nvs_set_i32(handle, key, saveVal);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "nvs set error");
            goto ret_err;
        }
        err = nvs_commit(handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "nvs commit error");
            goto ret_err;
        }
    }
    else
    {
        ESP_LOGI(TAG, "unknown param type");
        goto ret_err;
    }

    lua_pushinteger(L, 0);
    return 1;
ret_err:
    lua_pushinteger(L, 1);
    return 1;
}

// value = nvs.get(key)
static int lnvs_get(lua_State* L)
{
    size_t sl = 0;
    if (is_open != 1)
    {
        return luaL_error(L, "call nvs.open first");
    }

    int argc = lua_gettop(L);
    if (argc < 1)
    {
        return luaL_error(L, "expected at least 1 args");
    }

    int32_t out_value = 4096;
    const char* key = luaL_checklstring(L, 1, &sl);

    esp_err_t err = nvs_get_i32(handle, key, &out_value);
    switch (err)
    {
        case ESP_ERR_NVS_NOT_FOUND:
            // ESP_LOGE(TAG, "value not found");
            break;
        case ESP_ERR_NVS_INVALID_HANDLE:
            return luaL_error(L, "invalid nvs handle");
        case ESP_ERR_NVS_INVALID_NAME:
            return luaL_error(L, "invalid name");
        case ESP_ERR_NVS_INVALID_LENGTH:
            break;
        case ESP_OK:
            lua_pushinteger(L, out_value);
            return 1;
        default:
            break;
    }

    uint8_t out_str[128];
    size_t length = sizeof(out_str);
    err = nvs_get_str(handle, key, out_str, &length);
    switch (err)
    {
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGE(TAG, "value not found");
            lua_pushnil(L);
            break;
        case ESP_ERR_NVS_INVALID_HANDLE:
            return luaL_error(L, "invalid nvs handle");
        case ESP_ERR_NVS_INVALID_NAME:
            return luaL_error(L, "invalid name");
        case ESP_ERR_NVS_INVALID_LENGTH:
            break;
        case ESP_OK:
            lua_pushlstring(L, out_str, length);
            return 1;
        default:
            break;
    }

    return 1;
}

// res = nvs.remove(key)
//	res=0 if remove successfully, otherwise, res!=0
static int lnvs_remove(lua_State* L)
{
    size_t sl = 0;
    if (is_open != 1)
    {
        return luaL_error(L, "call nvs.open first");
    }

    int argc = lua_gettop(L);
    if (argc < 1)
    {
        return luaL_error(L, "expected 1 args");
    }

    const char* key = luaL_checklstring(L, 1, &sl);
    if (key == NULL)
    {
        return luaL_error(L, "the key is null");
    }

    if (!key_exist(L, key))
    {
        return luaL_error(L, "key doesn't exist!");
    }

    esp_err_t err = nvs_erase_key(handle, key);

    switch (err)
    {
        case ESP_OK:
            break;
        case ESP_ERR_NVS_INVALID_HANDLE:
            return luaL_error(L, "invalid handle");
        case ESP_ERR_NVS_READ_ONLY:
            return luaL_error(L, "nvs is readonly");
        default:
            break;
    }

    lua_pushinteger(L, 0);
    return 1;
}

// res = nvs.clearall()
static int lnvs_clearall(lua_State* L)
{
    int res = 0;
    esp_err_t err = nvs_erase_all(handle);
    if (err != ESP_OK)
    {
        res = 1;
    }
    lua_pushinteger(L, res);
    return 1;
}

#include "lrodefs.h"

const LUA_REG_TYPE nvs_map[] = {
    {LSTRKEY("open"), LFUNCVAL(lnvs_open)},       {LSTRKEY("close"), LFUNCVAL(lnvs_close)},
    {LSTRKEY("put"), LFUNCVAL(lnvs_put)},         {LSTRKEY("get"), LFUNCVAL(lnvs_get)},
    {LSTRKEY("remove"), LFUNCVAL(lnvs_remove)},   {LSTRKEY("clearall"), LFUNCVAL(lnvs_clearall)},
    {LSTRKEY("exist"), LFUNCVAL(lnvs_key_exist)}, {LNILKEY, LNILVAL}};

int luaopen_nvs(lua_State* L)
{
    // pre-handle
    return 0;
}
