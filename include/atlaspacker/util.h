// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#pragma once

#include <stdint.h>
#include <atlaspacker/atlaspacker.h>

typedef struct _Image
{
    struct _Image*  next;
    int             width;
    int             height;
    int             channels;
    uint8_t*        data;
    const char*     path;
    const char*     source; // if null, it's from a single file reference
} Image;

typedef struct _TreeNode
{
    struct _TreeNode* sibling;
    struct _TreeNode* child; // The first child
    int               type; // 0: folder, 1; file
    void*             data; // type==0: folder name, type==1: Image
    uint8_t           selected:1;
    uint8_t           readonly:1;
    uint8_t           :6;
} TreeNode;

// Gets time in micro seconds
uint64_t    GetTime();

Image*      CreateImage(const char* path, uint32_t color, int w, int h, int c);
Image*      LoadImage(const char* path);
void        DestroyImage(Image* image);
uint8_t*    CreateTileImage(Image* image, uint32_t tile_size, int alphathreshold, int* twidth, int* theight);

void    SortImages(Image** images, int num_images);

int     IsFile(const char* path);
int     IsDir(const char* path);
int     IterateFiles(const char* dirpath, int recursive, int (*callback)(void* ctx, const char*), void* ctx);

void    DebugPrintTileImage(uint32_t width, uint32_t height, uint8_t* data);

uint8_t* ReadFile(const char* path, uint32_t* file_size);

typedef struct
{
    uint8_t*    data;
    int         width;
    int         height;
    int         channels;
} Page;

Page* apRenderPages(apContext* context, int* num_pages, uint32_t debug_color);
