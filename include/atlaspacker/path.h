#pragma once

#include <stddef.h>

void apPathSplitProjectPath(const char* project_path, char* out_dir, size_t out_dir_size,
                            char* out_name, size_t out_name_size);

void apPathResolveStringPatterns(char* buffer, size_t buffer_size, const char* pattern,
                                 const char** key_value_pairs, int num_pairs);

void apPathNormalize(char* path);
