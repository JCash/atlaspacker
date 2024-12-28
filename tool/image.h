// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#pragma once

#include <stdint.h>
#include "hash.h"

struct Image
{
    //struct Image*   next;
    int             width;
    int             height;
    int             channels;
    uint8_t*        data;
    hash_t          path_hash;
    const char*     path;
    const char*     source; // if null, it's from a single file reference
};


Image*      CreateImage(const char* path, uint32_t color, int w, int h, int c);
Image*      LoadImage(const char* path);
void        DestroyImage(Image* image);
uint8_t*    CreateTileImage(Image* image, uint32_t tile_size, int alphathreshold, int* twidth, int* theight);

void        SortImages(Image** images, int num_images);
