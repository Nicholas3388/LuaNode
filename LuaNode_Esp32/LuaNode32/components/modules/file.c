// Module for interfacing with file system

#include "c_string.h"
#include "c_types.h"
#include "flash_fs.h"
#include "lauxlib.h"
#include "lualib.h"
#include "modules.h"
#include "platform.h"
#include "vfs.h"

static volatile int file_fd = FS_OPEN_OK - 1;

// Lua: open(filename, mode)
static int file_open(lua_State *L)
{
    size_t len;
    if (file_fd)
    {
        vfs_close(file_fd);
        file_fd = 0;
    }

    const char *fname = luaL_checklstring(L, 1, &len);
    const char *basename = vfs_basename(fname);
    luaL_argcheck(L, strlen(basename) <= 32 && strlen(fname) == len, 1, "filename invalid");

    const char *mode = luaL_optstring(L, 2, "r");

    file_fd = vfs_open(fname, mode);

    if (!file_fd)
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushboolean(L, 1);
    }
    return 1;
}

// Lua: close()
static int file_close(lua_State *L)
{
    if (file_fd)
    {
        vfs_close(file_fd);
        file_fd = 0;
        NODE_DBG("file close successfully\n");
    }
    return 0;
}

// Lua: format()
static int file_format(lua_State *L)
{
    file_close(L);
    if (!vfs_format())
    {
        NODE_ERR("\n*** ERROR ***: unable to format. FS might be compromised.\n");
        NODE_ERR("It is advised to re-flash the NodeMCU image.\n");
        luaL_error(L, "Failed to format file system");
    }
    else
    {
        NODE_ERR("format done.\n");
    }
    return 0;
}

// Lua: list()
static int file_list(lua_State *L)
{
    vfs_dir *dir;
    vfs_item *item;

    if ((dir = vfs_opendir("")))
    {
        lua_newtable(L);
        while ((item = vfs_readdir(dir)))
        {
            lua_pushinteger(L, vfs_item_size(item));
            lua_setfield(L, -2, vfs_item_name(item));
            vfs_closeitem(item);
        }
        vfs_closedir(dir);
        return 1;
    }
    return 0;
}

static int file_seek(lua_State *L)
{
    static const int mode[] = {VFS_SEEK_SET, VFS_SEEK_CUR, VFS_SEEK_END};
    static const char *const modenames[] = {"set", "cur", "end", NULL};
    if (!file_fd)
        return luaL_error(L, "open a file first");
    int op = luaL_checkoption(L, 1, "cur", modenames);
    long offset = luaL_optlong(L, 2, 0);
    op = vfs_lseek(file_fd, offset, mode[op]);
    if (op < 0)
        lua_pushnil(L); /* error */
    else
        lua_pushinteger(L, vfs_tell(file_fd));
    return 1;
}

// Lua: remove(filename)
static int file_remove(lua_State *L)
{
    size_t len;
    const char *fname = luaL_checklstring(L, 1, &len);
    const char *basename = vfs_basename(fname);
    luaL_argcheck(L, strlen(basename) <= 32 && strlen(fname) == len, 1, "filename invalid");
    file_close(L);
    vfs_remove((char *)fname);
    return 0;
}

// Lua: flush()
static int file_flush(lua_State *L)
{
    if (!file_fd)
        return luaL_error(L, "open a file first");
    if (vfs_flush(file_fd) == 0)
        lua_pushboolean(L, 1);
    else
        lua_pushnil(L);
    return 1;
}
#if 0
// Lua: check()
static int file_check( lua_State* L )
{
  file_close(L);
  lua_pushinteger(L, fs_check());
  return 1;
}
#endif

// Lua: rename("oldname", "newname")
static int file_rename(lua_State *L)
{
    size_t len;
    if (file_fd)
    {
        vfs_close(file_fd);
        file_fd = 0;
    }

    const char *oldname = luaL_checklstring(L, 1, &len);
    const char *basename = vfs_basename(oldname);
    luaL_argcheck(L, strlen(basename) <= 32 && strlen(oldname) == len, 1, "filename invalid");

    const char *newname = luaL_checklstring(L, 2, &len);
    basename = vfs_basename(newname);
    luaL_argcheck(L, strlen(basename) <= 32 && strlen(newname) == len, 2, "filename invalid");

    if (0 <= vfs_rename(oldname, newname))
    {
        lua_pushboolean(L, 1);
    }
    else
    {
        lua_pushboolean(L, 0);
    }
    return 1;
}

