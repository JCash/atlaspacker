
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

int STBI_write_png(char const *filename, int w, int h, int comp, const void *data, int stride_in_bytes)
{
    return stbi_write_png(filename, w, h, comp, data, stride_in_bytes);
}
