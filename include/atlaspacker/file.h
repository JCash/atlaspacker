// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#pragma once

#include <stdint.h>

int IsFile(const char* path);
int IsDir(const char* path);
int IterateFiles(const char* dirpath, int recursive, int (*callback)(void* ctx, const char*), void* ctx);

// Memory is allocated using malloc()
uint8_t* ReadFile(const char* path, uint32_t* file_size);
