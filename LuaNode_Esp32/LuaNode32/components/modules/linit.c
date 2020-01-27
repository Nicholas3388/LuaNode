#define linit_c
#define LUA_LIB
#define LUAC_CROSS_FILE

#include "c_string.h"
#include "esp_misc.h"
#include "lauxlib.h"
#include "lrotable.h"
#include "lua.h"
#include "luaconf.h"
#include "lualib.h"
#include "modules.h"
#include "user_modules.h"

extern const LUA_REG_TYPE nvs_map[];
extern const LUA_REG_TYPE mqtt_map[];
extern const LUA_REG_TYPE thread_map[];
extern const LUA_REG_TYPE gpio_map[];
extern const LUA_REG_TYPE pwm_map[];
extern const LUA_REG_TYPE node_map[];
extern const LUA_REG_TYPE wifi_map[];
extern const LUA_REG_TYPE net_map[];
extern const LUA_REG_TYPE file_map[];
extern const LUA_REG_TYPE tmr_map[];
extern const LUA_REG_TYPE i2c_map[];
extern const LUA_REG_TYPE uart_map[];
extern const LUA_REG_TYPE utils_map[];
extern const LUA_REG_TYPE strlib[];
extern const LUA_REG_TYPE tab_funcs[];
extern const LUA_REG_TYPE co_funcs[];
extern const LUA_REG_TYPE math_map[];

const luaL_Reg lua_libs[] = {
    {"base", luaopen_base},
    {"package", luaopen_package},
    {"table", luaopen_table},
    {"string", luaopen_string},
#ifdef USE_GPIO_MODULE
    {LUA_GPIOLIBNAME, luaopen_gpio},
#endif
#ifdef USE_PWM_MODULE
    {LUA_PWMLIBNAME, luaopen_pwm},
#endif
#ifdef USE_NODE_MODULE
    {LUA_NODELIBNAME, luaopen_node},
#endif
#ifdef USE_WIFI_MODULE
    {LUA_WIFILIBNAME, luaopen_wifi},
#endif
#ifdef USE_FILE_MODULE
    {LUA_FILELIBNAME, luaopen_file},
#endif
#ifdef USE_TMR_MODULE
    {LUA_TMRLIBNAME, luaopen_tmr},
#endif
#ifdef USE_I2C_MODULE
    {LUA_I2CLIBNAME, luaopen_i2c},
#endif
#ifdef USE_UART_MODULE
    {LUA_UARTLIBNAME, luaopen_uart},
#endif
#ifdef USE_UTILS_MODULE
    {LUA_UTILSLIBNAME, luaopen_utils},
#endif
#ifdef USE_LPEG_MODULE
    {LUA_LPEGLIBNAME, luaopen_lpeg},
#endif
#ifdef USE_NET_MODULE
    {LUA_NETLIBNAME, luaopen_net},
#endif
#ifdef USE_THREAD_MODULE
    {LUA_THREADLIBNAME, luaopen_thread},
#endif
#ifdef USE_MQTT_MODULE
    {LUA_MQTTLIBNAME, luaopen_mqtt},
#endif
#ifdef USE_NVS_MODULE
    {LUA_NVSLIBNAME, luaopen_nvs},
#endif
    {NULL, NULL},
};

const luaR_table lua_rotable[] = {{LUA_TABLIBNAME, tab_funcs},
                                  {LUA_STRLIBNAME, strlib},
//{LUA_COLIBNAME, co_funcs},
//{LUA_MATHLIBNAME, math_map},
#ifdef USE_GPIO_MODULE
                                  {LUA_GPIOLIBNAME, gpio_map},
#endif
#ifdef USE_PWM_MODULE
                                  {LUA_PWMLIBNAME, pwm_map},
#endif
#ifdef USE_NODE_MODULE
                                  {LUA_NODELIBNAME, node_map},
#endif
#ifdef USE_WIFI_MODULE
                                  {LUA_WIFILIBNAME, wifi_map},
#endif
#ifdef USE_FILE_MODULE
                                  {LUA_FILELIBNAME, file_map},
#endif
#ifdef USE_TMR_MODULE
                                  {LUA_TMRLIBNAME, tmr_map},
#endif
#ifdef USE_I2C_MODULE
                                  {LUA_I2CLIBNAME, i2c_map},
#endif
#ifdef USE_UART_MODULE
                                  {LUA_UARTLIBNAME, uart_map},
#endif
#ifdef USE_UTILS_MODULE
                                  {LUA_UTILSLIBNAME, utils_map},
#endif
#ifdef USE_NET_MODULE
                                  {LUA_NETLIBNAME, net_map},
#endif
#ifdef USE_THREAD_MODULE
                                  {LUA_THREADLIBNAME, thread_map},
#endif
#ifdef USE_MQTT_MODULE
                                  {LUA_MQTTLIBNAME, mqtt_map},
#endif
#ifdef USE_NVS_MODULE
                                  {LUA_NVSLIBNAME, nvs_map},
#endif
                                  {NULL, NULL}};

void luaL_openlibs(lua_State *L)
{
    const luaL_Reg *lib = lua_libs;
    for (; lib->name; lib++)
    {
        os_printf("load lib: %s\n", lib->name);
        if (lib->func)
        {
            lua_pushcfunction(L, lib->func);
            lua_pushstring(L, lib->name);
            lua_call(L, 1, 0);
        }
    }
    os_printf("other libs:\n");

    // luaopen_pwm(L);
}
