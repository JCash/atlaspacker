#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> // qsort
#include <string.h> // strcmp

#include <atlaspacker/atlaspacker.h>

#include "utils.h"
#include "render.h"

#if defined(_WIN32)
    #include "win32/dirent.h"
#else
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <dirent.h>
#endif

#include <stb_wrappers.h>

uint64_t GetTime()
{
    return apGetTime();
}

Image* CreateImage(const char* path, uint32_t color, int w, int h, int c)
{
    Image* image = (Image*)malloc(sizeof(Image));
    image->data = (uint8_t*)malloc((uint32_t)(w * h * c));
    image->width = w;
    image->height = h;
    image->channels = c;
    uint8_t* p = image->data;
    for (int i = 0; i < w*h; ++i)
    {
        for (int j = 0; j < c; ++j)
        {
            *p++ = (color >> (j*8)) & 0xFF;
        }
    }
    image->path = path;
    return image;
}

Image* LoadImage(const char* path)
{
    Image* image = (Image*)malloc(sizeof(Image));
    image->data = STBI_load(path, &image->width, &image->height, &image->channels);
    if (!image->data)
    {
        free((void*)image);
        printf("Failed to load %s\n", path);
        return 0;
    }
    image->path = strdup(path);
    return image;
}

void DestroyImage(Image* image)
{
    free((void*)image->path);
    free((void*)image->data);
    free((void*)image);
}


static int CompareImages(const Image** _a, const Image** _b)
{
    const Image* a = *_a;
    const Image* b = *_b;
    int a_w = a->width;
    int a_h = a->height;
    int b_w = b->width;
    int b_h = b->height;
    int area_a = a_w * a_h;
    int area_b = b_w * b_h;

    int max_a = a_w > a_h ? a_w : a_h;
    int min_a = a_w < a_h ? a_w : a_h;
    int max_b = b_w > b_h ? b_w : b_h;
    int min_b = b_w < b_h ? b_w : b_h;

    float square_a = ((float)max_a / (float)min_a) * (float)area_a;
    float square_b = ((float)max_b / (float)min_b) * (float)area_b;
    if (square_a == square_b)
    {
        return strcmp(a->path, b->path);
    }
    return (square_a <= square_b) ? 1 : -1;
}

typedef int (*QsortFn)(const void*, const void*);
void SortImages(Image** images, int num_images)
{
    qsort(images, (size_t)num_images, sizeof(images[0]), (QsortFn)CompareImages);
}


// typedef void (*FPageIterateCallback)(void* ctx, apImage* image);
// void apPageIterateImages(apContext* ctx, int page, FPageIterateCallback iter, void* iter_ctx)

typedef struct
{
    int width;
    int height;
    int channels;
    uint8_t* data;
} OutputImageCtx;

static void DrawTriangles(int width, int height, int channels, uint8_t* data,
                           apPos pos, apSize size, const apPosf* vertices, int num_vertices)
{

// printf("image pos: %d, %d\n", pos.x, pos.y);
// printf("image size: %d, %d\n", size.width, size.height);

    const uint8_t color[4] = {255,0,255,255};
    for (int i = 0; i < num_vertices/3; ++i, vertices += 3)
    {
        apPos p0 = { vertices[0].x, vertices[0].y };
        apPos p1 = { vertices[1].x, vertices[1].y };
        apPos p2 = { vertices[2].x, vertices[2].y };
        draw_line(p0.x, p0.y, p1.x, p1.y, data, width, height, channels, color);
        draw_line(p1.x, p1.y, p2.x, p2.y, data, width, height, channels, color);
        draw_line(p2.x, p2.y, p0.x, p0.y, data, width, height, channels, color);
    }
}

int DebugWriteOutput(apContext* ctx, const char* pattern)
{
    int num_pages = apGetNumPages(ctx);
    for (int i = 0; i < num_pages; ++i)
    {
        apPage* page = apGetPage(ctx, i);

        int width = page->dimensions.width;
        int height = page->dimensions.height;
        int channels = ctx->num_channels;

        uint32_t size = width * height * channels;
        uint8_t* output = (uint8_t*)malloc(size);
        memset(output, 0, size);

        //uint8_t* output = apRenderPage(ctx, i, &width, &height, &channels);

// DEBUG BACKGROUND RENDERING
    int tile_size = 16;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int tx = x / tile_size;
            int ty = y / tile_size;
            int odd = ((tx&1) && !(ty&1)) | (!(tx&1) && (ty&1));

            // uint8_t color_odd[4] = {255,255,255,128};
            // uint8_t color_even[4] = {0,0,0,128};
            uint8_t color_odd[4] = {32,32,32,255};
            uint8_t color_even[4] = {16,16,16,255};
            // uint8_t color_odd[4] = {64,96,64,255};
            // uint8_t color_even[4] = {32,64,32,255};

            uint8_t* color = color_even;

            if (odd)
                color = color_odd;

            for (int i = 0; i < channels; ++i)
                output[y * (width*channels) + (x*channels) + i ] = color[i];
        }
    }


        apImage* image = apPageGetFirstImage(page);
        while(image)
        {
            apCopyRGBA(output, width, height, channels,
                    image->data, image->width, image->height, image->channels,
                    image->placement.pos.x, image->placement.pos.y, image->rotation);

            apSize size = { image->width, image->height };
            DrawTriangles(width, height, channels, output,
                            image->placement.pos, size,
                            image->vertices, image->num_vertices);

            image = image->next;
        }

        // we use tga here to remove the compression time from the tests
        char path[64];
        snprintf(path, sizeof(path), "image_%s_%d.tga", pattern, i);
        int result = STBI_write_tga(path, width, height, channels, output);
        if (result)
            printf("Wrote %s at %d x %d\n", path, width, height);
    }
    return 1;
}

