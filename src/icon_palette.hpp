#pragma once
#include "atlas.hpp"
namespace paint {
enum class IconPaletteDither { None, FloydSteinberg };
struct IconPaletteOptions {
    unsigned bits_per_pixel = 8;
    unsigned alpha_threshold = 128;
    Color matte{255, 255, 255, 255};
    IconPaletteDither dither = IconPaletteDither::None;
};
struct IconPaletteResult {
    IconFrame frame;
    std::vector<Color> palette;
    std::size_t transparent_pixels = 0, partial_pixels = 0;
};
// Independent weighted median-cut in encoded RGB; see docs/ICON_EDITING.md.
// Builds an indexed DIB snapshot retained until the result's pixels are edited.
IconPaletteResult convert_icon_palette(const IconFrame& source, const IconPaletteOptions& options);
} // namespace paint
