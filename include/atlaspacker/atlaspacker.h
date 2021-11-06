// https://github.com/JCash/atlaspacker
// @2021 Mathias Westerdahl

/////////////////////////////////////////////////////////
#pragma once

#include <stdint.h>

#pragma pack(1)

typedef struct {
    int x, y;
} apPos;

typedef struct {
    int width, height;
} apSize;

typedef struct {
    apPos   pos;    // bottom left
    apSize  size;
} apRect;

typedef struct {
    apSize  dimensions;
    int     index;
} apPage;

typedef struct {
    const char* path;
    uint8_t*    data;
    apSize      dimensions;  // Original dimensions
    apRect      placement;   // The placement in the atlas
    int         width;
    int         height;
    int         channels;
    int         rotation;    // Degrees CCW: 0, 90, 180, 270
    int         page;
} apImage;

typedef struct
{
    int not_used;
} apOptions;

typedef struct {
    apOptions           options;

    apImage**           images;
    int                 num_images;
    apPage*             pages;
    int                 num_pages;
    int                 num_channels; // Max of the number of channels of each image
    struct _apPacker*   packer;
} apContext;

typedef struct _apPacker
{
    const char* packer_type;
    apImage* (*createImage)(struct _apPacker* packer, const char* path, int width, int height, int channels, uint8_t* data);
    void (*destroyImage)(struct _apPacker* packer, apImage* image);
    void (*packImages)(struct _apPacker* packer, apContext* ctx);
} apPacker;

#pragma options align=reset

/////////////////////////////////////////////////////////
apContext*  apCreate(apOptions* options, apPacker* packer);
void        apDestroy(apContext* ctx);
void        apAddImage(apContext* ctx, const char* path, int width, int height, int channels, uint8_t* data);
void        apPackImages(apContext* ctx);

// Get number of pages
int         apGetNumPages(apContext* ctx);

// returns an RGBA texture. Caller owns the memory
uint8_t*    apRenderPage(apContext* ctx, int page, int* width, int* height, int* channels);

//void        apRenderLayout(apContext* ctx, void (*callback)(int page, apImage* image));


/////////////////////////////////////////////////////////
// Internal
apPage*     apAllocPage(apContext* ctx);
uint32_t    apNextPowerOfTwo(uint32_t v);
// Rotates a coord (x,y) in fixed rotations of [(]0,90,180,270] degrees
apPos       apRotate(int x, int y, int width, int height, int rotation);

