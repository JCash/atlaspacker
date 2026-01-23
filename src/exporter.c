
#include <atlaspacker/exporter.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "lua/lualib.h"
#include "lua/lauxlib.h"

static int PPrintTable(lua_State* L, int index, int indent);

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
    assert(top == lua_gettop(L));
}

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

// static void PushJson(lua_State* L, cJSON* obj)
// {
//     if (cJSON_IsString(obj))
//     {
//         lua_pushstring(L, cJSON_GetStringValue(obj));
//     }
//     else if (cJSON_IsNumber(obj))
//     {
//         lua_pushnumber(L, cJSON_GetNumberValue(obj));
//     }
//     else if (cJSON_IsBool(obj))
//     {
//         lua_pushnumber(L, cJSON_IsTrue(obj));
//     }
//     else if (cJSON_IsArray(obj) || cJSON_IsObject(obj))
//     {
//         lua_newtable(L);

//         int index = cJSON_IsArray(obj) ? 1 : 0;

//         cJSON* item;
//         cJSON_ArrayForEach(item, obj)
//         {
//             PushJson(L, item);

//             if (index)
//             {
//                 lua_rawseti(L, -2, index++);
//             }
//             else
//             {
//                 lua_setfield(L, -2, item->string); // the name string
//             }
//         }
//     }
// }

static apOptionValue* FindOptionByName(apOptionValue* options, const char* name)
{
    while (options)
    {
        if (options->name && strcmp(options->name, name) == 0)
            return options;
        options = options->next;
    }
    return 0;
}

static apOptionValue* FindOptionsByName(apProject* project, const char* name, OptionValueType expected_type)
{
    apOptionValue* option = FindOptionByName(project->exporter_options, name);
    if (!option)
    {
        option = FindOptionByName(project->exporter_defaults, name);
    }
    if (option && expected_type != option->type)
    {
        printf("Expected property %s to be type %d, but was %d\n", name, expected_type, option->type);
        return 0;
    }
    return option;
}

