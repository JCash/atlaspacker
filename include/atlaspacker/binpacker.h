// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

#ifndef ATLASPACKER_BINPACKER_H
#define ATLASPACKER_BINPACKER_H

#include <atlaspacker/atlaspacker.h>

#pragma pack(1)

typedef enum
{
    AP_BP_MODE_DEFAULT = 0, // Default is whichever enum is also set as 0
    AP_BP_SKYLINE_BL = 0
} apBinPackMode;

typedef struct
{
    apBinPackMode   mode;
    int             no_rotate;
} apBinPackerOptions;

#pragma options align=reset

void      apBinPackerSetDefaultOptions(apBinPackerOptions* options);
apPacker* apBinPackerCreate(apBinPackerOptions* options);
void      apBinPackerDestroy(apPacker* packer);

#endif // ATLASPACKER_BINPACKER_H
