// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#pragma once
#include "atlaspacker.h"
#include "tilepacker.h"
#include "binpacker.h"

typedef enum PackerType
{
    PT_TILEPACKER,
    PT_BINPACKER,
} PackerType;

// typedef struct
// {
//     const char* path;
//     float       pivot_x;
//     float       pivot_y;
//     int         no_rotate:1; // 0: rotation allowed !=0: no rotation allowed
//     int         has_pivot:1; // 1: the pivot_x/pivot_y are valid
// } ImageInfo;

typedef struct
{
    const char**        sources; // list of files or directories
    int                 num_sources;

    // ImageInfo*      resolved_paths;
    // int             num_resolved_paths;

    // // Generic packer options
    // PackerType      packer_type;
    // int             po_page_size;   // The max size of each page

    // // packer option: tile packer (po_tp_)
    // int             po_tp_tile_size;    // Powers of two
    // int             po_tp_alpha;        // 0-255
    // int             po_tp_padding;      // Padding around each image
    // int             po_tp_no_rotate;    // Allows for rotating the image or not

    // // Output settings
    // const char*     output_path;

    // Exporter settings

    apOptions           options; // Generic options
    apTilePackerOptions options_tp;
    apBinPackerOptions  options_bp;

    PackerType          packer_type;
    apPacker*           packer;  // The packer to use. 0 if setup is invalid
    apContext*          context; // The final context to use. 0 if setup is invalid

    // The images are unaffected by the packer settings
    apImage*            images;
    int                 num_images;

    // // Any tile images are created for use with the tile packer
    // apTileImage*        tile_images;
    // int                 num_tile_images;

} apProject;

void        apDestroyProject(apProject* project);
apProject*  apLoadProjectFromPath(const char* path);
apProject*  apLoadProjectFromMemory(const char* path, void* data);
int         apSaveProject(const char* path, apProject* project);

void apProjectAddSources(apProject* project, const char** sources, int num_sources);

void apProjectLoadImages(apProject* project);
void apProjectPackerPrepareImages(apProject* project);



void apDebugPrintProject(apProject* p);
