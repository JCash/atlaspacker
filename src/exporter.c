
#include <atlaspacker/exporter.h>

#include "lua/lualib.h"
#include "lua/lauxlib.h"

static void PushPos(lua_State* L, apPos pos)
{
    lua_newtable(L);
    lua_pushinteger(L, pos.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, pos.y);
    lua_setfield(L, -2, "y");
}

static void PushPosf(lua_State* L, apPosf pos)
{
    lua_newtable(L);
    lua_pushnumber(L, pos.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, pos.y);
    lua_setfield(L, -2, "y");
}

static void PushSize(lua_State* L, apSize size)
{
    lua_newtable(L);
    lua_pushinteger(L, size.width);
    lua_setfield(L, -2, "width");
    lua_pushinteger(L, size.height);
    lua_setfield(L, -2, "height");
}

static void PushRect(lua_State* L, apRect rect)
{
    lua_newtable(L);
    lua_pushinteger(L, rect.pos.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, rect.pos.y);
    lua_setfield(L, -2, "y");
    lua_pushinteger(L, rect.size.width);
    lua_setfield(L, -2, "width");
    lua_pushinteger(L, rect.size.height);
    lua_setfield(L, -2, "height");
}

static int PushSettings(lua_State* L, apProject* project)
{
    lua_newtable(L);
    // lua_pushstring(L, "value");
    // lua_setfield(L, -2, "setting");
    return 0;
}

static int PushImage(lua_State* L, apImage* image)
{
    lua_newtable(L);

    // source
    lua_newtable(L);
        lua_pushstring(L, image->path);
        lua_setfield(L, -2, "path");

        lua_pushinteger(L, image->width);
        lua_setfield(L, -2, "width");

        lua_pushinteger(L, image->height);
        lua_setfield(L, -2, "height");

        lua_pushinteger(L, image->channels);
        lua_setfield(L, -2, "channels");

    lua_setfield(L, -2, "source");

    PushRect(L, image->placement);
    lua_setfield(L, -2, "placement");

    lua_pushinteger(L, image->rotation);
    lua_setfield(L, -2, "rotation");

    lua_pushinteger(L, image->page);
    lua_setfield(L, -2, "page");

    // apPosf*         vertices;
    // int             num_vertices;

    return 0;
}

static int PushPage(lua_State* L, apPage* page)
{
    lua_newtable(L);

    lua_pushinteger(L, page->index);
    lua_setfield(L, -2, "index");

    PushSize(L, page->dimensions);
    lua_setfield(L, -2, "dimensions");

    lua_pushstring(L, "images");
    lua_newtable(L);

    int index = 1;
    apImage* image = apPageGetFirstImage(page);
    while (image)
    {
        lua_pushinteger(L, index++);
        PushImage(L, image);
        lua_settable(L, -3);

        image = image->next;
    }

    lua_settable(L, -3); // "images"

    return 0;
}

static int PushOutput(lua_State* L, apProject* project)
{
    lua_newtable(L);

    // lua_pushstring(L, "HELLO");
    // lua_setfield(L, -2, "path");

    lua_pushstring(L, "pages");
    lua_newtable(L);

    for (int i = 0; i < apGetNumPages(project->context); ++i)
    {
        apPage* page = apGetPage(project->context, i);
        lua_pushinteger(L, i + 1);
        PushPage(L, page);
        lua_settable(L, -3);
    }

    lua_settable(L, -3); // "pages"
    return 0;
}

static int ValidateProject(apProject* project)
{
    if (!project->context)
        return 0;

    return 1;
}

static void printStack(lua_State* L)
{
    int top = lua_gettop(L);
    int bottom = 1;
    lua_getglobal(L, "tostring");
    for(int i = top; i >= bottom; i--)
    {
        lua_pushvalue(L, -1);
        lua_pushvalue(L, i);
        lua_pcall(L, 1, 1, 0);
        const char *str = lua_tostring(L, -1);
        if (str) {
            printf("%2d: %s\n", i, str);
        }else{
            printf("%2d: %s\n", i, luaL_typename(L, i));
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

int apExportProject(apProject* project, const char* exporter_path, const char* output_path)
{
    if (!ValidateProject(project))
        return 0;

    lua_State* L = luaL_newstate(); // We prefer to start with a clean slate
    luaL_openlibs(L);

    int result = luaL_dofile(L, exporter_path);
    if (LUA_ERRFILE == result)
    {
        printf("File not found: '%s'\n", exporter_path);
        lua_close(L);
        return 0;
    }
    else if (result != 0)
    {
        printf("File '%s' failed to load: %d '%s'\n", exporter_path, result, lua_tostring(L, -1));
        lua_close(L);
        return 0;
    }

    int top = lua_gettop(L);

    lua_getglobal(L, "export");
    printf("top: exporter: %d\n", lua_gettop(L));
    PushSettings(L, project);

    printf("top: settings: %d\n", lua_gettop(L));

    PushOutput(L, project);
    printf("top: output: %d\n", lua_gettop(L));

    printStack(L);

    if (lua_pcall(L, 2, 1, 0) != 0) {
        printf("Error running function 'export': %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 0;
    }

    int newtop = lua_gettop(L);
    if (newtop != top)
    {
        int retval = (int)lua_tointeger(L, -1);
        lua_pop(L, newtop - top);
        printf("Return value: %d == %s\n", retval, retval == 0 ? "ok":"fail");
    }
    else
    {
        printf("No return value (== OK)\n");
    }

    printf("Exported using '%s' to '%s'\n", exporter_path, output_path);
    lua_close(L);
    return 1;
}
