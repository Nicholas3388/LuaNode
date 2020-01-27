// Module for interfacing with PWM

#include "c_types.h"
#include "lauxlib.h"
#include "lualib.h"
#include "modules.h"
#include "platform.h"

#define MAX_DUTY (8192 - 1)

// Lua: realfrequency = setup( id, frequency, duty )
static int lpwm_setup(lua_State* L)
{
    s32 freq;  // signed, to error check for negative values
    unsigned duty;
    unsigned id;
    unsigned channel;

    id = luaL_checkinteger(L, 1);
    if (id == 0)
        return luaL_error(L, "no pwm for D0");
    // MOD_CHECK_ID( pwm, id );
    freq = luaL_checkinteger(L, 2);
    if (freq <= 0)
        return luaL_error(L, "wrong arg range");
    duty = luaL_checkinteger(L, 3);
    if (duty > MAX_DUTY)
        // Negative values will turn out > 100, so will also fail.
        return luaL_error(L, "duty should lower than 8192");
    channel = luaL_checkinteger(L, 4);
    if (channel > 7)
        return luaL_error(L, "channel range from 0 to 7");
    freq = platform_pwm_setup(id, (u32)freq, duty, channel);
    if (freq == 0)
        return luaL_error(L, "too many pwms.");
    lua_pushinteger(L, freq);
    return 1;
}

// Lua: close( id )
static int lpwm_close(lua_State* L)
{
    unsigned id;

    id = luaL_checkinteger(L, 1);
    // MOD_CHECK_ID( pwm, id );
    platform_pwm_close(id);
    return 0;
}

// Lua: start( id )
static int lpwm_start(lua_State* L)
{
    // MOD_CHECK_ID( pwm, id );
    platform_pwm_start();
    return 0;
}

// Lua: stop( channel )
static int lpwm_stop(lua_State* L)
{
    unsigned id;

    id = luaL_checkinteger(L, 1);
    // MOD_CHECK_ID( pwm, id );
    platform_pwm_stop(id);
    return 0;
}

// Lua: realclock = setfreq( freq )
static int lpwm_setfreq(lua_State* L)
{
    unsigned id;
    s32 freq;  // signed to error-check for negative values

    // id = luaL_checkinteger( L, 1 );
    // MOD_CHECK_ID( pwm, id );
    freq = luaL_checkinteger(L, 1);
    if (freq <= 0)
        return luaL_error(L, "wrong arg range");
    freq = platform_pwm_set_clock(0, (u32)freq);
    lua_pushinteger(L, freq);
    return 1;
}

// Lua: clock = getfreq( )
static int lpwm_getfreq(lua_State* L)
{
    unsigned id;
    u32 clk;

    // id = luaL_checkinteger( L, 1 );
    // MOD_CHECK_ID( pwm, id );
    clk = platform_pwm_get_clock();
    lua_pushinteger(L, clk);
    return 1;
}

// Lua: realduty = setduty( channel, duty )
static int lpwm_setduty(lua_State* L)
{
    unsigned id;
    s32 duty;  // signed to error-check for negative values

    id = luaL_checkinteger(L, 1);
    // MOD_CHECK_ID( pwm, id );
    duty = luaL_checkinteger(L, 2);
    if (duty > MAX_DUTY)
        return luaL_error(L, "max duty is 8191");
    duty = platform_pwm_set_duty(id, (u32)duty);
    lua_pushinteger(L, duty);
    return 1;
}

// Lua: duty = getduty( id )
static int lpwm_getduty(lua_State* L)
{
    unsigned id;
    u32 duty;

    id = luaL_checkinteger(L, 1);
    // MOD_CHECK_ID( pwm, id );
    duty = platform_pwm_get_duty(id);
    lua_pushinteger(L, duty);
    return 1;
}

// Module function map
const LUA_REG_TYPE pwm_map[] = {{LSTRKEY("setup"), LFUNCVAL(lpwm_setup)},
                                {LSTRKEY("close"), LFUNCVAL(lpwm_close)},
                                {LSTRKEY("start"), LFUNCVAL(lpwm_start)},
                                {LSTRKEY("stop"), LFUNCVAL(lpwm_stop)},
                                {LSTRKEY("setfreq"), LFUNCVAL(lpwm_setfreq)},
                                {LSTRKEY("getfreq"), LFUNCVAL(lpwm_getfreq)},
                                {LSTRKEY("setduty"), LFUNCVAL(lpwm_setduty)},
                                {LSTRKEY("getduty"), LFUNCVAL(lpwm_getduty)},
                                {LNILKEY, LNILVAL}};

LUALIB_API int luaopen_pwm(lua_State* L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else
    luaL_register(L, LUA_PWMLIBNAME, pwm_map);
    return 1;
#endif
}