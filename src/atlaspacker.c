#include <atlaspacker/atlaspacker.h>
#include <atlaspacker/binpacker.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // printf

apContext* apCreate(apOptions* options, apPacker* packer)
{
    assert(options != 0);
    assert(packer != 0);

    apContext* ctx = (apContext*)malloc(sizeof(apContext));
    memset(ctx, 0, sizeof(*ctx));
    ctx->options = *options;
    ctx->packer = packer;

    return ctx;
}

void apDestroy(apContext* ctx)
{
    for (int i = 0; i < ctx->num_images; ++i)
    {
        ctx->packer->destroyImage(ctx->packer, ctx->images[i]);
    }
    free((void*)ctx->images);
    free((void*)ctx);
}

void apAddImage(apContext* ctx, const char* path, int width, int height, int channels, uint8_t* data)
{
    if (channels > ctx->num_channels)
        ctx->num_channels = channels;
    apImage* image = ctx->packer->createImage(ctx->packer, path, width, height, channels, data);
    image->path = path;
    image->dimensions.width = width;
    image->dimensions.height = height;
    image->data = data;
    image->width = width;
    image->height = height;
    image->channels = channels;
    memset(&image->placement, 0, sizeof(image->placement));
    
    // add to the other images
    ctx->num_images++;
    ctx->images = (apImage**)realloc(ctx->images, sizeof(apImage**)*ctx->num_images);
    ctx->images[ctx->num_images-1] = image;
}

void apPackImages(apContext* ctx)
{
    ctx->packer->packImages(ctx->packer, ctx);
}

int apGetNumPages(apContext* ctx)
{
    return ctx->num_pages;
}

apPage* apAllocPage(apContext* ctx)
{
    ctx->num_pages++;
    ctx->pages = (apPage*)realloc(ctx->pages, sizeof(apPage)*ctx->num_pages);
    apPage* page = &ctx->pages[ctx->num_pages-1];
    memset(page, 0, sizeof(apPage));
    page->index = ctx->num_pages-1;
    // possibly set a max size of the page
    return page;
}

apPos apRotate(int x, int y, int width, int height, int rotation)
{
    apPos pos;
    if (rotation == 90)
    {
        pos.x = y;
        pos.y = width - 1 - x;
    }
    else if (rotation == 180)
    {
        pos.x = width - 1 - x;
        pos.y = height - 1 - y;
    }
    else if (rotation == 270)
    {
        pos.x = height - 1 - y;
        pos.y = x;
    }
    else {
        pos.x = x;
        pos.y = y;
    }
    return pos;
}

static void apCopyRGBA(uint8_t* dest, int dest_width, int dest_height, int dest_channels,
                        uint8_t* source, int source_width, int source_height, int source_channels,
                        int x, int y, int rotation);

uint8_t* apRenderPage(apContext* ctx, int page_index, int* width, int* height, int* channels)
{
    apPage* page = &ctx->pages[page_index];
    int w = page->dimensions.width;
    int h = page->dimensions.height;
    int c = ctx->num_channels;
    uint8_t* output = (uint8_t*)malloc(w * h * c);
    memset(output, 0, w * h * c);

    //printf("rendering page %d at %d x %d\n", page_index, w, h);

    int tile_size = 16;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int tx = x / tile_size;
            int ty = y / tile_size;
            int odd = ((tx&1) && !(ty&1)) | (!(tx&1) && (ty&1));

            uint8_t color_odd[4] = {255,255,255,128};
            uint8_t color_even[4] = {0,0,0,128};
            uint8_t* color = color_even;

            if (odd)
                color = color_odd;
                
            for (int i = 0; i < c; ++i)
                output[y * (w*c) + (x*c) + i ] = color[i];
        }
    }


    int num_rendered = 0;
    for (int i = 0; i < ctx->num_images; ++i)
    {
        apImage* image = ctx->images[i];
        if (image->page != page_index)
            continue;
        
        //printf("rendering image %s:%d at %d x %d\n", image->path, image->rotation, image->placement.pos.x, image->placement.pos.y);

        apCopyRGBA(output, w, h, c,
                    image->data, image->width, image->height, image->channels,
                    image->placement.pos.x, image->placement.pos.y, image->rotation);

        ++num_rendered;
    }
    if (num_rendered == 0)
    {
        printf("No images rendered on page %d\n", page_index);
        free((void*)output);
        return 0;
    }

    *width = w;
    *height = h;
    *channels = c;
    return output;
}


// Copy the source image into the target image
// Can handle cases where the target texel is outside of the destination
// Transparent source texels are ignored
static void apCopyRGBA(uint8_t* dest, int dest_width, int dest_height, int dest_channels,
                        uint8_t* source, int source_width, int source_height, int source_channels,
                        int dest_x, int dest_y, int rotation)
{
    for (int sy = 0; sy < source_height; ++sy)
    {
        for (int sx = 0; sx < source_width; ++sx, source += source_channels)
        {
            // Skip copying texels with alpha=0
            if (source_channels == 4 && source[4] == 0)
                continue;

            // Map the current coord into the rotated space
            apPos rotated_pos = apRotate(sx, sy, source_width, source_height, rotation);

            int target_x = dest_x + rotated_pos.x;
            int target_y = dest_y + rotated_pos.y;

            // If the target is outside of the destination area, we skip it
            if (target_x < 0 || target_x >= dest_width ||
                target_y < 0 || target_y >= dest_height)
                continue;

            int dest_index = target_y * dest_width * dest_channels + target_x * dest_channels;

            uint8_t color[4] = {255,255,255,255};
            for (int c = 0; c < source_channels; ++c)
                color[c] = source[c];

            int alphathreshold = 8;
            if (alphathreshold >= 0 && color[3] <= alphathreshold)
                continue; // Skip texels that are <= the alpha threshold

            for (int c = 0; c < dest_channels; ++c)
                dest[dest_index+c] = color[c];
            
            if (color[3] > 0 && color[3] < 255)
            {
                uint32_t r = dest[dest_index+0] + 48;
                dest[dest_index+0] = (uint8_t)(r > 255 ? 255 : r);
                dest[dest_index+1] = dest[dest_index+1] / 2;
                dest[dest_index+2] = dest[dest_index+2] / 2;

                uint32_t a = dest[dest_index+3] + 128;
                dest[dest_index+3] = (uint8_t)(a > 255 ? 255 : a);
            }
        }
    }
}

// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float
uint32_t apNextPowerOfTwo(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}