// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

#ifndef ATLASPACKER_PROJECT_H
#define ATLASPACKER_PROJECT_H

#include "atlaspacker.h"
#include "tilepacker.h"
#include "binpacker.h"

typedef enum PackerType
{
    PT_TILEPACKER,
    PT_BINPACKER,
} PackerType;

typedef enum OptionValueType
{
    OVT_BOOL,
    OVT_NUMBER,
    OVT_STRING,
} OptionValueType;

typedef struct apOptionValue
{
    struct apOptionValue* next;
    const char*           name;
    const char*           edit;
    const char*           desc;
    const char*           display;
    union {
        double      number;
        const char* string;
    } value;
    OptionValueType type;
} apOptionValue;

typedef struct apProjectStats
{
    int         num_images;
    uint64_t    image_load_time;   // microseconds
    uint64_t    layout_time;       // microseconds
    float       triangles_per_sprite;
    float       occupancy_percent;
} apProjectStats;

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

    const char*         exporter; // Name of the exporter
    apOptionValue*      exporter_options;
    apOptionValue*      exporter_defaults; // Only loaded in the editor

    // The images are unaffected by the packer settings
    apImage*            images;
    int                 num_images;

    apProjectStats      stats;

} apProject;

void        apDestroyProject(apProject* project);
apProject*  apLoadProjectFromPath(const char* path);
apProject*  apLoadProjectFromMemory(const char* path, void* data);
int         apSaveProject(const char* path, apProject* project);

void apProjectAddSources(apProject* project, const char** sources, int num_sources);

void apProjectLoadImages(apProject* project);
void apProjectPackerPrepareImages(apProject* project);
int  apProjectGetState(apProject* project, apProjectStats* stats);

// internal
void apDestroyOptions(apOptionValue* option);

void apDebugPrintProject(apProject* p);

#endif // ATLASPACKER_PROJECT_H
