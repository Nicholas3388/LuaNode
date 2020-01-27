// Module for interfacing with WIFI

#include "c_stdlib.h"
#include "c_string.h"
#include "c_types.h"
#include "esp_misc.h"
#include "esp_phy.h"
#include "esp_softap.h"
#include "esp_sta.h"
#include "esp_timer.h"
#include "lauxlib.h"
#include "lrodefs.h"
#include "lrotable.h"
#include "lua.h"
#include "lualib.h"
#include "modules.h"
#include "nodemcu_esp_event.h"
#include "platform.h"
#include "user_config.h"
#include "user_interface.h"
//#include "smart.h"
//#include "smartconfig.h"
#include "c_stdio.h"
#include "ip_fmt.h"

typedef void (*fill_cb_arg_fn)(lua_State *L, const system_event_t *evt);
typedef struct
{
    const char *name;
    system_event_id_t event_id;
    fill_cb_arg_fn fill_cb_arg;
} event_desc_t;

typedef enum _sta_status
{
    STA_IDLE = 0,
    STA_CONNECTING,
    STA_WRONGPWD,
    STA_APNOTFOUND,
    STA_FAIL,
    STA_GOTIP
} STA_STATUS;

typedef enum _phy_mode
{
    PHYMODE_B = 0,
    PHYMODE_G,
    PHYMODE_N
} PHY_MODE;

#define NULL_MODE 0x00
#define STATION_MODE 0x01
#define SOFTAP_MODE 0x02
#define STATIONAP_MODE 0x03

#define STATION_IF 0x00
#define SOFTAP_IF 0x01

#define DEFAULT_AP_CHANNEL 11
#define DEFAULT_AP_MAXCONNS 4
#define DEFAULT_AP_BEACON 100

static int wifi_smart_succeed = LUA_NOREF;
static uint8 getap_output_format = 0;

// wifi.sleep variables
#define FPM_SLEEP_MAX_TIME 0xFFFFFFF
static bool FLAG_wifi_force_sleep_enabled = 0;

// variables for wifi event monitor
static sint32_t wifi_status_cb_ref[6] = {LUA_NOREF, LUA_NOREF, LUA_NOREF, LUA_NOREF, LUA_NOREF, LUA_NOREF};
// static os_timer_t wifi_sta_status_timer;
static uint8 prev_wifi_status = 0;

// Forward declarations
static void on_event(const system_event_t *evt);

static void sta_conn(lua_State *L, const system_event_t *evt);
static void sta_disconn(lua_State *L, const system_event_t *evt);
static void sta_authmode(lua_State *L, const system_event_t *evt);
static void sta_got_ip(lua_State *L, const system_event_t *evt);
static void ap_staconn(lua_State *L, const system_event_t *evt);
static void ap_stadisconn(lua_State *L, const system_event_t *evt);
static void ap_probe_req(lua_State *L, const system_event_t *evt);

static void empty_arg(lua_State *L, const system_event_t *evt) {}

static const event_desc_t events[] = {{"sta_start", SYSTEM_EVENT_STA_START, empty_arg},
                                      {"sta_stop", SYSTEM_EVENT_STA_STOP, empty_arg},
                                      {"sta_connected", SYSTEM_EVENT_STA_CONNECTED, sta_conn},
                                      {"sta_disconnected", SYSTEM_EVENT_STA_DISCONNECTED, sta_disconn},
                                      {"sta_authmode_changed", SYSTEM_EVENT_STA_AUTHMODE_CHANGE, sta_authmode},
                                      {"sta_got_ip", SYSTEM_EVENT_STA_GOT_IP, sta_got_ip},

                                      {"ap_start", SYSTEM_EVENT_AP_START, empty_arg},
                                      {"ap_stop", SYSTEM_EVENT_AP_STOP, empty_arg},
                                      {"ap_sta_connected", SYSTEM_EVENT_AP_STACONNECTED, ap_staconn},
                                      {"ap_sta_disconnected", SYSTEM_EVENT_AP_STADISCONNECTED, ap_stadisconn},
                                      {"ap_probe_req", SYSTEM_EVENT_AP_PROBEREQRECVED, ap_probe_req}};

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
static int event_cb[ARRAY_LEN(events)];

