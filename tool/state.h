// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#pragma once

extern "C" {
    #include <atlaspacker/util.h>
    #include <atlaspacker/project.h>
}

#include <stdint.h>

#include <imgui.h>

#include <sokol_gfx.h>
#include <sokol_app.h>

// External
#include <external/array.h>
#include <external/hashtable.h>

// Editor
#include "hash.h"
#include "image.h"
#include "thread.h"
#include "tree.h"
#include "worker.h"

struct AppTexture {
    sg_image        image;
    ImTextureID     texture_id;
};

static const char* VERSION = "0.1";

typedef void (*FileDialogCallbackFn)(struct AppState* state, const char** paths, int num_paths);

struct AppState
{
    const char* path;
    apProject*  project;

    HMutex      mutex;      // Protects the state

    HWorker     thread;
    HWorker     uithread;   // Delayed jobs like file open dialogs

    // Set if a modal dialog is already opened. To disable accidental ImGui interactions
    int         modal_dialog:1;
    // Changes were made, and the project is dirty
    int         dirty:1;

    // // Async state
    int         loading_images; // Loading images is underway
    // int         creating_atlas; // Recreating the context, packer and the final atlas

    float       zoom;

    jc::Array<AppTexture*> page_textures;
    apSize                 page_size;

    // If set, then the textures need to be recreated
    // OpenGL requires you to do this on the context thread (unless you create an aux context)
    Page*           pages;
    int             num_pages;

    int             max_image_size; // What is the largest image size in the set?


    jc::HashTable<hash_t, Image*> images; // the raw images

    // Since we have a potential many-to-one mapping from source images to atlas images
    // it's convenient to keep track of the current hovers and selections here
    jc::HashTable<hash_t, bool> selected_images;

	TreeNode*   images_root; // The tree of images

    // Debug draw options
    bool        debug_draw_triangles;
};

// ****************************************************************************************
// Project
void    ProjectAddSource(AppState* state, const char** paths, uint32_t num_paths);

// ****************************************************************************************
// Images
Image*  GetImage(AppState* state, hash_t path_hash);
void    AddImage(AppState* state, Image* image);
void    DestroyImages(AppState* state);

// ****************************************************************************************
// Textures
void        DeleteTexture(AppTexture* texture);
AppTexture* CreateTexture(uint8_t* image, int width, int height, int channels);
AppTexture* CreateTextureFromImage(Image* image);

// ****************************************************************************************
// Page textures
void AllocPagesTextures(AppState* state, int count);
void DeletePageTextures(AppState* state);
void CreateDefaultTexture(AppState* state);
void CreateAtlasTextures(AppState* state);
