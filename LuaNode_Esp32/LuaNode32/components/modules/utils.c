// libutils.a
#include <stdlib.h>

#include "base64.h"
#include "c_string.h"
#include "datetime.h"
#include "lauxlib.h"
#include "modules.h"
//#include "md5.h"
#include "lualib.h"

/**
 * translate Hex to String
 * pbDest: the memory to stored encoding string
 * pbSrc: the input hex
 * nLen: the length of hex
 */
void HexToStr(unsigned char *pbDest, unsigned char *pbSrc, int nLen)
{
    char ddl, ddh;
    int i;

    for (i = 0; i < nLen; i++)
    {
        ddh = 48 + pbSrc[i] / 16;
        ddl = 48 + pbSrc[i] % 16;
        if (ddh > 57)
            ddh = ddh + 7;
        if (ddl > 57)
            ddl = ddl + 7;
        pbDest[i * 2] = ddh;
        pbDest[i * 2 + 1] = ddl;
    }

    pbDest[nLen * 2] = '\0';
}

// return the lenght of encoding string
static int base64_encode0(lua_State *L)
{
    int argc = lua_gettop(L);
    const char *str = lua_tostring(L, 1);
    char *buff = malloc(strlen(str) * 2);
    memset(buff, 0, strlen(str) * 2);
    int num = base64_encode(str, strlen(str), buff, 1);
    lua_pushstring(L, buff);
    return 1;
}

// return the length of decoding string
static int base64_decode0(lua_State *L)
{
    int argc = lua_gettop(L);
    const char *str = lua_tostring(L, 1);
    char *buff = malloc(strlen(str) * 2);
    memset(buff, 0, strlen(str) * 2);
    int num = base64_decode(str, buff);
    lua_pushstring(L, buff);
    return 1;
}

static int leapyear0(lua_State *L)
{
    int year = lua_tointeger(L, 1);
    int isLeap = isLeapyear(year);
    lua_pushboolean(L, isLeap);
    return 1;
}

// md5 encode, return the encoding string
/*static int md5_encode( lua_State* L ) {
        unsigned char *str = (unsigned char *)lua_tostring(L, 1);	//
string to be encoded char digest[16]; char ecrypt[32]; memset(digest, 0, 16);
        memset(ecrypt, 0, 32);
        MD5_CTX md5;
    MD5Init(&md5);
        MD5Update(&md5, str, strlen(str));
    MD5Final(&md5, digest);
        HexToStr(ecrypt, digest, strlen(digest));
        lua_pushstring(L, ecrypt);
        return 1;
}*/

// Module function map
const LUA_REG_TYPE utils_map[] = {{LSTRKEY("base64_encode"), LFUNCVAL(base64_encode0)},
                                  {LSTRKEY("base64_decode"), LFUNCVAL(base64_decode0)},
                                  //{ LSTRKEY( "md5_encode" ), LFUNCVAL( md5_encode ) },
                                  {LSTRKEY("leapyear"), LFUNCVAL(leapyear0)},
                                  {LNILKEY, LNILVAL}};

LUALIB_API int luaopen_utils(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else
    luaL_register(L, LUA_UTILSLIBNAME, utils_map);
    return 1;
#endif
}
