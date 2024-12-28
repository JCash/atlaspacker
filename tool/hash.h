// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#pragma once

#include <stdint.h>

typedef uint64_t hash_t;

hash_t Hash(const char* str);
hash_t Hash(void* buffer, uint32_t size);