static void PushOptions(lua_State* L, apProject* project)
{
    lua_newtable(L);

    apOptionValue* option = project->exporter_options;
    while (option)
    {
        lua_newtable(L);

        //
        switch (option->type)
        {
        case OVT_BOOL:   lua_pushboolean(L, option->value.number != 0); break;
        case OVT_NUMBER: lua_pushnumber(L, option->value.number); break;
        case OVT_STRING: lua_pushstring(L, option->value.string); break;
        }
        lua_setfield(L, -2, "value");

        lua_pushstring(L, option->name);
        lua_setfield(L, -2, "name");

        if (option->edit)
        {
            lua_pushstring(L, option->edit);
            lua_setfield(L, -2, "edit");
        }

        if (option->desc)
        {
            lua_pushstring(L, option->desc);
            lua_setfield(L, -2, "desc");
        }

        if (option->display)
        {
            lua_pushstring(L, option->display);
            lua_setfield(L, -2, "display");
        }

        // Add it to the option table
        lua_setfield(L, -2, option->name);

        option = option->next;
    }
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

static int PushAtlasData(lua_State* L, apProject* project)
{
    lua_newtable(L);

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

////////////////////////////////////

static const char* GetTableString(lua_State* L, int index, const char* name)
{
    int top = lua_gettop(L);
// printf("  GetTableString: name '%s'\n", name);
// printStack(L);

    lua_pushvalue(L, index);    // -1: table
    lua_pushstring(L, name);    // -2: name, -1: table
    lua_gettable(L, -2);        // -2: value, -1: table

    lua_pushvalue(L, -1);       // -3: value, -2: value (to avoid modifications by tostring!), -1: table

    const char* string = 0;
    if (lua_isstring(L, -2))
    {
        string = lua_tostring(L, -1);
        string = strdup(string);
    }

    lua_pop(L, 3);
    assert(lua_gettop(L) == top);
    return string;
}

static int ParseValue(lua_State* L, int index, const char* name, OptionValueType* type, const char** string, double* number)
{
    int top = lua_gettop(L);

    lua_pushvalue(L, index);    // -1: table
    lua_pushstring(L, name);    // -2: name, -1: table
    lua_gettable(L, -2);        // -2: value, -1: table
    // to avoid modifications by tostring
    lua_pushvalue(L, -1);       // -3: value, -2: value, -1: table

    int ok = 0;
    if (!lua_isnil(L, -1))
    {
        ok = 1;
        if (lua_isnumber(L, -1))
        {
            *number = lua_tonumber(L, -1);
            *type = OVT_NUMBER;
        }
        else if (lua_isboolean(L, -1))
        {
            *number = lua_toboolean(L, -1);
            *type = OVT_BOOL;
        }
        else if (lua_isstring(L, -1))
        {
            *string = strdup(lua_tostring(L, -1));
            *type = OVT_STRING;
        }
        else
        {
            fprintf(stderr, "Option '%s' has invalid value type: '%s'\n", name, lua_typename(L, -1));
            ok = 0;
        }
    }

    lua_pop(L, 3);
    assert(lua_gettop(L) == top);
    return ok;
}

static apOptionValue* CloneOptionValue(const apOptionValue* src)
{
    apOptionValue* option = (apOptionValue*)malloc(sizeof(apOptionValue));
    memset(option, 0, sizeof(*option));

    option->type = src->type;
    option->name = src->name ? strdup(src->name) : 0;
    option->edit = src->edit ? strdup(src->edit) : 0;
    option->desc = src->desc ? strdup(src->desc) : 0;
    option->display = src->display ? strdup(src->display) : 0;

    if (src->type == OVT_STRING)
        option->value.string = src->value.string ? strdup(src->value.string) : 0;
    else
        option->value.number = src->value.number;

    return option;
}

static void AppendOption(apOptionValue** head, apOptionValue* option)
{
    if (!option)
        return;

    if (!*head)
    {
        *head = option;
        return;
    }

    apOptionValue* tail = *head;
    while (tail->next)
        tail = tail->next;
    tail->next = option;
}

static apOptionValue* ParseOption(lua_State* L, int index)
{
    const char* name = GetTableString(L, index, "name");
    const char* edit = GetTableString(L, index, "edit");
    const char* desc = GetTableString(L, index, "desc");
    const char* display = GetTableString(L, index, "display");

    if (!name)
        return 0;

    OptionValueType type = 0;
    const char* string = 0;
    double number = 0;

    int result = ParseValue(L, index, "value", &type, &string, &number);
    if (!result)
        return 0;

    apOptionValue* option = (apOptionValue*)malloc(sizeof(apOptionValue));
    memset(option, 0, sizeof(*option));

    option->name = name;
    option->edit = edit;
    option->desc = desc;
    option->display = display;

    option->type = type;
    if (type == OVT_STRING)
    {
        option->value.string = string;
    }
    else
    {
        option->value.number = number;
    }

    return option;
}

static apOptionValue* ParseOptions(lua_State* L, int index)
{
    if (!lua_istable(L, index))
    {
        return 0;
    }

    lua_pushvalue(L, index);        // -1: table
    lua_pushnil(L);                 // -1: nil, -2: table

    apOptionValue* first = 0;
    apOptionValue* last = 0;
    while (lua_next(L, -2) != 0)    // -1: value, -2: key, -3: table
    {
        // Copy to avoid lua_tostring to modify the original key
        lua_pushvalue(L, -2);       // -1: key, -2: value, -3: key, -4: table

        if (lua_istable(L, -2))
        {
            apOptionValue* option = ParseOption(L, -2);
            if (!first)
                first = option;
            if (last)
                last->next = option;
            last = option;
        }

        lua_pop(L, 2);              // -1: key, -2: table
    }
    lua_pop(L, 1); // pop the table
    return first;
}

static int ValidateProject(apProject* project)
{
    if (!project->context)
        return 0;

    return 1;
}


// *********************************************************************************************
// Lua Helpers


static const char* PushValueAsString(lua_State* L, int index)
{
    lua_pushvalue(L, index);
    // [-1] value
    lua_getglobal(L, "tostring");
    // [-2] value
    // [-1] tostring()
    lua_insert(L, -2);
    // [-2] tostring()
    // [-1] value
    lua_call(L, 1, 1);
    // [-1] result
    const char* result = lua_tostring(L, -1);
    if (result == 0x0)
    {
        lua_pop(L, 1);
    }
    return result;
}

static void PrintIndent(int indent)
{
    for (int i = 0; i < indent; ++i)
        printf("  ");
}

static int PPrintTable(lua_State* L, int index, int indent)
{
    const void* table_data = (const void*)lua_topointer(L, index);

    lua_pushvalue(L, index);
    lua_pushnil(L);
    // [-2] table
    // [-1] key

    if(lua_next(L, -2) == 0)
    {
        // [-1] table
        PrintIndent(indent);
        printf("{ } --[[%p]]", table_data);
        lua_pop(L, 1);
        return 0;
    }

    // [-3] table
    // [-2] key
    // [-1] value
    PrintIndent(indent);
    printf("{ --[[%p]]", table_data);

    indent++;

    int is_first = 1;
    do
    {
        PrintIndent(indent);
        printf("%s\n", is_first ? "" : ",");
        int value_type = lua_type(L, -1);

        const char* key_string = PushValueAsString(L, -2);
        if (key_string == 0x0)
        {
            return luaL_error(L, "tostring must return a string to print");
        }
        // [-4] table
        // [-3] key
        // [-2] value
        // [-1] key name

        PrintIndent(indent);
        printf("%s = ", key_string);
        lua_pop(L, 1);
        // [-3] table
        // [-2] key
        // [-1] value

        if (value_type == LUA_TTABLE)
        {
            PPrintTable(L, -1, indent);
        }
        else if (value_type == LUA_TSTRING)
        {
            printf("\"%s\"", lua_tostring(L, -1));
        }
        else
        {
            const char* value_string = PushValueAsString(L, -1);
            if (value_string == 0x0)
            {
                return luaL_error(L, "tostring must return a string to print");
            }
            // [-4] table
            // [-3] key
            // [-2] value
            // [-1] value name

            printf("%s", value_string);
            lua_pop(L, 1);
            // [-3] table
            // [-2] key
            // [-1] value
        }

        lua_pop(L, 1);
        // [-2] table
        // [-1] key
        is_first = 0;
    } while (lua_next(L, -2) != 0);

    // [-1] table
    indent--;

    printf("\n");
    PrintIndent(indent);
    printf("}\n");

    lua_pop(L, 1);
    return 0;
}

static int PPrint(lua_State* L)
{
    int n = lua_gettop(L);
    for (int s = 1; s <= n; ++s)
    {
        if (lua_type(L, s) == LUA_TTABLE)
        {
            if (s == 1)
            {
                printf("\n");
            }
            PPrintTable(L, s, 0);
            printf("%s", (n > s) ? ",\n" : "");
        }
        else
        {
            const char* value_str = PushValueAsString(L, s);
            if (value_str == 0x0)
            {
                return luaL_error(L, "tostring must return a string to print");
            }
            printf("%s%s", value_str, (n > s) ? ",\n" : "");
            lua_pop(L, 1);
        }
    }
    return 0;
}

static int Print(lua_State* L)
{
    int n = lua_gettop(L);
    lua_getglobal(L, "tostring");
    char buffer[2048];
    buffer[0] = 0;
    for (int i = 1; i <= n; ++i)
    {
        const char *s;
        lua_pushvalue(L, -1);
        lua_pushvalue(L, i);
        lua_call(L, 1, 1);
        s = lua_tostring(L, -1);
        if (s == 0x0)
            return luaL_error(L, "tostring must return a string to xprint");
        if (i > 1)
            strlcat(buffer, "\t", sizeof(buffer));
        strlcat(buffer, s, sizeof(buffer));
        lua_pop(L, 1);
    }
    printf("%s\n", buffer);
    lua_pop(L, 1);
    assert(n == lua_gettop(L));
    return 0;
}

// From ldblib.c (getthread)
static lua_State* GetLuaThread(lua_State *L, int *arg)
{
    if (lua_isthread(L, 1)) {
        *arg = 1;
        return lua_tothread(L, 1);
    }
    else {
        *arg = 0;
        return L;
    }
}

// From https://zeux.io/2010/11/07/lua-callstack-with-c-debugger/
// and also
// https://github.com/defold/defold/blob/dev/engine/lua/src/lua/ldblib.c#L321
static void GetLuaTraceback(lua_State* L, const char* infostring, void (*cbk)(lua_State* L, lua_Debug* entry, void* ctx), void* ctx)
{
    const int LEVELS1 = 12;  // size of the first part of the stack
    const int LEVELS2 = 10;  // size of the second part of the stack

    int firstpart = 1;
    int arg = 0;
    lua_State* L1 = GetLuaThread(L, &arg);

    lua_Debug entry;
    int level = 0;

    if (lua_isnumber(L, arg+2)) {
        level = (int)lua_tointeger(L, arg+2);
        lua_pop(L, 1); // problematic?!
    }
    else {
        level = (L == L1) ? 1 : 0;  // level 0 may be this own function
    }

    if (lua_gettop(L) != arg && !lua_isstring(L, arg+1)) return;  // message is not a string

    while (lua_getstack(L1, level++, &entry))
    {
        if (level > LEVELS1 && firstpart) {
            // no more than `LEVELS2' more levels?
            if (!lua_getstack(L1, level+LEVELS2, &entry)) {
                level--;  // keep going
            } else {
                lua_pushliteral(L, "\n\t...");  // too many levels
                while (lua_getstack(L1, level+LEVELS2, &entry))  // find last levels
                    level++;
            }
            firstpart = 0;
            continue;
        }
        int status = lua_getinfo(L1, infostring, &entry);
        if (!status)
            continue;

        cbk(L1, &entry, ctx);
    }
}

static void WriteLuaTracebackEntry(lua_Debug* entry)
{
    if (*entry->namewhat != '\0')
    {
        fprintf(stderr, "  %s:%d: in function %s\n", entry->short_src, entry->currentline, entry->name);
    }
    else if (*entry->what == 'm')
    {
        fprintf(stderr, "  %s:%d: in main chunk\n", entry->short_src, entry->currentline);
    }
    else if (*entry->what == 'C' || *entry->what == 't')
    {
        fprintf(stderr, "  %s:%d: ?\n", entry->short_src, entry->currentline);
    }
    else
    {
        fprintf(stderr, "  %s:%d: in function <%s:%d>\n", entry->short_src, entry->currentline, entry->short_src, entry->linedefined);
    }
}

typedef struct LuaCallstackCtx
{
    int first;
} LuaCallstackCtx;

static void GetLuaStackTraceCbk(lua_State* L, lua_Debug* entry, void* _ctx)
{
    LuaCallstackCtx* ctx = (LuaCallstackCtx*)_ctx;

    if (ctx->first)
    {
        fprintf(stderr, "stack traceback:\n");
        ctx->first = 0;
    }

    WriteLuaTracebackEntry(entry);
}

static int BacktraceErrorHandler(lua_State *m_state)
{
    if (!lua_isstring(m_state, 1))
        return 1;

    lua_createtable(m_state, 0, 2);
    lua_pushvalue(m_state, 1);
    lua_setfield(m_state, -2, "error");

    char traceback[1024];
    LuaCallstackCtx ctx;
    ctx.first = 1;
    GetLuaTraceback(m_state, "Sln", GetLuaStackTraceCbk, &ctx);
    lua_pushstring(m_state, traceback);
    lua_setfield(m_state, -2, "traceback");

    return 1;
}

static int PCall(lua_State* L, int nargs, int nresult)
{
    lua_pushcfunction(L, BacktraceErrorHandler);
    int err_index = lua_gettop(L) - nargs - 1;
    lua_insert(L, err_index);
    int result = lua_pcall(L, nargs, nresult, err_index);
    lua_remove(L, err_index);
    if (result == LUA_ERRMEM) {
        lua_pop(L, 1);  // Pop BacktraceErrorHandler since it will not be called on OOM
        fprintf(stderr, "Lua memory allocation error.\n");
    } else if (result != 0) {
        // extract the individual fields for printing and passing
        lua_getfield(L, -1, "error");
        lua_getfield(L, -2, "traceback");
        // if handling error that happened during the error handling, print it and clean up and exit
        fprintf(stderr, "In error handler: %s%s\n", lua_tostring(L, -2), lua_tostring(L, -1));
        lua_pop(L, 3);
        return result;
    }
    return result;
}

// *********************************************************************************************

static int LuaLoadFile(lua_State* L, const char* path)
{
    int result = luaL_dofile(L, path);
    if (LUA_ERRFILE == result)
    {
        printf("File not found: '%s'\n", path);
        lua_close(L);
        return 0;
    }
    else if (result != 0)
    {
        printf("File '%s' failed to load: %d '%s'\n", path, result, lua_tostring(L, -1));
        lua_close(L);
        return 0;
    }
    return 1;
}

static apOptionValue* LuaGetDefaults(lua_State* L, apProject* project)
{
    const char* fnname = "get_defaults";
    int top = lua_gettop(L);

    lua_getglobal(L, fnname);

    if (PCall(L, 0, 1) != 0)
    {
        printf("Error running function '%s': %s\n", fnname, lua_tostring(L, -1));
        return 0;
    }

    int newtop = lua_gettop(L);
    if (newtop != (top+1))
    {
        printf("Wrong number of return values from `%s()`\n", fnname);
        printStack(L);
        return 0;
    }

    apOptionValue* options = ParseOptions(L, -1);
    lua_pop(L, 1);

    return options;
}


// Let the exporter modify the settings
static apOptionValue* LuaUpdateOptions(lua_State* L, apProject* project)
{
    const char* fnname = "update_options";
    int top = lua_gettop(L);

    lua_getglobal(L, fnname);

    PushOptions(L, project);

    if (PCall(L, 1, 1) != 0)
    {
        printf("Error running function '%s': %s\n", fnname, lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0;
    }

    int newtop = lua_gettop(L);
    if (newtop != (top+1))
    {
        printf("Wrong number of return values from `%s()`\n", fnname);
        printStack(L);
        return 0;
    }

    apOptionValue* options = ParseOptions(L, -1);
    lua_pop(L, 1);

    return options;
}

static int LuaExport(lua_State* L, apProject* project, const char* output_path)
{
    const char* fnname = "export";

    int top = lua_gettop(L);

    lua_getglobal(L, fnname);

    PushOptions(L, project);
    PushAtlasData(L, project);
    lua_pushstring(L, output_path);

    // *****************************************************************************
    // Export

    if (PCall(L, 3, 1) != 0)
    {
        printf("Error running function '%s': %s\n", fnname, lua_tostring(L, -1));
        return 0;
    }

    // *****************************************************************************

    int newtop = lua_gettop(L);
    if (newtop != (top+1))
    {
        int retval = (int)lua_tointeger(L, -1);
        lua_pop(L, newtop - top);
        printf("Wrong number of return values from `%s()`\n", fnname);
        return retval != 0; // 0 == fail
    }

    return 1;
}

static lua_State* CreateLuaState()
{
    lua_State* L = luaL_newstate(); // We prefer to start with a clean slate
    luaL_openlibs(L);

    lua_register(L, "print", Print);
    lua_register(L, "pprint", PPrint);

    return L;
}

int apExportProject(apProject* project, const char* exporter_path, const char* project_path)
{
    if (!ValidateProject(project))
        return 0;

    lua_State* L = CreateLuaState(); // We prefer to start with a clean slate

    // Load the exporter.lua file
    int result = LuaLoadFile(L, exporter_path);

    apOptionValue* data_file_option = FindOptionsByName(project, "data_file", OVT_STRING);
    const char* output_path = data_file_option ? data_file_option->value.string : 0;

    if (result)
    {
        if (output_path)
        {
            // Call the exporter
            result = LuaExport(L, project, output_path);
        }
    }

    lua_close(L);

    if (result)
    {
        printf("Exported using '%s' to '%s'\n", exporter_path, output_path ? output_path : "null");
    }

    return result;
}

apOptionValue* apExportGetDefaultOptions(apProject* project, const char* exporter_path)
{
    lua_State* L = CreateLuaState(); // We prefer to start with a clean slate

    int result = LuaLoadFile(L, exporter_path);
    if (!result)
        return 0;

    apOptionValue* options = LuaGetDefaults(L, project);
    lua_close(L);
    return options;
}

int apExportUpdateOptions(apProject* project, const char* exporter_path)
{
    lua_State* L = CreateLuaState(); // We prefer to start with a clean slate

    int result = LuaLoadFile(L, exporter_path);
    if (!result)
        return 0;

    apOptionValue* original = project->exporter_options;
    apOptionValue* options = LuaUpdateOptions(L, project);
    lua_close(L);

    if (options)
    {
        if (original)
        {
            for (apOptionValue* opt = original; opt; opt = opt->next)
            {
                if (!FindOptionByName(options, opt->name))
                {
                    apOptionValue* clone = CloneOptionValue(opt);
                    AppendOption(&options, clone);
                }
            }
        }
        apDestroyOptions(original);
        project->exporter_options = options;
    }

    return result != 0 && options != 0 ? 1 : 0;
}
