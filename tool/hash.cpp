// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker


#include "hash.h"

#include <string.h>
#include <xxhash.h>

static const hash_t g_Seed = 0x12345678;

uint64_t Hash(void* buffer, uint32_t size)
{
    return XXH64(buffer, (size_t)size, g_Seed);
}

uint64_t Hash(const char* str)
{
    if (!str)
        return 0;
    return Hash((void*)str, (uint32_t)strlen(str));
}
