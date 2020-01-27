// Module for interfacing with the I2C interface
#include "driver/i2c.h"

#include <string.h>

#include "lauxlib.h"
#include "lualib.h"
#include "modules.h"
#include "platform.h"

// Lua: speed = i2c.setup( mode, port, scl, sda )
static int i2c_setup(lua_State *L)
{
    unsigned mode = luaL_checkinteger(L, 1);
    if (mode != I2C_MODE_MASTER && mode != I2C_MODE_SLAVE)
    {
        return luaL_error(L, "invalid mode");
    }
    unsigned port = luaL_checkinteger(L, 2);
    if (port != I2C_NUM_0 && port != I2C_NUM_1)
    {
        return luaL_error(L, "valid port is 0 or 1");
    }
    unsigned scl = luaL_checkinteger(L, 3);
    unsigned sda = luaL_checkinteger(L, 4);

    if (mode == I2C_MODE_SLAVE && lua_gettop(L) < 5)
    {
        return luaL_error(L, "lack of argument: addr");
    }

    if (lua_gettop(L) > 4)
    {  // SLAVE mode
        unsigned addr = luaL_checkinteger(L, 5);
        if (addr < 0 || addr > 127)
        {
            return luaL_error(L, "invalid addr (addr < 127)");
        }
        platform_i2c_setup(mode, port, scl, sda, addr);
        printf("I2C %d set as SLAVE\n", port);
    }
    else
    {
        platform_i2c_setup(mode, port, scl, sda, 0);
        printf("I2C %d set as MASTER\n", port);
    }

    return 1;
}

// Lua: i2c.start( id )
static int i2c_start(lua_State *L)
{
    unsigned id = luaL_checkinteger(L, 1);

    // MOD_CHECK_ID( i2c, id );
    platform_i2c_send_start(id);
    return 0;
}

// Lua: i2c.stop( id )
static int i2c_stop(lua_State *L)
{
    unsigned id = luaL_checkinteger(L, 1);

    // MOD_CHECK_ID( i2c, id );
    platform_i2c_send_stop(id);
    return 0;
}

// Lua: status = i2c.address( id, address, direction )
static int i2c_address(lua_State *L)
{
    /* unsigned id = luaL_checkinteger( L, 1 );
     int address = luaL_checkinteger( L, 2 );
     int direction = luaL_checkinteger( L, 3 );

     //MOD_CHECK_ID( i2c, id );
     if ( address < 0 || address > 127 )
       return luaL_error( L, "wrong arg range" );
     lua_pushboolean( L, platform_i2c_send_address( id, (u16)address, direction
     ) );*/
    return 1;
}

// Lua: wrote = i2c.write( mode, port, addr, data1, [data2], ..., [datan] )
// data can be either a string, a table or an 8-bit number
static int i2c_write(lua_State *L)
{
    unsigned mode = luaL_checkinteger(L, 1);
    unsigned port = luaL_checkinteger(L, 2);
    unsigned addr = luaL_checkinteger(L, 3);
    const char *pdata;
    size_t datalen, i;
    int numdata = 0;
    u32 wrote = 0;
    unsigned argn;
    unsigned char buf[256];
    memset(buf, 0, 256);

    // MOD_CHECK_ID( i2c, id );
    if (lua_gettop(L) < 4)
        return luaL_error(L, "wrong arg type");
    for (argn = 4; argn <= lua_gettop(L); argn++)
    {
        // lua_isnumber() would silently convert a string of digits to an
        // integer whereas here strings are handled separately.
        if (lua_type(L, argn) == LUA_TNUMBER)
        {
            numdata = (int)luaL_checkinteger(L, argn);
            if (numdata < 0 || numdata > 255)
                return luaL_error(L, "wrong arg range");
            // if( platform_i2c_send_byte( mode, port, numdata ) == 0 )
            //  break;
            buf[wrote] = numdata;
            wrote++;
        }
        else if (lua_istable(L, argn))
        {
            datalen = lua_objlen(L, argn);
            for (i = 0; i < datalen; i++)
            {
                lua_rawgeti(L, argn, i + 1);
                numdata = (int)luaL_checkinteger(L, -1);
                lua_pop(L, 1);
                if (numdata < 0 || numdata > 255)
                    return luaL_error(L, "wrong arg range");
                buf[wrote] = numdata;
                wrote++;
                // if( platform_i2c_send_byte( mode, port, numdata ) == 0 )
                //  break;
            }
        }
        else
        {
            pdata = luaL_checklstring(L, argn, &datalen);
            for (i = 0; i < datalen; i++)
            {
                buf[wrote] = pdata[i];
                wrote++;
            }
            // if( platform_i2c_send_byte( mode, port, pdata[ i ] ) == 0 )
            //  break;
        }
    }

    int ret = platform_i2c_send_byte(mode, port, addr, buf, wrote);
    if (mode == I2C_MODE_MASTER && ret != ESP_OK)
    {
        printf("Master write slave error, IO not connected....\n");
    }
    lua_pushinteger(L, wrote);
    return 1;
}

