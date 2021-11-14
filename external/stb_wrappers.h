#pragma once

int STBI_write_png(char const *filename, int w, int h, int comp, const void *data, int stride_in_bytes);
int STBI_write_tga(char const *filename, int w, int h, int comp, const void *data);
unsigned char* STBI_load(char const *filename, int *x, int *y, int *channels_in_file);
