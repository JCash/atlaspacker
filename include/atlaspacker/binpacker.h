// https://github.com/JCash/atlaspacker
// @2021 Mathias Westerdahl

#pragma once

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
} apBinPackOptions;

#pragma options align=reset

apPacker* apCreateBinPacker(apBinPackOptions* options);
void      apDestroyBinPacker(apPacker* packer);
