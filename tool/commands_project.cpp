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
    // TODO: Add a ProjectLoad command
    ProjectFileOpenContext* ctx = (ProjectFileOpenContext*)_ctx;
    AppState* state = ctx->state;

    if (RESULT_OK == result)
    {
        apProject* project = apLoadProjectFromPath(ctx->path);
        if (project)
        {
            if (!apExportUpdateOptions(project, state->exporter_path))
            {

            }

            project->exporter_defaults = apExportGetDefaultOptions(project, state->exporter_path);

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

    SetModalDialog(state, 0);

    if (NFD_OKAY == result)
    {
        ctx->paths = (const char**)malloc(sizeof(char**)*ctx->num_paths);
        ctx->paths[0] = (const char*)outpath;
    }

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

    if (!state->exporter_path)
    {
        return RESULT_FAILED;
    }

    apExportProject(state->project, state->exporter_path, state->path);

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
    WorkerPushJob(worker, ProjectFileExport_Process, ProjectFileExport_Finished, (void*)state);
}

// ************************************************************************************
// Open a folder

struct FolderOpenContext
{
    AppState*   state;
    const char* extensions;
    bool        folder_only;

    // out
    const char** paths;
    int          num_paths;

    void*       callback_ctx;
    void        (*callback)(void* ctx, const char*);
};

// Called on worker thread
static int FolderOpen_Process(void* _ctx)
{
    FolderOpenContext* ctx = (FolderOpenContext*)_ctx;
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

    SetModalDialog(state, 0);

    if (NFD_OKAY == result)
    {
        ctx->paths = (const char**)malloc(sizeof(char**)*ctx->num_paths);
        ctx->paths[0] = (const char*)outpath;
    }

    return NFD_OKAY == result ? RESULT_OK : RESULT_FAILED;
}

// Called on main thread
static void FolderOpen_Finished(int result, void* _ctx)
{
    FolderOpenContext* ctx = (FolderOpenContext*)_ctx;
    AppState* state = ctx->state;

    if (RESULT_OK == result)
    {
        ctx->callback(ctx->callback_ctx, ctx->paths[0]);
    }

    for (int i = 0; i < ctx->num_paths; ++i)
        free((void*)ctx->paths[i]);
    free((void*)ctx->paths);
    delete ctx;
}

void CommandFolderOpen(HWorker worker, AppState* state, void (*callback)(void* ctx, const char*), void* callback_ctx)
{
    FolderOpenContext* ctx = new FolderOpenContext;
    ctx->state       = state;
    ctx->folder_only = true;
    ctx->extensions  = 0;
    ctx->callback    = callback;
    ctx->callback_ctx= callback_ctx;
    WorkerPushJob(worker, FolderOpen_Process, FolderOpen_Finished, (void*)ctx);
}