nodemcu_esp_event_reg_t esp_event_cb_table[] = {
    {SYSTEM_EVENT_STA_START, on_event},
    {SYSTEM_EVENT_STA_STOP, on_event},
    {SYSTEM_EVENT_STA_CONNECTED, on_event},
    {SYSTEM_EVENT_STA_DISCONNECTED, on_event},
    {SYSTEM_EVENT_STA_AUTHMODE_CHANGE, on_event},
    {SYSTEM_EVENT_STA_GOT_IP, on_event},

    {SYSTEM_EVENT_AP_START, on_event},
    {SYSTEM_EVENT_AP_STOP, on_event},
    {SYSTEM_EVENT_AP_STACONNECTED, on_event},
    {SYSTEM_EVENT_AP_STADISCONNECTED, on_event},
    {SYSTEM_EVENT_AP_PROBEREQRECVED, on_event},
};

#define SET_SAVE_MODE(save)                                                                         \
    do                                                                                              \
    {                                                                                               \
        esp_err_t storage_err = esp_wifi_set_storage(save ? WIFI_STORAGE_FLASH : WIFI_STORAGE_RAM); \
        if (storage_err != ESP_OK)                                                                  \
            return luaL_error(L, "failed to update wifi storage, code %d", storage_err);            \
    } while (0)

bool luaL_optbool(lua_State *L, int idx, bool def)
{
    if (lua_isboolean(L, idx))
        return lua_toboolean(L, idx);
    else
        return def;
}

static lua_State *gL = NULL;
static int scan_cb_ref = LUA_NOREF;

// Lua: realmode = getmode()

static int wifi_getmode(lua_State *L)
{
    wifi_mode_t mode;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK)
        return luaL_error(L, "failed to get mode, code %d", err);
    lua_pushinteger(L, mode);
    return 1;
}

static int event_idx_by_name(const char *name)
{
    for (unsigned i = 0; i < ARRAY_LEN(events); ++i)
        if (strcmp(events[i].name, name) == 0)
            return i;
    return -1;
}

static int event_idx_by_id(system_event_id_t id)
{
    for (unsigned i = 0; i < ARRAY_LEN(events); ++i)
        if (events[i].event_id == id)
            return i;
    return -1;
}

static void sta_conn(lua_State *L, const system_event_t *evt)
{
    lua_pushlstring(L, (const char *)evt->event_info.connected.ssid, evt->event_info.connected.ssid_len);
    lua_setfield(L, -2, "ssid");

    char bssid_str[MAC_STR_SZ];
    macstr(bssid_str, evt->event_info.connected.bssid);
    lua_pushstring(L, bssid_str);
    lua_setfield(L, -2, "bssid");

    lua_pushinteger(L, evt->event_info.connected.channel);
    lua_setfield(L, -2, "channel");

    lua_pushinteger(L, evt->event_info.connected.authmode);
    lua_setfield(L, -2, "auth");
}

static void sta_disconn(lua_State *L, const system_event_t *evt)
{
    lua_pushlstring(L, (const char *)evt->event_info.disconnected.ssid, evt->event_info.disconnected.ssid_len);
    lua_setfield(L, -2, "ssid");

    char bssid_str[MAC_STR_SZ];
    macstr(bssid_str, evt->event_info.disconnected.bssid);
    lua_pushstring(L, bssid_str);
    lua_setfield(L, -2, "bssid");

    lua_pushinteger(L, evt->event_info.disconnected.reason);
    lua_setfield(L, -2, "reason");
}

static void sta_authmode(lua_State *L, const system_event_t *evt)
{
    lua_pushinteger(L, evt->event_info.auth_change.old_mode);
    lua_setfield(L, -2, "old_mode");
    lua_pushinteger(L, evt->event_info.auth_change.new_mode);
    lua_setfield(L, -2, "new_mode");
}

static void sta_got_ip(lua_State *L, const system_event_t *evt)
{
    char ipstr[IP_STR_SZ] = {0};
    ip4str(ipstr, &evt->event_info.got_ip.ip_info.ip);
    lua_pushstring(L, ipstr);
    lua_setfield(L, -2, "ip");

    ip4str(ipstr, &evt->event_info.got_ip.ip_info.netmask);
    lua_pushstring(L, ipstr);
    lua_setfield(L, -2, "netmask");

    ip4str(ipstr, &evt->event_info.got_ip.ip_info.gw);
    lua_pushstring(L, ipstr);
    lua_setfield(L, -2, "gw");
}

