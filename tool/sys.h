// License: MIT
// @2021-@2024 Mathias Westerdahl

#ifndef ATLASPACKER_TOOL_SYS_H
#define ATLASPACKER_TOOL_SYS_H

#include <stdint.h>

const char* GetApplicationPath(char* buffer, uint32_t buffer_size);

const char* GetWorkingDir(char* buffer, uint32_t buffer_size);

// Parses an environment variable of the format NAME=folder1,folder2
// and calls the function for each path
void GetEnvVarDirs(const char* name, void (*fn)(void* ctx, const char* path), void *ctx);

#endif // ATLASPACKER_TOOL_SYS_H
