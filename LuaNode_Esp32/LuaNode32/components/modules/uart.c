// Module for interfacing with serial

#include "driver/uart.h"

#include "c_string.h"
#include "c_types.h"
#include "lauxlib.h"
#include "lrodefs.h"
#include "lrotable.h"
#include "lualib.h"
#include "modules.h"
#include "platform.h"

#define DEFAULT_TXD (4)
#define DEFAULT_RXD (5)
#define DEFAULT_RTS (18)
#define DEFAULT_CTS (19)

static lua_State *gL = NULL;
static int uart_receive_rf = LUA_NOREF;
bool run_input = true;
bool uart_on_data_cb(const char *buf, size_t len)
{
    if (!buf || len == 0)
        return false;
    if (uart_receive_rf == LUA_NOREF)
        return false;
    if (!gL)
        return false;
    lua_rawgeti(gL, LUA_REGISTRYINDEX, uart_receive_rf);
    lua_pushlstring(gL, buf, len);
    lua_call(gL, 1, 0);
    return !run_input;
}

uint16_t need_len = 0;
int16_t end_char = -1;
// Lua: uart.on("method", [number/char], function, [run_input])
static int uart_on(lua_State *L)
{
    size_t sl, el;
    int32_t run = 1;
    uint8_t stack = 1;
    const char *method = luaL_checklstring(L, stack, &sl);
    stack++;
    if (method == NULL)
        return luaL_error(L, "wrong arg type");

    if (lua_type(L, stack) == LUA_TNUMBER)
    {
        need_len = (uint16_t)luaL_checkinteger(L, stack);
        stack++;
        end_char = -1;
        if (need_len > 255)
        {
            need_len = 255;
            return luaL_error(L, "wrong arg range");
        }
    }
    else if (lua_isstring(L, stack))
    {
        const char *end = luaL_checklstring(L, stack, &el);
        stack++;
        if (el != 1)
        {
            return luaL_error(L, "wrong arg range");
        }
        end_char = (int16_t)end[0];
        need_len = 0;
    }

    // luaL_checkanyfunction(L, stack);
    if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION)
    {
        if (lua_isnumber(L, stack + 1))
        {
            run = lua_tointeger(L, stack + 1);
        }
        lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
    }
    else
    {
        lua_pushnil(L);
    }
    if (sl == 4 && c_strcmp(method, "data") == 0)
    {
        run_input = true;
        if (uart_receive_rf != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, uart_receive_rf);
            uart_receive_rf = LUA_NOREF;
        }
        if (!lua_isnil(L, -1))
        {
            uart_receive_rf = luaL_ref(L, LUA_REGISTRYINDEX);
            gL = L;
            if (run == 0)
                run_input = false;
        }
        else
        {
            lua_pop(L, 1);
        }
    }
    else
    {
        lua_pop(L, 1);
        return luaL_error(L, "method not supported");
    }
    return 0;
}

bool uart0_echo = true;
// Lua: actualbaud = setup( id, baud, databits, parity, stopbits, flow_ctl, txd,
// rxd, rts, cts )
static int uart_setup(lua_State *L)
{
    unsigned id, databits, parity, stopbits, flow, txd = DEFAULT_TXD, rxd = DEFAULT_RXD, rts = DEFAULT_CTS,
                                                   cts = DEFAULT_RTS;
    u32 baud, res;

    id = luaL_checkinteger(L, 1);
    MOD_CHECK_ID(uart, id);
    baud = luaL_checkinteger(L, 2);
    databits = luaL_checkinteger(L, 3);
    parity = luaL_checkinteger(L, 4);
    stopbits = luaL_checkinteger(L, 5);
    flow = luaL_checkinteger(L, 6);
    int params_num = lua_gettop(L);
    if (params_num > 6)
    {
        int i;
        for (i = 7; i < params_num; i++)
        {
            switch (i)
            {
                case 7:
                    txd = luaL_checkinteger(L, 7);
                    break;
                case 8:
                    rxd = luaL_checkinteger(L, 8);
                    break;
                case 9:
                    rts = luaL_checkinteger(L, 9);
                    break;
                case 10:
                    cts = luaL_checkinteger(L, 10);
                    break;
            }
        }
    }

    res = platform_uart_setup(id, baud, databits, parity, stopbits, flow, txd, rxd, rts, cts);
    lua_pushinteger(L, res);
    return 1;
}

