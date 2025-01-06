// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#include "state.h"
#include "sys.h"

#include <stdio.h> // printf
#include <limits.h> // PATH_MAX

#include <sokol_gfx.h>
#include <sokol_imgui.h>

extern "C" {
    #include <atlaspacker/file.h>
}

// ****************************************************************************************

void ProjectAddSource(AppState* state, const char** paths, uint32_t num_paths)
{
    SCOPED_MUTEX(state->mutex);

    if (state->project)
    {
        apProjectAddSources(state->project, paths, num_paths);
    }
}

// ****************************************************************************************

Image* GetImage(AppState* state, hash_t path_hash)
{
    SCOPED_MUTEX(state->mutex);
    Image** pimage = state->images.Get(path_hash);
    return pimage ? *pimage : 0;
}

void AddImage(AppState* state, Image* image)
{
    SCOPED_MUTEX(state->mutex);

    if (state->images.Full())
    {
        uint32_t cap = state->images.Capacity() + 32;
        state->images.SetCapacity(cap);
    }
    state->images.Put(image->path_hash, image);
}

void DestroyImages(AppState* state)
{
    for (jc::HashTable<hash_t, Image*>::Iterator it = state->images.Begin(); it != state->images.End(); ++it)
    {
        Image* image = *it.GetValue();
        AppTexture* texture = (AppTexture*)image->context;
        if (texture)
            DeleteTexture(texture);
        DestroyImage(image);
    }
}

// ****************************************************************************************

void DeleteTexture(AppTexture* texture)
{
    sg_destroy_image(texture->image);
    delete texture;
}

AppTexture* CreateTexture(uint8_t* image, int width, int height, int channels)
{
    if (width == 0 || height == 0 || channels == 0)
    {
        return 0;
    }

    uint8_t* tmp = 0;
    if (channels == 3)
    {
        // TODO: Move to a apRGBToRGBA() helper function
        tmp = (uint8_t*)malloc(width * height * 4);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                tmp[y * 4 * width + x * 4 + 0] = image[y * 3 * width + x * 3 + 0];
                tmp[y * 4 * width + x * 4 + 1] = image[y * 3 * width + x * 3 + 1];
                tmp[y * 4 * width + x * 4 + 2] = image[y * 3 * width + x * 3 + 2];
                tmp[y * 4 * width + x * 4 + 3] = 0xFF;
            }
        }

        image = tmp;
    }

    sg_image_desc def_image_desc;
    memset(&def_image_desc, 0, sizeof(def_image_desc));
    def_image_desc.width = width;
    def_image_desc.height = height;
    def_image_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    def_image_desc.data.subimage[0][0].ptr = image;
    def_image_desc.data.subimage[0][0].size = width * height * 4;
    def_image_desc.label = "atlas-image";

    AppTexture* texture = new AppTexture;
    texture->image = sg_make_image(&def_image_desc);
    texture->texture_id = simgui_imtextureid(texture->image);

    if (tmp)
    {
        free((void*)tmp);
    }
    return texture;
}

AppTexture* CreateTextureFromImage(Image* image)
{
    AppTexture* texture = CreateTexture(image->data, image->width, image->height, image->channels);
    if (texture->texture_id == 0)
    {
        DeleteTexture(texture);
        return 0;
    }
    return texture;
}

// ****************************************************************************************

void AllocPagesTextures(AppState* state, int count)
{
    state->page_textures.SetCapacity(count);
    state->page_textures.SetSize(0);
}

void DeletePageTextures(AppState* state)
{
    for (int i = 0; i < state->page_textures.Size(); ++i)
    {
        DeleteTexture(state->page_textures[i]);
    }
    state->page_textures.SetSize(0);
}

void CreateDefaultTexture(AppState* state)
{
    const int width = 64;
    const int height = 64;
    uint32_t def_pixels[width*height];
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if ((x+y)&1) def_pixels[y*width + x] = 0xFF333333;
            else         def_pixels[y*width + x] = 0xFF555555;
        }
    }
    DeletePageTextures(state);
    AllocPagesTextures(state, 1);
    AppTexture* texture = CreateTexture((uint8_t*)def_pixels, width, height, 4);
    state->page_textures.Push(texture);

    state->zoom = 1.0f / state->page_textures.Size();
}

void CreateAtlasTextures(AppState* state)
{
    int old_num_pages = state->page_textures.Size();

    DeletePageTextures(state);
    AllocPagesTextures(state, state->num_pages);

    for (int i = 0; i < state->num_pages; ++i)
    {
        Page* page = &state->pages[i];
        AppTexture* texture = CreateTexture(page->data, page->width, page->height, page->channels);
        if (texture)
            state->page_textures.Push(texture);

        state->page_size.width = page->width;
        state->page_size.height = page->height;
        free((void*)page->data);
    }

    if (state->num_pages == 0)
        state->num_pages = 1;
    if (old_num_pages != state->num_pages) // We want to maintain the zoom while the user is updating settings
        state->zoom = 1.0f / state->num_pages;

    free((void*)state->pages);
    state->pages = 0;
    state->num_pages = 0;
}

// ****************************************************************************************

void AddExporterFolder(AppState* state, const char* folder)
{
    if (!IsDir(folder))
        return;

    if (state->exporter_folders.Full())
        state->exporter_folders.SetCapacity(state->exporter_folders.Capacity()+1);
    state->exporter_folders.Push(strdup(folder));

    printf("EXPORTER FOLDER: '%s'\n", folder);
}

void UpdateExporterFolders(AppState* state)
{
    // If set, let's priotiize them
    GetEnvVarDirs("AP_EXPORTER_DIRS", (void (*)(void*, const char*))AddExporterFolder, state);

    char path[PATH_MAX] = "";
    if (GetApplicationPath(path, sizeof(path)))
    {
        AddExporterFolder(state, path);
    }

    if (GetWorkingDir(path, sizeof(path)))
    {
        AddExporterFolder(state, path);
    }
}

void FreeExporterFolders(AppState* state)
{
    for (uint32_t i = 0; i < state->exporter_folders.Size(); ++i)
    {
        free((void*)state->exporter_folders[i]);
    }
    state->exporter_folders.SetSize(0);
}

const char* FindExporter(AppState* state, const char* exporter, char* buffer, uint32_t buffer_size)
{
    for (uint32_t i = 0; i < state->exporter_folders.Size(); ++i)
    {
        const char* folder = state->exporter_folders[i];
        snprintf(buffer, buffer_size, "%s/%s/exporter.lua", folder, exporter);
        if (IsFile(buffer))
            return buffer;
    }
    return 0;
}

// ****************************************************************************************
Preferences* CreatePreferences()
{
    Preferences* prefs = new Preferences;
    return prefs;
}

void DestroyPreferences(Preferences* prefs)
{
    delete prefs;
}

Preferences* LoadPreferences(const char* path)
{
    return 0;
}

bool SavePreferences(const char* path, Preferences* prefs)
{
    return false;
}
