#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> // qsort
#include <string.h> // strcmp

#include <atlaspacker/atlaspacker.h>

#include "utils.h"
#include "render.h"

#if defined(_WIN32)
    #include "win32/dirent.h"
    #include <Windows.h> // QPC
#else
    #include <sys/time.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <dirent.h>
#endif

#include <stb_wrappers.h>

uint64_t GetTime()
{
#if defined(_WIN32)
    LARGE_INTEGER tickPerSecond;
    LARGE_INTEGER tick;
    QueryPerformanceFrequency(&tickPerSecond);
    QueryPerformanceCounter(&tick);
    return (uint64_t)(tick.QuadPart / (tickPerSecond.QuadPart / 1000000));
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint64_t)(tv.tv_sec) * 1000000U + (uint64_t)(tv.tv_usec);
#endif
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
    return (square_a <= square_b) ? 1 : -1;
}

typedef int (*QsortFn)(const void*, const void*);
void SortImages(Image** images, int num_images)
{
    qsort(images, (size_t)num_images, sizeof(images[0]), (QsortFn)CompareImages);
}

int DebugWriteOutput(apContext* ctx, const char* pattern)
{
    int num_pages = apGetNumPages(ctx);
    for (int i = 0; i < num_pages; ++i)
    {
        int width, height, channels;
        uint8_t* output = apRenderPage(ctx, i, &width, &height, &channels);
        if (!output)
        {
            return 0;
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