#if 0
// Lua: alt( set )
static int uart_alt( lua_State* L )
{
  unsigned set;
  
  set = luaL_checkinteger( L, 1 );

  //platform_uart_alt( set );
  return 0;
}
#endif

// Lua: write( id, string1, [string2], ..., [stringn] )
static int uart_write(lua_State *L)
{
    int id;
    const char *buf;
    size_t len, i;
    int total = lua_gettop(L), s;

    id = luaL_checkinteger(L, 1);
    // MOD_CHECK_ID( uart, id );
    for (s = 2; s <= total; s++)
    {
        if (lua_type(L, s) == LUA_TNUMBER)
        {
            len = lua_tointeger(L, s);
            if (len > 255)
                return luaL_error(L, "invalid number");
            platform_uart_send(id, &len, 1);
        }
        else
        {
            luaL_checktype(L, s, LUA_TSTRING);
            buf = lua_tolstring(L, s, &len);
            printf("data to write: %s\n", buf);
            platform_uart_send(id, buf, len);
        }
    }
    return 0;
}

// Lua: uart.uninstall( uart_num )
static int uart_uninstall(lua_State *L)
{
    int num;
    num = luaL_checkinteger(L, 1);
    if (num < 0 || num > 2)
    {
        return luaL_error(L, "invalid argument");
    }
    platform_uart_uninstall(num);
    return 0;
}

// Module function map
const LUA_REG_TYPE uart_map[] = {{LSTRKEY("setup"), LFUNCVAL(uart_setup)},
                                 {LSTRKEY("write"), LFUNCVAL(uart_write)},
                                 {LSTRKEY("on"), LFUNCVAL(uart_on)},
                                 //{ LSTRKEY( "alt" ),   LFUNCVAL( uart_alt ) },
                                 {LSTRKEY("uninstall"), LFUNCVAL(uart_uninstall)},
                                 {LSTRKEY("UART_1"), LNUMVAL(UART_NUM_1)},
                                 {LSTRKEY("UART_2"), LNUMVAL(UART_NUM_2)},
                                 {LSTRKEY("DATA_5_BITS"), LNUMVAL(UART_DATA_5_BITS)},
                                 {LSTRKEY("DATA_6_BITS"), LNUMVAL(UART_DATA_6_BITS)},
                                 {LSTRKEY("DATA_7_BITS"), LNUMVAL(UART_DATA_7_BITS)},
                                 {LSTRKEY("DATA_8_BITS"), LNUMVAL(UART_DATA_8_BITS)},
                                 {LSTRKEY("FLOWCTRL_DISABLE"), LNUMVAL(UART_HW_FLOWCTRL_DISABLE)},
                                 {LSTRKEY("FLOWCTRL_RTS"), LNUMVAL(UART_HW_FLOWCTRL_RTS)},
                                 {LSTRKEY("FLOWCTRL_CTS"), LNUMVAL(UART_HW_FLOWCTRL_CTS)},
                                 {LSTRKEY("FLOWCTRL_CTS_RTS"), LNUMVAL(UART_HW_FLOWCTRL_CTS_RTS)},
                                 {LSTRKEY("STOPBITS_1"), LNUMVAL(UART_STOP_BITS_1)},
                                 {LSTRKEY("STOPBITS_1_5"), LNUMVAL(UART_STOP_BITS_1_5)},
                                 {LSTRKEY("STOPBITS_2"), LNUMVAL(UART_STOP_BITS_2)},
                                 {LSTRKEY("PARITY_NONE"), LNUMVAL(UART_PARITY_DISABLE)},
                                 {LSTRKEY("PARITY_EVEN"), LNUMVAL(UART_PARITY_EVEN)},
                                 {LSTRKEY("PARITY_ODD"), LNUMVAL(UART_PARITY_ODD)},
                                 {LNILKEY, LNILVAL}};

LUALIB_API int luaopen_uart(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else
    luaL_register(L, LUA_UARTLIBNAME, uart_map);
    return 1;
#endif
}