void DebugDrawHull(const apPosf* vertices, int num_vertices, uint8_t* color, const Image* image, uint8_t* data)
{
    for (int i = 0; i < num_vertices; ++i)
    {
        int j = (i+1)%num_vertices;
        apPosf p0 = vertices[i];
        apPosf p1 = vertices[j];
        p0.x = (p0.x + 0.5f) * (float)image->width * 0.999f;
        p0.y = (p0.y + 0.5f) * (float)image->height * 0.999f;
        p1.x = (p1.x + 0.5f) * (float)image->width * 0.999f;
        p1.y = (p1.y + 0.5f) * (float)image->height * 0.999f;

        draw_line((int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, data, image->width, image->height, image->channels, color);
    }
}

void DebugDrawTriangles(const apPosf* triangles, int num_triangles, uint8_t* color, const Image* image, uint8_t* data)
{
    for (int i = 0; i < num_triangles; ++i, ++triangles)
    {
        apPosf p0 = triangles[0];
        apPosf p1 = triangles[1];
        apPosf p2 = triangles[2];

        p0.x = (p0.x + 0.5f) * (float)image->width * 0.999f;
        p0.y = (p0.y + 0.5f) * (float)image->height * 0.999f;
        p1.x = (p1.x + 0.5f) * (float)image->width * 0.999f;
        p1.y = (p1.y + 0.5f) * (float)image->height * 0.999f;
        p2.x = (p2.x + 0.5f) * (float)image->width * 0.999f;
        p2.y = (p2.y + 0.5f) * (float)image->height * 0.999f;

        draw_line((int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, data, image->width, image->height, image->channels, color);
        draw_line((int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, data, image->width, image->height, image->channels, color);
        draw_line((int)p2.x, (int)p2.y, (int)p0.x, (int)p0.y, data, image->width, image->height, image->channels, color);
    }
}


int IterateFiles(const char* dirpath, int recursive, int (*callback)(void* ctx, const char*), void* ctx)
{
    struct dirent* entry = 0;
    DIR* dir = 0;
    dir = opendir(dirpath);
    if (!dir) {
        return 0;
    }

    while( (entry = readdir(dir)) )
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char abs_path[1024];
        snprintf(abs_path, sizeof(abs_path), "%s/%s", dirpath, entry->d_name);

        struct stat path_stat;
        stat(abs_path, &path_stat);

        int isdir = S_ISDIR(path_stat.st_mode);
        if (!isdir)
            callback(ctx, abs_path);

        if (isdir && recursive) {

            // Make sure the directory still exists (the callback might have removed it!)
            int should_continue = IterateFiles(abs_path, recursive, callback, ctx);
            if (!should_continue) {
                goto cleanup;
            }
        }
    }

cleanup:
    closedir(dir);
    return 1;
}

void DebugPrintTileImage(uint32_t width, uint32_t height, uint8_t* data)
{
    printf("IMAGE: %u %u\n", width, height);
    for (int y = 0; y < height; ++y)
    {
        printf("    ");
        for (int x = 0; x < width; ++x)
        {
            int value = data[y*width+x];
            if (value < 10)
                printf("%d", value);
            else
                printf("X");
            if ((x%8) == 7)
                printf(" ");
        }
        printf("\n");
    }
    printf("\n");
}

static int IsSubImageNonEmpty(int tile_size, int x, int y,
                                int width, int height, int alphathreshold, const uint8_t* data)
{
    int channels = 4;
    for (int yy = y; yy < y + tile_size && yy < height; ++yy)
    {
        for (int xx = x; xx < x + tile_size && xx < width; ++xx)
        {
            int index = yy * width * channels + xx * channels;
            if (data[index+3] > alphathreshold)
                return 1;
        }
    }
    return 0;
}

static void ConvertImageToTiles(uint32_t tile_size, int alphathreshold,
                                            int width, int height, int channels, const uint8_t* src_image,
                                            int twidth, int theight, uint8_t* timage)
{
    (void)theight;
    for (int y = 0, ty = 0; y < height; y += tile_size, ++ty)
    {
        for (int x = 0, tx = 0; x < width; x += tile_size, ++tx)
        {
            // Check area in image
            int nonempty = 1; // always visible for RGB images
            if (channels == 4) // only for alpha images, we'll do a visibility check
                nonempty = IsSubImageNonEmpty(tile_size, x, y, width, height, alphathreshold, src_image);

            timage[ty*twidth+tx] = nonempty;
        }
    }
}

uint8_t* CreateTileImage(Image* image, uint32_t tile_size, int alphathreshold, int* twidth, int* theight)
{
    *twidth = apMathRoundUp(image->width, tile_size) / tile_size;
    *theight = apMathRoundUp(image->height, tile_size) / tile_size;
    uint8_t* timage = (uint8_t*)malloc(*twidth * *theight);
    ConvertImageToTiles(tile_size, alphathreshold, image->width, image->height, image->channels, image->data, *twidth, *theight, timage);
    return timage;
}
