#pragma once
#include "image.hpp"
namespace paint {
// Native CONV*: double precision, premultiplied sRGB, separable nodal/basin plans.
// Input and output may alias; destination is replaced only after success.
void conv_resize(const Image& source, int width, int height, Image& destination);
} // namespace paint
