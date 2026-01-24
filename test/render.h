// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

#pragma once

#include <stdint.h>

void draw_line(int x0, int y0, int x1, int y1, uint8_t* image, int width, int height, int nchannels, const uint8_t* color);
