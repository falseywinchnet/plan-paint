#pragma once
#include "image.hpp"
namespace paint {
Image load_image(const std::string& path);
void save_image(const Image& image, const std::string& path, int quality = 95);
std::vector<std::uint8_t> encode_bmp(const Image& image);
std::vector<std::uint8_t> encode_png(const Image& image);
Image decode_image(const void* data, std::size_t size);
} // namespace paint
