// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#ifndef ATLASPACKER_UTIL_H
#define ATLASPACKER_UTIL_H

#include <stdint.h>
#include <atlaspacker/atlaspacker.h>

// Gets time in micro seconds
uint64_t    GetTime();

void    DebugPrintTileImage(uint32_t width, uint32_t height, uint8_t* data);

typedef struct
{
    uint8_t*    data;
    int         width;
    int         height;
    int         channels;
} Page;

Page* apRenderPages(apContext* context, int* num_pages, uint32_t debug_color);

#endif // ATLASPACKER_UTIL_H