static void ap_staconn(lua_State *L, const system_event_t *evt)
{
    char mac[MAC_STR_SZ];
    macstr(mac, evt->event_info.sta_connected.mac);
    lua_pushstring(L, mac);
    lua_setfield(L, -2, "mac");

    lua_pushinteger(L, evt->event_info.sta_connected.aid);
    lua_setfield(L, -2, "id");
}

static void ap_stadisconn(lua_State *L, const system_event_t *evt)
{
    char mac[MAC_STR_SZ];
    macstr(mac, evt->event_info.sta_disconnected.mac);
    lua_pushstring(L, mac);
    lua_setfield(L, -2, "mac");

    lua_pushinteger(L, evt->event_info.sta_disconnected.aid);
    lua_setfield(L, -2, "id");
}

static void ap_probe_req(lua_State *L, const system_event_t *evt)
{
    char str[MAC_STR_SZ];
    macstr(str, evt->event_info.ap_probereqrecved.mac);
    lua_pushstring(L, str);
    lua_setfield(L, -2, "from");

    lua_pushinteger(L, evt->event_info.ap_probereqrecved.rssi);
    lua_setfield(L, -2, "rssi");
}

static void on_event(const system_event_t *evt)
{
    int idx = event_idx_by_id(evt->event_id);
    if (idx < 0 || event_cb[idx] == LUA_NOREF)
        return;

    lua_State *L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, event_cb[idx]);
    lua_pushstring(L, events[idx].name);
    lua_createtable(L, 0, 5);
    events[idx].fill_cb_arg(L, evt);
    lua_call(L, 2, 0);
}

static int wifi_eventmon(lua_State *L)
{
    const char *event_name = luaL_checkstring(L, 1);
    if (!lua_isnoneornil(L, 2))
        luaL_checkanyfunction(L, 2);
    lua_settop(L, 2);

    int idx = event_idx_by_name(event_name);
    if (idx < 0)
        return luaL_error(L, "unknown event: %s", event_name);

    luaL_unref(L, LUA_REGISTRYINDEX, event_cb[idx]);
    event_cb[idx] = lua_isnoneornil(L, 2) ? LUA_NOREF : luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
}

static int wifi_setmode(lua_State *L)
{
    int mode = luaL_checkinteger(L, 1);
    wifi_mode_t tmp_mode;
    esp_err_t err = esp_wifi_get_mode(&tmp_mode);
    if (err == ESP_OK)
    {
        if (tmp_mode == mode)
        {
            return 0;
        }
    }
    bool save = luaL_optbool(L, 2, false);
    SET_SAVE_MODE(save);
    switch (mode)
    {
        case WIFI_MODE_NULL:
        case WIFI_MODE_STA:
        case WIFI_MODE_AP:
        case WIFI_MODE_APSTA:
            return ((err = esp_wifi_set_mode(mode)) == ESP_OK) ? 0 : luaL_error(L, "failed to set mode, code %d", err);
        default:
            return luaL_error(L, "invalid wifi mode %d", mode);
    }
}

static int wifi_start(lua_State *L)
{
    esp_err_t err = esp_wifi_start();
    return (err == ESP_OK) ? 0 : luaL_error(L, "failed to start wifi, code %d", err);
}

static int wifi_stop(lua_State *L)
{
    esp_err_t err = esp_wifi_stop();
    return (err == ESP_OK) ? 0 : luaL_error(L, "failed to stop wifi, code %d", err);
}

