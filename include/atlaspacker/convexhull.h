// https://github.com/JCash/atlaspacker
// @2021 Mathias Westerdahl

#pragma once

#include <stdint.h>
#include <atlaspacker/atlaspacker.h>

// May return 0 if it couldn't calculate a hull (i.e. if the image is empty)
apPosf* apConvexHullFromImage(int num_planes, uint8_t* image, int width, int height, int* num_vertices);
