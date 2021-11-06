
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int STBI_write_png(char const *filename, int w, int h, int comp, const void *data, int stride_in_bytes)
{
    return stbi_write_png(filename, w, h, comp, data, stride_in_bytes);
}

unsigned char* STBI_load(char const *filename, int *x, int *y, int *channels_in_file)
{
    return stbi_load(filename, x, y, channels_in_file, 0);
}

