// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#ifndef ATLASPACKER_TOOL_IMAGE_H
#define ATLASPACKER_TOOL_IMAGE_H

#include <stdint.h>
#include "hash.h"

struct Image
{
    uint8_t*        data;       // The image data
    void*           context;    // Custom context for the application
    int             width;
    int             height;
    int             channels;
    hash_t          path_hash;  // hash of the relative path
    const char*     path;       // The relative path
};


Image*      CreateImage(const char* path, uint32_t color, int w, int h, int c);
Image*      LoadImage(const char* path);
void        DestroyImage(Image* image);
uint8_t*    CreateTileImage(Image* image, uint32_t tile_size, int alphathreshold, int* twidth, int* theight);

void        SortImages(Image** images, int num_images);

#endif // ATLASPACKER_TOOL_IMAGE_H
