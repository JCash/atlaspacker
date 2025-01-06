// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#include "commands.h"
#include "worker.h"
#include "state.h"

#include <unistd.h> // getcwd

extern "C" {
    #include <atlaspacker/file.h>
    #include <atlaspacker/project.h>
}

#include <stdio.h> // printf

// ************************************************************************************
// Recreate the atlas information

static int RecreateAtlas_Process(void* _ctx)
{
    AppState* state = (AppState*)_ctx;

    uint64_t tend;
    uint64_t tstart;

    SCOPED_MUTEX(state->mutex);

    apProject* project = state->project;
    apPacker* packer = 0;

    printf("Creating packer of type: %d\n", project->packer_type);

    if (project->packer_type == PT_TILEPACKER)
        packer = apTilePackerCreate(&project->options_tp);
    else
        packer = apBinPackerCreate(&project->options_bp);

    if (!packer)
    {
        // Handle any errors
    }

    tstart = GetTime();
    project->context = apCreate(&project->options, packer);
    if (project->context)
    {
        printf("Adding images: %u\n", state->images.Size());
        tstart = GetTime();

        // TODO: Make sure we only create apImages for the unique images that we want to pack
        // Any many-to-one mappings needs to happen before this point.
        for (jc::HashTable<hash_t, Image*>::Iterator it = state->images.Begin(); it != state->images.End(); ++it)
        {
            Image* image = *it.GetValue();
            apAddImage(project->context, image->path, image->width, image->height, image->channels, image->data);
        }

        tend = GetTime();

        printf("Adding images took %.2f ms\n", (tend-tstart)/1000.0f);


        tstart = GetTime();

        apPackImages(project->context);

        tend = GetTime();
        printf("Packing atlas images took %.2f ms\n", (tend-tstart)/1000.0f);
    }

    {
        SCOPED_MUTEX(state->mutex);
        state->num_pages = 0;
        state->pages = apRenderPages(project->context, &state->num_pages, 0);
    }

    return RESULT_OK;
}

// called on the main thread
static void RecreateAtlas_Finished(int result, void* _ctx)
{
    AppState* state = (AppState*)_ctx;
    if (RESULT_OK == result)
    {
        SCOPED_MUTEX(state->mutex);
        if (state->pages)
            CreateAtlasTextures(state);
    }
}

void CommandRecreateAtlas(HWorker worker, AppState* state)
{
    WorkerPushJob(worker, RecreateAtlas_Process, RecreateAtlas_Finished, (void*)state);
}


// ************************************************************************************
// Load images on a thread


static int IsImageSuffix(const char* suffix)
{
    return suffix != 0 && (strcmp(suffix, ".png") == 0 || strcmp(suffix, ".PNG") == 0);
}

typedef struct ImageLoaderContext
{
    TreeNode* root;
    TreeNode* parent; // Current node to attach to
    Image*    list;
} ImageLoaderContext;


typedef int (*QsortFn)(const void*, const void*);
static int ComparePaths(const char** _a, const char** _b)
{
    const char* a = *_a;
    const char* b = *_b;
    return strcmp(a, b);
}

struct ImageListContext
{
    const char*            root;
    jc::Array<const char*> paths;

    ~ImageListContext()
    {
        for (uint32_t i = 0; i < paths.Size(); ++i)
        {
            free((void*)paths[i]);
        }
    }

    void SortPaths()
    {
        qsort(paths.Begin(), (size_t)paths.Size(), sizeof(const char*), (QsortFn)ComparePaths);
    }

    void AddPath(const char* path)
    {
        // TODO: Make this relative path more robust
        const char* relative = strstr(path, root);

        if (paths.Full())
            paths.SetCapacity(paths.Capacity()+32);
        paths.Push(strdup(relative));
    }
};

static int ImageListIterator(void* _ctx, const char* path)
{
    ImageListContext* ctx = (ImageListContext*)_ctx;

    const char* suffix = strrchr(path, '.');
    if (!IsImageSuffix(suffix))
        return 0; // continue

    ctx->AddPath(path);
    return 0;
}

