// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#include "commands.h"
#include "worker.h"
#include "state.h"

#include <unistd.h> // getcwd

extern "C" {
    #include <atlaspacker/exporter.h>
    #include <atlaspacker/project.h>
    #include <nfd.h>
}

#include <stdio.h> // printf

const char* TEST_EXPORTER_PATH = "exporters/defold/exporter.lua";

static void SetModalDialog(AppState* state, int set)
{
    SCOPED_MUTEX(state->mutex);
    state->modal_dialog = set;
}

// ************************************************************************************
// Open Project File

struct ProjectFileOpenContext
{
    AppState* state;
    char*     path; // non zero if successful, 0 otherwise
};

// Called on worker thread
static int ProjectFileOpen_Process(void* _ctx)
{
    ProjectFileOpenContext* ctx = (ProjectFileOpenContext*)_ctx;
    AppState* state = ctx->state;

    SetModalDialog(state, 1);

    nfdchar_t* outpath = 0;
    nfdresult_t result = NFD_OpenDialog("ap", 0, &outpath);
    if (NFD_OKAY == result)
    {
        ctx->path = strdup(outpath);
        free(outpath);
        return RESULT_OK;
    }
    return RESULT_FAILED;
}

// Called on main thread
static void ProjectFileOpen_Finished(int result, void* _ctx)
{
    ProjectFileOpenContext* ctx = (ProjectFileOpenContext*)_ctx;
    AppState* state = ctx->state;

    if (RESULT_OK == result)
    {
        apProject* project = apLoadProjectFromPath(ctx->path);
        if (project)
        {
            if (!apExportUpdateOptions(project, TEST_EXPORTER_PATH))
            {

            }

            project->exporter_defaults = apExportGetDefaultOptions(project, TEST_EXPORTER_PATH);

            apDebugPrintProject(project);

            if (state->project)
                apDestroyProject(state->project);

            SCOPED_MUTEX(state->mutex);
            state->project = project;
            state->path    = ctx->path;
            ctx->path      = 0;

            CommandLoadImages(state->thread, state);
        }
    }

    free((void*)ctx->path);
    delete ctx;

    SetModalDialog(state, 0);
}

void CommandProjectFileOpen(HWorker worker, AppState* state)
{
    ProjectFileOpenContext* ctx = new ProjectFileOpenContext;
    ctx->state = state;
    ctx->path  = 0;
    WorkerPushJob(worker, ProjectFileOpen_Process, ProjectFileOpen_Finished, ctx);
}

// ************************************************************************************
// Project file save

static int ProjectFileSave_Process(void* _ctx)
{
    AppState* state = (AppState*)_ctx;
    SetModalDialog(state, 1);

    bool r = false;
    if (state->path == 0)
    {
        nfdchar_t* outpath = 0;
        nfdresult_t result = NFD_SaveDialog("ap", 0, &outpath);
        if (NFD_OKAY == result)
        {
            SCOPED_MUTEX(state->mutex);

            if (apSaveProject(outpath, state->project))
            {
                r = true;
                state->path = outpath; // it'll be deleted with the project

                printf("MAWE Wrote document: %s\n", state->path);
            }
            else
            {
                free(outpath);
            }
        }
    }
    else
    {
        r = apSaveProject(state->path, state->project);
    }

    return r ? RESULT_OK : RESULT_FAILED;
}

// On the main thread
static void ProjectFileSave_Finished(int result, void* _ctx)
{
    AppState* state = (AppState*)_ctx;
    if (RESULT_OK == result)
    {
        SCOPED_MUTEX(state->mutex);
        state->dirty = 0;

        // TODO: Update the window title!
        //UpdateWindowTitle(state, false);
    }
    SetModalDialog(state, 0);
}

void CommandProjectFileSave(HWorker worker, AppState* state)
{
    WorkerPushJob(worker, ProjectFileSave_Process, ProjectFileSave_Finished, state);
}

// ************************************************************************************
// Add Image File/Folder

