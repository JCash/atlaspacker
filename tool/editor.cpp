#include <stdint.h>

#include "commands.h"
#include "gui.h"
#include "state.h"
#include "thread.h"
#include "worker.h"

extern "C" {
    #include <atlaspacker/file.h>
    #include <atlaspacker/exporter.h>
    #include <atlaspacker/project.h>

    #include <nfd.h>
}

#include <imgui.h>

#define SOKOL_APP_IMPL
#define SOKOL_IMPL
#define SOKOL_IMGUI_IMPL
#define SOKOL_GLCORE
#define SOKOL_NO_ENTRY
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#include <sokol_imgui.h>

// #define NOC_FILE_DIALOG_IMPLEMENTATION
// #if defined(__APPLE__)
//     #define NOC_FILE_DIALOG_OSX
// #elif defined(_MSC_VER)
//     #define NOC_FILE_DIALOG_WIN
// #else
//     #define NOC_FILE_DIALOG_GTK
// #endif
// #include <noc_file_dialog.h>

#if defined(__APPLE__)
    #define KEY_CODE_OPEN           SAPP_KEYCODE_O
    #define KEY_CODE_SAVE           SAPP_KEYCODE_S
    #define KEY_CODE_EXPORT         SAPP_KEYCODE_E
    #define KEY_CODE_QUIT           SAPP_KEYCODE_Q
    #define KEY_CODE_MODIFIERS      SAPP_MODIFIER_SUPER
#elif defined(_WIN32)
    #define KEY_CODE_OPEN           SAPP_KEYCODE_O
    #define KEY_CODE_SAVE           SAPP_KEYCODE_S
    #define KEY_CODE_EXPORT         SAPP_KEYCODE_E
    #define KEY_CODE_MODIFIERS      SAPP_MODIFIER_CTRL
#else
    #define KEY_CODE_OPEN           SAPP_KEYCODE_O
    #define KEY_CODE_SAVE           SAPP_KEYCODE_S
    #define KEY_CODE_EXPORT         SAPP_KEYCODE_E
    #define KEY_CODE_QUIT           SAPP_KEYCODE_Q
    #define KEY_CODE_MODIFIERS      SAPP_MODIFIER_SUPER
#endif

static struct {
    sg_pass_action pass_action;
} SokolActionState;

static const char* GetWindowTitle(const char* path, bool dirty, char* buffer, uint32_t buffer_size)
{
    const char* asterisk = dirty ? "*" : "";
    snprintf(buffer, buffer_size, "AtlasPacker %s: %s%s", VERSION, path?path:"untitled", asterisk);
    return buffer;
}

static void UpdateWindowTitle(AppState* state, bool dirty)
{
    char title[1024];
    GetWindowTitle(state->path, dirty, title, sizeof(title));
    sapp_set_window_title(title);
}

static bool Quit(AppState* state)
{
    printf("TODO: Check if the project is dirty!\n");
    bool should_quit = true;
    if (!should_quit)
        return false;

    // TODO: Add a UnloadProject function
    {
        SCOPED_MUTEX(state->mutex);

        DeletePageTextures(state);
        DestroyImages(state);

        if (state->project)
            apDestroyProject(state->project);
        state->project = 0;
    }

    WorkerDestroy(state->thread);
    WorkerDestroy(state->uithread);

    MutexDestroy(state->mutex);

    return true;
}

static void OnSokolInit(void* user_data)
{
    AppState* state = (AppState*)user_data;

    sg_desc desc = {
        .environment = sglue_environment(),
        .logger.func = slog_func,
    };
    sg_setup(&desc);

    simgui_desc_t imdesc = { 0 };
    simgui_setup(&imdesc);

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // initial clear color
    SokolActionState.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.5f, 1.0f, 1.0 } }
    };

    // Create dummy texture for atlas pages
    state->zoom = 1.0f;

    CreateDefaultTexture(state);
}


