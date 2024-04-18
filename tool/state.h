#pragma once

#include <atlaspacker/util.h>
#include <atlaspacker/project.h>
#include <thread.h>
#include "worker.h"

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


	Image** 	images; // the raw images
	int         num_images;

	TreeNode    images_root; // The tree of images

} AppState;
