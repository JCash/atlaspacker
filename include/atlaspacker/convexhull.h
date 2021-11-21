// https://github.com/JCash/atlaspacker
// @2021 Mathias Westerdahl

#pragma once

#include <stdint.h>
#include <atlaspacker/atlaspacker.h>

apPosf* apConvexHullFromImage(int num_planes, uint8_t* image, int width, int height, int* num_vertices);
