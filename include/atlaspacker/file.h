// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

#ifndef ATLASPACKER_FILE_H
#define ATLASPACKER_FILE_H

#include <stdint.h>

int IsFile(const char* path);
int IsDir(const char* path);
int IterateFiles(const char* dirpath, int recursive, int (*callback)(void* ctx, const char*), void* ctx);

// Memory is allocated using malloc()
uint8_t* ReadFile(const char* path, uint32_t* file_size);

#endif // ATLASPACKER_FILE_H