// Lua: fsinfo()
static int file_fsinfo(lua_State *L)
{
    u32_t total, used;
    if (vfs_fsinfo("", &total, &used))
    {
        return luaL_error(L, "file system failed");
    }
    NODE_DBG("total: %d, used:%d\n", total, used);
    if (total > 0x7FFFFFFF || used > 0x7FFFFFFF || used > total)
    {
        return luaL_error(L, "file system error");
    }
    lua_pushinteger(L, total - used);
    lua_pushinteger(L, used);
    lua_pushinteger(L, total);
    return 3;
}

// g_read()
static int file_g_read(lua_State *L, int n, int16_t end_char)
{
    if (n <= 0 || n > LUAL_BUFFERSIZE)
        n = LUAL_BUFFERSIZE;
    if (end_char < 0 || end_char > 255)
        end_char = EOF;

    luaL_Buffer b;
    if (!file_fd)
        return luaL_error(L, "open a file first");

    luaL_buffinit(L, &b);
    char *p = luaL_prepbuffer(&b);
    int i;

    n = vfs_read(file_fd, p, n);
    for (i = 0; i < n; ++i)
        if (p[i] == end_char)
        {
            ++i;
            break;
        }

    if (i == 0)
    {
        luaL_pushresult(&b);            /* close buffer */
        return (lua_objlen(L, -1) > 0); /* check whether read something */
    }

    vfs_lseek(file_fd, -(n - i), VFS_SEEK_CUR);
    luaL_addsize(&b, i);
    luaL_pushresult(&b); /* close buffer */
    return 1;            /* read at least an `eol' */
}

// Lua: read()
// file.read() will read all byte in file
// file.read(10) will read 10 byte from file, or EOF is reached.
// file.read('q') will read until 'q' or EOF is reached.
static int file_read(lua_State *L)
{
    unsigned need_len = LUAL_BUFFERSIZE;
    int16_t end_char = EOF;
    size_t el;
    if (lua_type(L, 1) == LUA_TNUMBER)
    {
        need_len = (unsigned)luaL_checkinteger(L, 1);
        if (need_len > LUAL_BUFFERSIZE)
        {
            need_len = LUAL_BUFFERSIZE;
        }
    }
    else if (lua_isstring(L, 1))
    {
        const char *end = luaL_checklstring(L, 1, &el);
        if (el != 1)
        {
            return luaL_error(L, "wrong arg range");
        }
        end_char = (int16_t)end[0];
    }

    return file_g_read(L, need_len, end_char);
}

// Lua: readline()
static int file_readline(lua_State *L) { return file_g_read(L, LUAL_BUFFERSIZE, '\n'); }

// Lua: write("string")
static int file_write(lua_State *L)
{
    if (!file_fd)
        return luaL_error(L, "open a file first");
    size_t l, rl;
    const char *s = luaL_checklstring(L, 1, &l);
    rl = vfs_write(file_fd, s, l);
    if (rl == l)
        lua_pushboolean(L, 1);
    else
        lua_pushnil(L);
    return 1;
}

// Lua: writeline("string")
static int file_writeline(lua_State *L)
{
    if (!file_fd)
        return luaL_error(L, "open a file first");
    size_t l, rl;
    const char *s = luaL_checklstring(L, 1, &l);
    rl = vfs_write(file_fd, s, l);
    if (rl == l)
    {
        rl = vfs_write(file_fd, "\n", 1);
        if (rl == 1)
            lua_pushboolean(L, 1);
        else
            lua_pushnil(L);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

// Module function map
const LUA_REG_TYPE file_map[] = {{LSTRKEY("list"), LFUNCVAL(file_list)},
                                 {LSTRKEY("open"), LFUNCVAL(file_open)},
                                 {LSTRKEY("close"), LFUNCVAL(file_close)},
                                 {LSTRKEY("write"), LFUNCVAL(file_write)},
                                 {LSTRKEY("writeline"), LFUNCVAL(file_writeline)},
                                 {LSTRKEY("read"), LFUNCVAL(file_read)},
                                 {LSTRKEY("readline"), LFUNCVAL(file_readline)},
                                 {LSTRKEY("format"), LFUNCVAL(file_format)},
#if defined(BUILD_SPIFFS) && !defined(BUILD_WOFS)
                                 {LSTRKEY("remove"), LFUNCVAL(file_remove)},
                                 {LSTRKEY("seek"), LFUNCVAL(file_seek)},
                                 {LSTRKEY("flush"), LFUNCVAL(file_flush)},
                                 //{ LSTRKEY( "check" ),     LFUNCVAL( file_check ) },
                                 {LSTRKEY("rename"), LFUNCVAL(file_rename)},
                                 {LSTRKEY("fsinfo"), LFUNCVAL(file_fsinfo)},
#endif
                                 {LNILKEY, LNILVAL}};

LUALIB_API int luaopen_file(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else
    luaL_register(L, LUA_FILELIBNAME, file_map);
    return 1;
#endif
}
