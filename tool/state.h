#pragma once

extern "C" {
    #include <atlaspacker/util.h>
    #include <atlaspacker/project.h>
}

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

typedef void (*FileDialogCallbackFn)(struct AppState* state, const char** paths, int num_paths);

struct AppState
{
    const char* path;
    apProject*  project;

    // Trick to delay open a file dialog
    int                     open_project_dialog:1;	// For opening a project open dialog
    int                     save_project_dialog:1;  // For opening a project save dialog
    int                     open_file_dialog:1;		// For adding an image file to the project
    int                     open_folder_dialog:1;	// For adding a folder containing image file to the project
    const char*             file_dialog_extensions;
    FileDialogCallbackFn    file_dialog_callback;

    // state
    int         dirty:1; // Changes were made, and the project is dirty

    // Async state
    int         dirty_fileset; 	// The images need to be loaded
    int         loading_images; // Loading images is underway
    int         creating_atlas; // Recreating the context, packer and the final atlas

    HMutex      mutex;
    HWorker     thread;

    float       zoom;

    jc::Array<AppTexture> page_textures;
    apSize                page_size;

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