static void OnSokolFrame(void* user_data)
{
    AppState* state = (AppState*)user_data;

    // Flush any finished jobs
    // NOTE: The state->uithread isn't updated here, due to the macOS specifics between
    // the NFD dialogs and the Sokol frame
    // Instead, it is updated in the mouse up
    WorkerUpdate(state->thread);

    int width = sapp_width();
    int height = sapp_height();

    simgui_frame_desc_t frame_desc = {
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    };
    simgui_new_frame(&frame_desc);

    DrawEditor(state, width, height);

    /*=== UI CODE ENDS HERE ===*/
    sg_pass pass = { .action = SokolActionState.pass_action, .swapchain = sglue_swapchain() };
    sg_begin_pass(&pass);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void OnSokolCleanup(void* user_data) {
    simgui_shutdown();
    sg_shutdown();
}

// ********************************************************************************************

static void OnSokolEvent(const sapp_event* ev, void* user_data) {
    AppState* state = (AppState*)user_data;

    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN)
    {
        if (ev->key_code == KEY_CODE_OPEN && (ev->modifiers & KEY_CODE_MODIFIERS)==KEY_CODE_MODIFIERS)
        {
            SCOPED_MUTEX(state->mutex);
            if (!state->modal_dialog)
                CommandProjectFileOpen(state->uithread, state);
        }
        else if (ev->key_code == KEY_CODE_SAVE && (ev->modifiers & KEY_CODE_MODIFIERS)==KEY_CODE_MODIFIERS)
        {
            SCOPED_MUTEX(state->mutex);
            if (!state->modal_dialog)
                CommandProjectFileSave(state->uithread, state);
        }
        else if (ev->key_code == KEY_CODE_EXPORT && (ev->modifiers & KEY_CODE_MODIFIERS)==KEY_CODE_MODIFIERS)
        {
            SCOPED_MUTEX(state->mutex);
            if (!state->modal_dialog)
                CommandProjectFileExport(state->uithread, state);
        }
        else if (ev->key_code == KEY_CODE_QUIT && (ev->modifiers & KEY_CODE_MODIFIERS)==KEY_CODE_MODIFIERS)
        {
            sapp_request_quit();
        }
        else
        {
            simgui_handle_event(ev);
        }
    }
    else if (ev->type == SAPP_EVENTTYPE_QUIT_REQUESTED)
    {
        if (!Quit(state))
            sapp_cancel_quit();
    }
    else if (ev->type == SAPP_EVENTTYPE_FILES_DROPPED)
    {

        // the mouse position where the drop happened
        // float x = ev->mouse_x;
        // float y = ev->mouse_y;

        // // get the number of files and their paths like this:
        // const int num_dropped_files = sapp_get_num_dropped_files();
        // for (int i = 0; i < num_dropped_files; i++) {
        //     const char* path = sapp_get_dropped_file_path(i);
        //     ...
        // }
    }
    else if (ev->type == SAPP_EVENTTYPE_MOUSE_UP)
    {
        simgui_handle_event(ev);

        // Due to the macOS specifics of the file dialog open
        // it is currently processed here
        WorkerUpdate(state->uithread);
    }
    else
    {
        simgui_handle_event(ev);
    }
}

int main(int argc, char* argv[])
{
    AppState state;
    memset(&state, 0, sizeof(state));

    state.mutex = MutexCreate();

    state.thread = WorkerCreate();
    state.uithread = WorkerCreateNoThread();

    if (argc > 1)
    {
        state.path = argv[argc-1];
        state.project = apLoadProjectFromPath(state.path);

        int dirty_fileset = state.project != 0;

        if (!state.project)
        {
            fprintf(stderr, "Failed to read prooject from %s\n", state.path);
            state.project = apLoadProjectFromMemory("untitled", 0);
        }

        if (dirty_fileset)
            CommandLoadImages(state.thread, &state);
    }
    else
    {
        state.path      = 0;
        state.project   = apLoadProjectFromMemory("untitled", 0);
    }

    char title[1024];
    GetWindowTitle(state.path, false, title, sizeof(title));

    sapp_desc desc = {
        .width = 1280,
        .height = 1024,
        .user_data = (void*)&state,
        .init_userdata_cb = OnSokolInit,
        .frame_userdata_cb = OnSokolFrame,
        .cleanup_userdata_cb = OnSokolCleanup,
        .event_userdata_cb = OnSokolEvent,
        .logger.func = slog_func,
        .enable_dragndrop = true,
        .max_dropped_files = 8 *1024,
        .max_dropped_file_path_length = 8192,
        .window_title = title,
    };

    sapp_run(&desc);
    // we'll never get here. See Quit() instead
    return 0;
}
