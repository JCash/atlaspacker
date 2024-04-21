#pragma once

#include <atlaspacker/util.h>
#include <atlaspacker/project.h>
#include <thread.h>
#include "worker.h"

#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_imgui.h> // for simgui_image_t
typedef void* ImTextureID;


typedef struct {
    sg_image        image;
    simgui_image_t  imgui_image;
    ImTextureID     texture_id;
} AppTexture;

typedef struct {
    const char* path;
    apProject*  project;

    // Trick to delay open a file dialog
    int         open_project_dialog:1;	// For opening a project open dialog
    int         save_project_dialog:1;  // For opening a project save dialog
    int         open_file_dialog:1;		// For adding an image file to the project
    int         open_folder_dialog:1;	// For adding a folder containing image file to the project

    // Async state
    int         dirty_fileset; 	// The images need to be loaded
    int         loading_images; // Loading images is underway
    int         creating_atlas; // Recreating the context, packer and the final atlas

    thread_mutex_t mutex;
    worker*        thread;

    float           zoom;
    int             num_page_textures;
    AppTexture*     page_textures;
    apSize          page_size;

    // If set, then the textures need to be recreated
    // OpenGL requires you to do this on the context thread (unless you create an aux context)
    Page*           pages;
    int             num_pages;

	Image** 	images; // the raw images
	int         num_images;

	TreeNode    images_root; // The tree of images

} AppState;
