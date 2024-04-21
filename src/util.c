// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> // qsort
#include <string.h> // strcmp

#include <atlaspacker/util.h>
#include <atlaspacker/atlaspacker.h>

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
    image->next = 0;
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
    free((void*)image->source);
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

int IsFile(const char* path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

int IsDir(const char* path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

int IterateFiles(const char* dirpath, int recursive, int (*callback)(void* ctx, const char*), void* ctx)
{
    struct dirent* entry = 0;
    DIR* dir = 0;
    dir = opendir(dirpath);
    if (!dir) {
        fprintf(stderr, "Failed to open dir: '%s'\n", dirpath);
        return 0;
    }

    while( (entry = readdir(dir)) )
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char abs_path[2048];
        snprintf(abs_path, sizeof(abs_path), "%s/%s", dirpath, entry->d_name);

        int isdir = IsDir(abs_path);
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


uint8_t* ReadFile(const char* path, uint32_t* file_size)
{
	FILE* file = fopen(path, "rb");
	if (!file)
	{
		return 0;
	}

	fseek(file, 0, SEEK_END);
	uint32_t length = (uint32_t)ftell(file);
	fseek(file, 0, SEEK_SET);

	uint8_t* data = (uint8_t*)malloc(length);
	fread(data, 1, length, file);
	fclose(file);

	return data;
}


Page* apRenderPages(apContext* context, int* num_pages, uint32_t debug_color)
{
    int channels = context->num_channels;
    *num_pages = apGetNumPages(context);

    Page* pages = (Page*)malloc(*num_pages * sizeof(Page));

    for (int i = 0; i < *num_pages; ++i)
    {
        apPage* page = apGetPage(context, i);

        int width = page->dimensions.width;
        int height = page->dimensions.height;

        uint32_t size = width * height * channels;
        uint8_t* output = (uint8_t*)malloc(size);
        memset(output, 0, size);

        pages[i].width = width;
        pages[i].height = height;
        pages[i].channels = channels;
        pages[i].data = output;

        // TODO: Let the atlas packer render the page, together with the debug info
        //uint8_t* output = apRenderPage(ctx, page, debug_color);

// DEBUG BACKGROUND RENDERING
        if (debug_color)
        {
            // Ask the packer to create an empty debug image of correct size
            // We need this in order to properly render debug data that only the packer knows
            // E.g. the tile size
            // apPackerDebugDrawBackground(output, width, height, debug_color);
        }
        // int tile_size = 16;
        // for (int y = 0; y < height; ++y)
        // {
        //     for (int x = 0; x < width; ++x)
        //     {
        //         int tx = x / tile_size;
        //         int ty = y / tile_size;
        //         int odd = ((tx&1) && !(ty&1)) | (!(tx&1) && (ty&1));

        //         // uint8_t color_odd[4] = {255,255,255,128};
        //         // uint8_t color_even[4] = {0,0,0,128};
        //         uint8_t color_odd[4] = {32,32,32,255};
        //         uint8_t color_even[4] = {16,16,16,255};
        //         // uint8_t color_odd[4] = {64,96,64,255};
        //         // uint8_t color_even[4] = {32,64,32,255};

        //         uint8_t* color = color_even;

        //         if (odd)
        //             color = color_odd;

        //         for (int i = 0; i < channels; ++i)
        //             output[y * (width*channels) + (x*channels) + i ] = color[i];
        //     }
        // }


        apImage* image = apPageGetFirstImage(page);
        while(image)
        {
            apCopyRGBA(output, width, height, channels,
                    image->data, image->width, image->height, image->channels,
                    image->placement.pos.x, image->placement.pos.y, image->rotation);

            // TODO: Add this to the debug part of the rendering
            // apSize size = { image->width, image->height };
            // DrawTriangles(width, height, channels, output,
            //                 image->placement.pos, size,
            //                 image->vertices, image->num_vertices);

            image = image->next;
        }

        // // we use tga here to remove the compression time from the tests
        // char path[64];
        // snprintf(path, sizeof(path), "image_%s_%d.tga", pattern, i);
        // int result = STBI_write_tga(path, width, height, channels, output);
        // if (result)
        //     printf("Wrote %s at %d x %d\n", path, width, height);
    }
    return pages;
}
