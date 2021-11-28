#pragma once

#include <stdint.h>
#include <atlaspacker/atlaspacker.h>

#pragma pack(1)
typedef struct _Image
{
    int width;
    int height;
    int channels;
    uint8_t* data;
    const char* path;
} Image;
#pragma options align=reset


Image*  CreateImage(const char* path, uint32_t color, int w, int h, int c);
Image*  LoadImage(const char* path);
void    DestroyImage(Image* image);
void    SortImages(Image** images, int num_images);
int     DebugWriteOutput(apContext* ctx, const char* pattern);
void    DebugDrawHull(const apPosf* vertices, int num_vertices, uint8_t* color, const Image* image, uint8_t* data);