static int wifi_sta_config(lua_State *L)
{
    luaL_checkanytable(L, 1);
    bool save = luaL_optbool(L, 2, false);
    lua_settop(L, 1);

    wifi_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    lua_getfield(L, 1, "ssid");
    size_t len;
    const char *str = luaL_checklstring(L, -1, &len);
    if (len > sizeof(cfg.sta.ssid))
        len = sizeof(cfg.sta.ssid);
    strncpy(cfg.sta.ssid, str, len);

    lua_getfield(L, 1, "pwd");
    str = luaL_optlstring(L, -1, "", &len);
    if (len > sizeof(cfg.sta.password))
        len = sizeof(cfg.sta.password);
    strncpy(cfg.sta.password, str, len);

    lua_getfield(L, 1, "bssid");
    cfg.sta.bssid_set = false;
    if (lua_isstring(L, -1))
    {
        const char *bssid = luaL_checklstring(L, -1, &len);
        const char *fmts[] = {"%hhx%hhx%hhx%hhx%hhx%hhx", "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                              "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx", "%hhx %hhx %hhx %hhx %hhx %hhx", NULL};
        for (unsigned i = 0; fmts[i]; ++i)
        {
            if (sscanf(bssid, fmts[i], &cfg.sta.bssid[0], &cfg.sta.bssid[1], &cfg.sta.bssid[2], &cfg.sta.bssid[3],
                       &cfg.sta.bssid[4], &cfg.sta.bssid[5]) == 6)
            {
                cfg.sta.bssid_set = true;
                break;
            }
        }
        if (!cfg.sta.bssid_set)
            return luaL_error(L, "invalid BSSID: %s", bssid);
    }

    lua_getfield(L, 1, "auto");
    bool auto_conn = luaL_optbool(L, -1, true);

    SET_SAVE_MODE(save);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK)
        return luaL_error(L, "failed to set wifi config, code %d", err);

    if (auto_conn)
        err = esp_wifi_connect();
    if (err != ESP_OK)
        return luaL_error(L, "failed to begin connect, code %d", err);

    err = esp_wifi_set_auto_connect(auto_conn);
    return (err == ESP_OK) ? 0 : luaL_error(L, "failed to set wifi auto-connect, code %d", err);
}

static int wifi_sta_connect(lua_State *L)
{
    esp_err_t err = esp_wifi_connect();
    return (err == ESP_OK) ? 0 : luaL_error(L, "connect failed, code %d", err);
}

static int wifi_sta_disconnect(lua_State *L)
{
    esp_err_t err = esp_wifi_disconnect();
    return (err == ESP_OK) ? 0 : luaL_error(L, "disconnect failed, code %d", err);
}

static int wifi_sta_getconfig(lua_State *L)
{
    wifi_config_t cfg;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK)
        return luaL_error(L, "failed to get config, code %d", err);

    lua_createtable(L, 0, 3);
    size_t ssid_len = strnlen(cfg.sta.ssid, sizeof(cfg.sta.ssid));
    lua_pushlstring(L, cfg.sta.ssid, ssid_len);
    lua_setfield(L, -2, "ssid");

    size_t pwd_len = strnlen(cfg.sta.password, sizeof(cfg.sta.password));
    lua_pushlstring(L, cfg.sta.password, pwd_len);
    lua_setfield(L, -2, "pwd");

    if (cfg.sta.bssid_set)
    {
        char bssid_str[MAC_STR_SZ];
        macstr(bssid_str, cfg.sta.bssid);
        lua_pushstring(L, bssid_str);
        lua_setfield(L, -2, "bssid");
    }

    bool auto_conn;
    err = esp_wifi_get_auto_connect(&auto_conn);
    if (err != ESP_OK)
        return luaL_error(L, "failed to get auto-connect, code %d", err);

    lua_pushboolean(L, auto_conn);
    lua_setfield(L, -2, "auto");

    return 1;
}

static int wifi_sta_scan(lua_State *L)
{
    if (scan_cb_ref != LUA_NOREF)
        return luaL_error(L, "scan already in progress");

    luaL_checkanytable(L, 1);

    wifi_scan_config_t scan_cfg;
    memset(&scan_cfg, 0, sizeof(scan_cfg));

    // TODO: support ssid/bssid/channel/hidden features of wifi_scan_config_t

    luaL_checkanyfunction(L, 2);
    lua_settop(L, 2);
    scan_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
    if (err != ESP_OK)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, scan_cb_ref);
        scan_cb_ref = LUA_NOREF;
        return luaL_error(L, "failed to start scan, code %d", err);
    }
    else
        return 0;
}

