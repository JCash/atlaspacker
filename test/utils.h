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

// Gets time in micro seconds
uint64_t    GetTime();

Image*      CreateImage(const char* path, uint32_t color, int w, int h, int c);
Image*      LoadImage(const char* path);
void        DestroyImage(Image* image);
uint8_t*    CreateTileImage(Image* image, uint32_t tile_size, int alphathreshold, int* twidth, int* theight);

void    SortImages(Image** images, int num_images);
int     DebugWriteOutput(apContext* ctx, const char* pattern);
void    DebugDrawHull(const apPosf* vertices, int num_vertices, uint8_t* color, const Image* image, uint8_t* data);
void    DebugDrawTriangles(const apPosf* triangles, int num_vertices, uint8_t* color, const Image* image, uint8_t* data);

int     IterateFiles(const char* dirpath, int recursive, int (*callback)(void* ctx, const char*), void* ctx);

void    DebugPrintTileImage(uint32_t width, uint32_t height, uint8_t* data);