// Called from the worker thread
static void LoadImageAndAddNode(AppState* state, TreeNode* parent, const char* path)
{
    hash_t path_hash = Hash(path);

    Image* image = GetImage(state, path_hash);
    if (!image)
    {
        Image* image = LoadImage(path);
        AddImage(state, image);
        image->path_hash = path_hash;
    }
    // TODO: increment ref count

    // Hook it into the tree
    TreeNode* n = TreeNodeFindChild(parent, path);
    if (!n)
    {
        n = TreeNodeCreateImage(path);
        TreeNodeAdd(parent, n);
    }
}

static int LoadImages_Process(void* ctx)
{
    AppState* state = (AppState*)ctx;

    uint64_t tstart = GetTime();

    jc::Array<const char*> sources;
    TreeNode* root = 0;

    {
        SCOPED_MUTEX(state->mutex);

        state->loading_images = 1;

        apProject* project = state->project;
        sources.SetCapacity(project->num_sources);
        for (uint32_t i = 0; i < project->num_sources; ++i)
        {
            sources.Push(strdup(project->sources[i]));
        }

        root = state->images_root ? TreeNodeTreeClone(state->images_root) : TreeNodeCreateFolder("images");
    }

    char project_dir[2048];
    if (state->path)
    {
        strncpy(project_dir, state->path, sizeof(project_dir));
        char* end = strrchr(project_dir, '/');
        if (end)
            *(end+1) = 0;
    }
    else
    {
        getcwd(project_dir, sizeof(project_dir));
    }

    for (int i = 0; i < sources.Size(); ++i)
    {
        const char* path = sources[i];

        char full_path[2048];
        if (path[0] == '.' && path[1] == '/') // relative path
        {
            path += 2;
            strncpy(full_path, project_dir, sizeof(full_path));
            strncat(full_path, path, sizeof(full_path) - strlen(full_path) - 1);
        }
        else
        {
            strncpy(full_path, path, sizeof(full_path));
        }

        if (IsFile(full_path))
        {
            LoadImageAndAddNode(state, root, path);
        }
        else if (IsDir(full_path))
        {
            const char* folder = strrchr(full_path, '/');
            while ((*folder) == '/')
                folder++;

            TreeNode* foldernode = TreeNodeFindChild(root, folder);
            if (!foldernode)
            {
                foldernode = TreeNodeCreateFolder(folder);
                TreeNodeAdd(root, foldernode);
            }

            ImageListContext file_list;
            file_list.root = full_path;
            IterateFiles(full_path, true, ImageListIterator, &file_list);
            file_list.SortPaths();

            for (uint32_t j = 0; j < file_list.paths.Size(); ++j)
            {
                const char* relative = file_list.paths[j];
                LoadImageAndAddNode(state, foldernode, relative);
            }
        }
    }

    {
        SCOPED_MUTEX(state->mutex);

        state->max_image_size = 0;
        for (jc::HashTable<hash_t, Image*>::Iterator it = state->images.Begin(); it != state->images.End(); ++it)
        {
            Image* image = *it.GetValue();
            if (image->width > state->max_image_size)
                state->max_image_size = image->width;
            if (image->height > state->max_image_size)
                state->max_image_size = image->height;
        }

        state->loading_images = 0;

        if (state->images_root)
            TreeNodeTreeDestroy(state->images_root);
        state->images_root = root;

        CommandRecreateAtlas(state->thread, state);
    }

    uint64_t tend = GetTime();
    printf("ThreadLoadImages: Loaded %u images in %.3f s!\n", state->images.Size(), (tend - tstart) / 1000000.0f);
    return RESULT_OK;
}

static void LoadImages_Finished(int result, void* _ctx)
{
    (void)result;
    (void)_ctx;
}

void CommandLoadImages(HWorker worker, AppState* state)
{
    WorkerPushJob(worker, LoadImages_Process, LoadImages_Finished, (void*)state);
}
