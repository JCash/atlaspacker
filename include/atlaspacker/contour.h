// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2023 Mathias Westerdahl

#pragma once

#include <stdint.h>

struct apPosf;

// Input is a 1-channel black and white tile image
// Returns a triangle list that encompasses the original image (3-tuples of vertices)
// Supports concave shapes or "holes" in the image
// Vertices are in normalized space (-0.5,-0.5) to (0.5,0.5), with (0,0) in the center of the image
apPosf* apContourFromImage(uint8_t* image, int width, int height, int* num_vertices);
