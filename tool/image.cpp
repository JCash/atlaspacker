// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#include "image.h"

#include <stdio.h>  // printf
#include <stdlib.h> // qsort
#include <string.h> // memset

extern "C" {
    #include <stb_wrappers.h>
}

static Image* AllocImage()
{
    Image* image = (Image*)malloc(sizeof(Image));
    memset(image, 0, sizeof(*image));
    return image;
}

Image* CreateImage(const char* path, uint32_t color, int w, int h, int c)
{
    Image* image = AllocImage();
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
    image->path_hash = Hash(image->path);
    return image;
}

Image* LoadImage(const char* path)
{
    Image* image = AllocImage();
    image->data = STBI_load(path, &image->width, &image->height, &image->channels);
    if (!image->data)
    {
        free((void*)image);
        printf("Failed to load %s\n", path);
        return 0;
    }
    image->path = strdup(path);
    image->path_hash = Hash(image->path);
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
