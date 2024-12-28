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

typedef struct
{
    const char**        sources; // list of files or directories
    int                 num_sources;

    // Packer settings

    apOptions           options; // Generic options
    apTilePackerOptions options_tp;
    apBinPackerOptions  options_bp;

    PackerType          packer_type;
    apPacker*           packer;  // The packer to use. 0 if setup is invalid
    apContext*          context; // The final context to use. 0 if setup is invalid

    // The images are unaffected by the packer settings
    apImage*            images;
    int                 num_images;

} apProject;

void        apDestroyProject(apProject* project);
apProject*  apLoadProjectFromPath(const char* path);
apProject*  apLoadProjectFromMemory(const char* path, void* data);
int         apSaveProject(const char* path, apProject* project);

void apProjectAddSources(apProject* project, const char** sources, int num_sources);

void apProjectLoadImages(apProject* project);
void apProjectPackerPrepareImages(apProject* project);



void apDebugPrintProject(apProject* p);