static int wifi_ap_config(lua_State *L)
{
    luaL_checkanytable(L, 1);
    bool save = luaL_optbool(L, 2, false);

    wifi_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    lua_getfield(L, 1, "ssid");
    size_t len;
    const char *str = luaL_checklstring(L, -1, &len);
    if (len > sizeof(cfg.ap.ssid))
        len = sizeof(cfg.ap.ssid);
    strncpy(cfg.ap.ssid, str, len);
    cfg.ap.ssid_len = len;

    lua_getfield(L, 1, "pwd");
    str = luaL_optlstring(L, -1, "", &len);
    if (len > sizeof(cfg.ap.password))
        len = sizeof(cfg.ap.password);
    strncpy(cfg.ap.password, str, len);

    lua_getfield(L, 1, "auth");
    int authmode = luaL_optint(L, -1, WIFI_AUTH_WPA2_PSK);
    if (authmode < 0 || authmode >= WIFI_AUTH_MAX)
        return luaL_error(L, "unknown auth mode %d", authmode);
    cfg.ap.authmode = authmode;

    lua_getfield(L, 1, "channel");
    cfg.ap.channel = luaL_optint(L, -1, DEFAULT_AP_CHANNEL);

    lua_getfield(L, 1, "hidden");
    cfg.ap.ssid_hidden = luaL_optbool(L, -1, false);

    lua_getfield(L, 1, "max");
    cfg.ap.max_connection = luaL_optint(L, -1, DEFAULT_AP_MAXCONNS);

    lua_getfield(L, 1, "beacon");
    cfg.ap.beacon_interval = luaL_optint(L, -1, DEFAULT_AP_BEACON);

    SET_SAVE_MODE(save);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    return (err == ESP_OK) ? 0 : luaL_error(L, "failed to set wifi config, code %d", err);
}

static int wifi_init(lua_State *L)
{
    for (unsigned i = 0; i < ARRAY_LEN(event_cb); ++i) event_cb[i] = LUA_NOREF;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    return (err == ESP_OK) ? 0 : luaL_error(L, "failed to init wifi, code %d", err);
}

const LUA_REG_TYPE wifi_sta_map[] = {{LSTRKEY("config"), LFUNCVAL(wifi_sta_config)},
                                     {LSTRKEY("connect"), LFUNCVAL(wifi_sta_connect)},
                                     {LSTRKEY("disconnect"), LFUNCVAL(wifi_sta_disconnect)},
                                     {LSTRKEY("getconfig"), LFUNCVAL(wifi_sta_getconfig)},
                                     {LSTRKEY("scan"), LFUNCVAL(wifi_sta_scan)},

                                     {LNILKEY, LNILVAL}};

const LUA_REG_TYPE wifi_ap_map[] = {{LSTRKEY("config"), LFUNCVAL(wifi_ap_config)},

                                    {LSTRKEY("AUTH_OPEN"), LNUMVAL(WIFI_AUTH_OPEN)},
                                    {LSTRKEY("AUTH_WEP"), LNUMVAL(WIFI_AUTH_WEP)},
                                    {LSTRKEY("AUTH_WPA_PSK"), LNUMVAL(WIFI_AUTH_WPA_PSK)},
                                    {LSTRKEY("AUTH_WPA2_PSK"), LNUMVAL(WIFI_AUTH_WPA2_PSK)},
                                    {LSTRKEY("AUTH_WPA_WPA2_PSK"), LNUMVAL(WIFI_AUTH_WPA_WPA2_PSK)},

                                    {LNILKEY, LNILVAL}};

const LUA_REG_TYPE wifi_map[] = {{LSTRKEY("getmode"), LFUNCVAL(wifi_getmode)},
                                 {LSTRKEY("on"), LFUNCVAL(wifi_eventmon)},
                                 {LSTRKEY("setmode"), LFUNCVAL(wifi_setmode)},
                                 {LSTRKEY("start"), LFUNCVAL(wifi_start)},
                                 {LSTRKEY("stop"), LFUNCVAL(wifi_stop)},

                                 {LSTRKEY("sta"), LROVAL(wifi_sta_map)},
                                 {LSTRKEY("ap"), LROVAL(wifi_ap_map)},

                                 {LSTRKEY("NULLMODE"), LNUMVAL(WIFI_MODE_NULL)},
                                 {LSTRKEY("STATION"), LNUMVAL(WIFI_MODE_STA)},
                                 {LSTRKEY("SOFTAP"), LNUMVAL(WIFI_MODE_AP)},
                                 {LSTRKEY("STATIONAP"), LNUMVAL(WIFI_MODE_APSTA)},

                                 {LNILKEY, LNILVAL}};

LUALIB_API int luaopen_wifi(lua_State *L)
{
    // printf("wifi init\n");
    wifi_init(L);
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else
    luaL_register(L, LUA_WIFILIBNAME, wifi_map);
    return 1;
#endif
}
