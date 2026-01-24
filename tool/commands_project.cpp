// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

#include "commands.h"
#include "worker.h"
#include "state.h"

#include <unistd.h> // getcwd
#include <stdlib.h>
#include <string.h>

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

static void DestroyAtlasPages(AppState* state)
{
    for (int i = 0; i < state->num_pages; ++i)
    {
        free(state->pages[i].data);
    }
    free(state->pages);
    state->pages = 0;
    state->num_pages = 0;
}

static void ResetProjectState(AppState* state)
{
    DestroyAtlasPages(state);
    DestroyImages(state);
    if (state->images.Capacity() > 0)
        state->images.Clear();
    if (state->selected_images.Capacity() > 0)
        state->selected_images.Clear();

    if (state->images_root)
    {
        TreeNodeTreeDestroy(state->images_root);
        state->images_root = 0;
    }

    state->max_image_size = 0;
    state->loading_images = 0;
    CreateDefaultTexture(state);
}

static int RemoveProjectSource(apProject* project, const char* source)
{
    if (!project || !project->sources || !source)
        return 0;

    for (int i = 0; i < project->num_sources; ++i)
    {
        if (strcmp(project->sources[i], source) == 0)
        {
            free((void*)project->sources[i]);
            for (int j = i + 1; j < project->num_sources; ++j)
            {
                project->sources[j - 1] = project->sources[j];
            }
            project->num_sources--;
            if (project->num_sources == 0)
            {
                free((void*)project->sources);
                project->sources = 0;
            }
            return 1;
        }
    }
    return 0;
}

static void RemoveImageByHash(AppState* state, hash_t path_hash)
{
    Image** image_ptr = state->images.Get(path_hash);
    if (!image_ptr)
        return;

    Image* image = *image_ptr;
    AppTexture* texture = (AppTexture*)image->context;
    if (texture)
        DeleteTexture(texture);
    DestroyImage(image);
    state->images.Erase(path_hash);
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
        ctx->path = outpath;
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
            if (project->exporter)
            {
                char path[2048];
                const char* exporter = FindExporter(state, project->exporter, path, sizeof(path));
                if (exporter)
                {
                    free((void*)state->exporter_path);
                    state->exporter_path = strdup(exporter);
                    if (!apExportUpdateOptions(project, state->exporter_path))
                    {

                    }
                    project->exporter_defaults = apExportGetDefaultOptions(project, state->exporter_path);
                }
                else
                {
                    fprintf(stderr, "Failed to find exporter '%s/exporter.lua'\n", project->exporter);
                }
            }

            apDebugPrintProject(project);

            if (state->project)
                apDestroyProject(state->project);
            ClearExporterOptionCache(state);

            SCOPED_MUTEX(state->mutex);
            ResetProjectState(state);
            if (state->path)
            {
                free((void*)state->path);
                state->path = 0;
            }
            state->project = project;
            state->path    = ctx->path;
            ctx->path      = 0;

            CommandLoadImages(state->thread, state);
        }
    }

    free((void*)ctx->path);
    delete ctx;

    SetModalDialog(state, 0);
    {
        SCOPED_MUTEX(state->mutex);
        state->project_open_pending = 0;
    }
}

void CommandProjectFileOpen(HWorker worker, AppState* state)
{
    {
        SCOPED_MUTEX(state->mutex);
        if (state->project_open_pending)
            return;
        state->project_open_pending = 1;
    }
    ProjectFileOpenContext* ctx = new ProjectFileOpenContext;
    ctx->state = state;
    ctx->path  = 0;
    WorkerPushJob(worker, ProjectFileOpen_Process, ProjectFileOpen_Finished, ctx);
}

// ************************************************************************************
// New Project File

void CommandProjectFileNew(HWorker worker, AppState* state)
{
    (void)worker;

    SCOPED_MUTEX(state->mutex);

    printf("New file command invoked\n");

    if (state->path)
    {
        free((void*)state->path);
        state->path = 0;
    }

    if (state->project)
        apDestroyProject(state->project);
    state->project = apLoadProjectFromMemory("untitled", 0);

    ClearExporterOptionCache(state);
    ResetProjectState(state);

    state->dirty = 0;
}

// ************************************************************************************
// Delete selected images (top-level only)

void CommandDeleteSelectedImages(HWorker worker, AppState* state)
{
    (void)worker;

    SCOPED_MUTEX(state->mutex);

    if (!state->project || !state->images_root)
        return;

    TreeNode* root = state->images_root;
    TreeNode* prev = 0;
    TreeNode* node = root->child;
    int removed = 0;

    while (node)
    {
        TreeNode* next = node->sibling;
        if (node->type == TN_TYPE_IMAGE && node->selected)
        {
            if (RemoveProjectSource(state->project, node->path))
            {
                RemoveImageByHash(state, node->path_hash);
                state->selected_images.Erase(node->path_hash);

                if (prev)
                    prev->sibling = next;
                else
                    root->child = next;

                free((void*)node->path);
                free((void*)node);
                removed++;
            }
            else
            {
                prev = node;
            }
        }
        else
        {
            prev = node;
        }
        node = next;
    }

    if (removed == 0)
        return;

    int max_size = 0;
    for (jc::HashTable<hash_t, Image*>::Iterator it = state->images.Begin(); it != state->images.End(); ++it)
    {
        Image* image = *it.GetValue();
        if (image->width > max_size)
            max_size = image->width;
        if (image->height > max_size)
            max_size = image->height;
    }
    state->max_image_size = max_size;
    state->dirty = 1;

    CommandRecreateAtlas(state->thread, state);
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
                state->path = outpath;
                outpath = 0;

                printf("MAWE Wrote document: %s\n", state->path);
            }
            free(outpath);
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
        printf("Wrote project file: %s\n", state->path ? state->path : "(unknown)");

        // TODO: Update the window title!
        //UpdateWindowTitle(state, false);
    }
    SetModalDialog(state, 0);
    {
        SCOPED_MUTEX(state->mutex);
        state->project_save_pending = 0;
    }
}

void CommandProjectFileSave(HWorker worker, AppState* state)
{
    {
        SCOPED_MUTEX(state->mutex);
        if (state->project_save_pending)
            return;
        state->project_save_pending = 1;
    }
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

    printf("Export command invoked\n");

    if (!state->project)
    {
        return RESULT_FAILED;
    }

    if (!state->exporter_path)
    {
        printf("No exporter path set\n");
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
    {
        SCOPED_MUTEX(state->mutex);
        state->project_export_pending = 0;
    }
}

void CommandProjectFileExport(HWorker worker, AppState* state)
{
    {
        SCOPED_MUTEX(state->mutex);
        if (state->project_export_pending)
            return;
        state->project_export_pending = 1;
    }
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
