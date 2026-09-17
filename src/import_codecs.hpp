#pragma once
#include "image.hpp"
namespace paint {
void reject_animation(const std::uint8_t* data, std::size_t size);
bool is_avif(const void* data, std::size_t size);
Image decode_avif(const void* data, std::size_t size);
Image rasterize_svg(const std::string& path, int width = 0, int height = 0);
Image decode_native_heif(const void* data, std::size_t size);
} // namespace paint
