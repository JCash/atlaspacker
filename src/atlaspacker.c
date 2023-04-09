#include <atlaspacker/atlaspacker.h>
#include <atlaspacker/binpacker.h>

#include <assert.h>
#include <math.h> // sqrtf
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

apImage* apAddImage(apContext* ctx, const char* path, int width, int height, int channels, const uint8_t* data)
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

    ctx->num_images++;
    ctx->images = (apImage**)realloc(ctx->images, sizeof(apImage**)*ctx->num_images);
    ctx->images[ctx->num_images-1] = image;

    return image;
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
                        const uint8_t* source, int source_width, int source_height, int source_channels,
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

// DEBUG BACKGROUND RENDERING
    int tile_size = 16;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int tx = x / tile_size;
            int ty = y / tile_size;
            int odd = ((tx&1) && !(ty&1)) | (!(tx&1) && (ty&1));

            // uint8_t color_odd[4] = {255,255,255,128};
            // uint8_t color_even[4] = {0,0,0,128};
            // uint8_t color_odd[4] = {32,32,32,255};
            // uint8_t color_even[4] = {16,16,16,255};
            uint8_t color_odd[4] = {64,96,64,255};
            uint8_t color_even[4] = {32,64,32,255};

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
                        const uint8_t* source, int source_width, int source_height, int source_channels,
                        int dest_x, int dest_y, int rotation)
{
    for (int sy = 0; sy < source_height; ++sy)
    {
        for (int sx = 0; sx < source_width; ++sx, source += source_channels)
        {
            // Skip copying texels with alpha=0
            if (source_channels == 4 && source[3] == 0)
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

static int apTexelNonZeroKernel(const uint8_t* image, uint32_t width, uint32_t height, uint32_t num_channels, int x, int y, int kernelsize)
{
    int start_x = x < kernelsize ? 0 : x - kernelsize;
    int start_y = y < kernelsize ? 0 : y - kernelsize;
    int end_x = (x + kernelsize) > (width-1) ? width-1 : x + kernelsize;
    int end_y = (y + kernelsize) > (height-1) ? height-1 : y + kernelsize;

    for (int yy = start_y; yy <= end_y; ++yy)
    {
        for (int xx = start_x; xx <= end_x; ++xx)
        {
            uint8_t color[] = { 255, 255, 255, 255 };

            int index = yy*width*num_channels + xx*num_channels;
            uint8_t bits = 0;
            for (int c = 0; c < num_channels; ++c)
            {
                color[c] = image[index+c];
                bits |= color[c];
            }

            if (num_channels == 4 && color[3] == 0)
                continue; // completely transparent

            if (bits)
            {
                return 1; // non zero
            }
        }
    }
    return 0;
}

static int apTexelAlphaNonZero(const uint8_t* image, uint32_t width, uint32_t height, uint32_t num_channels, int x, int y)
{
    int index = y*width*num_channels + x*num_channels;
    return image[index+3] > 0 ? 1 : 0;
}

apPosf apMathNormalize(apPosf a)
{
    float length = sqrtf(a.x*a.x + a.y*a.y);
    apPosf v = { a.x / length, a.y / length};
    return v;
}

float apMathDot(apPosf a, apPosf b)
{
    return a.x * b.x + a.y * b.y;
}

apPosf apMathSub(apPosf a, apPosf b)
{
    apPosf v = { a.x - b.x, a.y - b.y };
    return v;
}
apPosf apMathAdd(apPosf a, apPosf b)
{
    apPosf v = { a.x + b.x, a.y + b.y };
    return v;
}
apPosf apMathScale(apPosf a, float s)
{
    apPosf v = { a.x * s, a.y * s};
    return v;
}
apPosf apMathMul(apPosf a, apPosf b)
{
    apPosf v = { a.x * b.x, a.y * b.y };
    return v;
}
float apMathMax(float a, float b)
{
    return a > b ? a : b;
}
float apMathMin(float a, float b)
{
    return a < b ? a : b;
}
int apMathRoundUp(int x, int multiple)
{
    return ((x + multiple - 1) / multiple) * multiple;
}

static int apOverlapAxisTest2D(apPosf axis, const apPosf* a, int sizea, const apPosf* b, int sizeb)
{
    float mina = 100000.0f;
    float maxa = -100000.0f;
    float minb = 100000.0f;
    float maxb = -100000.0f;

    for (int j = 0; j < sizea; ++j)
    {
        float d = apMathDot(axis, a[j]);
        maxa = apMathMax(maxa, d);
        mina = apMathMin(mina, d);
    }

    for (int j = 0; j < sizeb; ++j)
    {
        float d = apMathDot(axis, b[j]);
        maxb = apMathMax(maxb, d);
        minb = apMathMin(minb, d);
    }

    if (maxa < minb || maxb < mina)
        return 0;
    return 1;
}

// Vertices are in CCW order
int apOverlapTest2D(const apPosf* a, int sizea, const apPosf* b, int sizeb)
{
    for (int i = 0; i < sizea; ++i)
    {
        apPosf diff = apMathNormalize(apMathSub(a[(i+1)%sizea], a[i]));
        apPosf n = { -diff.y, diff.x };
        if (!apOverlapAxisTest2D(n, a, sizea, b, sizeb))
            return 0;
    }
    for (int i = 0; i < sizeb; ++i)
    {
        apPosf diff = apMathNormalize(apMathSub(b[(i+1)%sizeb], b[i]));
        apPosf n = { -diff.y, diff.x };
        if (!apOverlapAxisTest2D(n, a, sizea, b, sizeb))
            return 0;
    }
    return 1;
}

uint8_t* apCreateHullImage(const uint8_t* image, uint32_t width, uint32_t height, uint32_t num_channels, int dilate)
{
    uint8_t* out = (uint8_t*)malloc(width*height);
    memset(out, 0, width*height);

    if (dilate)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int value = apTexelNonZeroKernel(image, width, height, num_channels, x, y, dilate);
                out[y*width + x] = value*255;
            }
        }
    }
    else {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int value = apTexelAlphaNonZero(image, width, height, num_channels, x, y);
                out[y*width + x] = value*255;
            }
        }
    }
    return out;
}
