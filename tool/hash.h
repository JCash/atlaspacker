// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

#ifndef ATLASPACKER_TOOL_HASH_H
#define ATLASPACKER_TOOL_HASH_H

#include <stdint.h>

typedef uint64_t hash_t;

hash_t Hash(const char* str);
hash_t Hash(void* buffer, uint32_t size);

#endif // ATLASPACKER_TOOL_HASH_H