// Lua: read = i2c.read( mode, port, addr, len )
static int i2c_read(lua_State *L)
{
    unsigned mode = luaL_checkinteger(L, 1);
    unsigned port = luaL_checkinteger(L, 2);
    if (port != I2C_NUM_0 && port != I2C_NUM_1)
    {
        return luaL_error(L, "valid port is 0 or 1");
    }
    unsigned addr = luaL_checkinteger(L, 3);
    unsigned len = luaL_checkinteger(L, 4);
    if (len < 0)
    {
        return luaL_error(L, "read len must greater than 0");
    }
    else if (len == 0)
    {
        luaL_pushresult(0);
        return 1;
    }
    luaL_Buffer b;

    // MOD_CHECK_ID( i2c, id );
    luaL_buffinit(L, &b);
    uint8_t data_rw[1024] = {0};
    int r_size = platform_i2c_recv_byte(mode, port, addr, data_rw, len);
    if (mode == I2C_MODE_MASTER && r_size != ESP_OK)
    {
        printf("Master read slave error....\n");
    }
    luaL_addlstring(&b, data_rw, strlen(data_rw));
    luaL_pushresult(&b);
    return 1;
}

// Lua: i2c.uninstall( i2c_num )
static int i2c_uninstall(lua_State *L)
{
    unsigned num = luaL_checkinteger(L, 1);
    if (num != I2C_NUM_0 && num != I2C_NUM_1)
    {
        return luaL_error(L, "invalid argument");
    }
    platform_i2c_uninstall(num);
    return 0;
}

// Module function map
const LUA_REG_TYPE i2c_map[] = {
    {LSTRKEY("setup"), LFUNCVAL(i2c_setup)},         {LSTRKEY("start"), LFUNCVAL(i2c_start)},
    {LSTRKEY("stop"), LFUNCVAL(i2c_stop)},           {LSTRKEY("address"), LFUNCVAL(i2c_address)},
    {LSTRKEY("write"), LFUNCVAL(i2c_write)},         {LSTRKEY("read"), LFUNCVAL(i2c_read)},
    {LSTRKEY("uninstall"), LFUNCVAL(i2c_uninstall)}, {LSTRKEY("MASTER"), LNUMVAL(I2C_MODE_MASTER)},
    {LSTRKEY("SLAVE"), LNUMVAL(I2C_MODE_SLAVE)},     {LSTRKEY("I2C_0"), LNUMVAL(I2C_NUM_0)},
    {LSTRKEY("I2C_1"), LNUMVAL(I2C_NUM_1)},          {LNILKEY, LNILVAL}};

LUALIB_API int luaopen_i2c(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else
    luaL_register(L, LUA_I2CLIBNAME, i2c_map);
    return 1;
#endif
}