struct ImageFileOpenContext
{
    AppState*   state;
    const char* extensions;
    bool        folder_only;
    bool        allow_multiple;
    // out
    const char** paths;
    int          num_paths;
};

// Called on worker thread
static int ImageFileOpen_Process(void* _ctx)
{
    ImageFileOpenContext* ctx = (ImageFileOpenContext*)_ctx;
    AppState* state = ctx->state;
    ctx->num_paths = 0;

    SetModalDialog(state, 1);

    nfdresult_t result;
    nfdchar_t* outpath = 0;

    if (ctx->folder_only)
    {
        result = NFD_PickFolder(0, &outpath);
        if (NFD_OKAY == result)
        {
            ctx->num_paths = 1;
        }
    }
    else
    {
        // TODO: USE NFD_OpenDialogMultiple to open multiple files!
        result = NFD_OpenDialog(ctx->extensions, 0, &outpath);
        if (NFD_OKAY == result)
        {
            ctx->num_paths = 1;
        }
    }

    ctx->paths = (const char**)malloc(sizeof(char**)*ctx->num_paths);
    ctx->paths[0] = (const char*)outpath;

    SetModalDialog(state, 0);

    SCOPED_MUTEX(state->mutex);
    state->modal_dialog = 0;

    return NFD_OKAY == result ? RESULT_OK : RESULT_FAILED;
}

// Called on main thread
static void ImageFileOpen_Finished(int result, void* _ctx)
{
    ImageFileOpenContext* ctx = (ImageFileOpenContext*)_ctx;
    AppState* state = ctx->state;

    if (RESULT_OK == result)
    {
        if (ctx->folder_only)
        {
            printf("Add folder: %s %d", ctx->paths[0], ctx->num_paths);
        }
        else
        {
            printf("Add file: %s %d", ctx->paths[0], ctx->num_paths);
        }

        ProjectAddSource(state, ctx->paths, ctx->num_paths);
    }

    for (int i = 0; i < ctx->num_paths; ++i)
        free((void*)ctx->paths[i]);
    free((void*)ctx->paths);
    delete ctx;

    // Trigger a load of the images on the other thread
    CommandLoadImages(state->thread, state);
}

void CommandAddImageFile(HWorker worker, AppState* state)
{
    ImageFileOpenContext* ctx = new ImageFileOpenContext;
    ctx->state       = state;
    ctx->folder_only = false;
    ctx->extensions  = "png,jpg";
    ctx->paths       = 0;
    ctx->num_paths   = 0;
    WorkerPushJob(worker, ImageFileOpen_Process, ImageFileOpen_Finished, ctx);
}

void CommandAddImageFolder(HWorker worker, AppState* state)
{
    ImageFileOpenContext* ctx = new ImageFileOpenContext;
    ctx->state       = state;
    ctx->folder_only = true;
    ctx->extensions  = 0;
    ctx->paths       = 0;
    ctx->num_paths   = 0;
    WorkerPushJob(worker, ImageFileOpen_Process, ImageFileOpen_Finished, ctx);
}

// ************************************************************************************
// Export the project file

static int ProjectFileExport_Process(void* _ctx)
{
    AppState* state = (AppState*)_ctx;

    SCOPED_MUTEX(state->mutex);

    if (!state->project)
    {
        return RESULT_FAILED;
    }

    // TODO: Each exporter could expose settings.
    // We could store those settings as json, and pass them on to this function when exporting
    const char* output_path = "/Users/mathiaswesterdahl/work/projects/users/mawe/extension-texturepacker/examples/ap/spineboy/ap_spineboy.tpinfo";
    apExportProject(state->project, TEST_EXPORTER_PATH, output_path);

    return RESULT_OK;
}

static void ProjectFileExport_Finished(int result, void* _ctx)
{
    AppState* state = (AppState*)_ctx;
    (void)result;
    SetModalDialog(state, 0);
}

void CommandProjectFileExport(HWorker worker, AppState* state)
{
    WorkerPushJob(state->thread, ProjectFileExport_Process, ProjectFileExport_Finished, (void*)state);
}
