#include "text.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <algorithm>
#include <cmath>
#include <fstream>
namespace paint {
static std::uint32_t next_codepoint(const std::string& text, std::size_t& cursor) {
    std::uint32_t value = static_cast<unsigned char>(text[cursor++]);
    if (value < 128) {
        return value;
    }
    int following = value < 224 ? 1 : value < 240 ? 2 : 3;
    value &= following == 1 ? 31 : following == 2 ? 15 : 7;
    for (int i = 0; i < following && cursor < text.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(text[cursor++]);
        if ((byte & 192) != 128) {
            return 0xfffd;
        }
        value = (value << 6) | (byte & 63);
    }
    return value;
}
void draw_text(Image& image, Point origin, const std::string& text, const TextStyle& style, Color color,
               Color background, const std::string& font_path) {
    std::vector<unsigned char> font_bytes;
    std::ifstream file(font_path, std::ios::binary | std::ios::ate);
    if (file) {
        std::streamoff size = file.tellg();
        if (size > 0 && size < 32000000) {
            font_bytes.resize(static_cast<std::size_t>(size));
            file.seekg(0);
            file.read(reinterpret_cast<char*>(font_bytes.data()), size);
        }
    }
    const unsigned char* data = font_bytes.empty() ? embedded_font : font_bytes.data();
    stbtt_fontinfo font{};
    if (!stbtt_InitFont(&font, data, stbtt_GetFontOffsetForIndex(data, 0))) {
        return;
    }
    float scale = stbtt_ScaleForPixelHeight(&font, static_cast<float>(style.size));
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
    int line_height = static_cast<int>(std::ceil((ascent - descent + line_gap) * scale));
    int baseline = static_cast<int>(origin.y + ascent * scale);
    int pen = static_cast<int>(origin.x);
    for (std::size_t cursor = 0; cursor < text.size();) {
        std::uint32_t codepoint = next_codepoint(text, cursor);
        if (codepoint == '\n') {
            pen = static_cast<int>(origin.x);
            baseline += line_height;
            continue;
        }
        int advance = 0, bearing = 0;
        stbtt_GetCodepointHMetrics(&font, static_cast<int>(codepoint), &advance, &bearing);
        int advance_pixels = static_cast<int>(std::round(advance * scale));
        if (style.opaque) {
            for (int y = baseline - static_cast<int>(ascent * scale);
                 y < baseline - static_cast<int>(descent * scale); ++y) {
                for (int x = pen; x < pen + advance_pixels; ++x) {
                    image.set(x, y, background);
                }
            }
        }
        int width = 0, height = 0, x_offset = 0, y_offset = 0;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, static_cast<int>(codepoint),
                                                         &width, &height, &x_offset, &y_offset);
        if (bitmap) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    Color pixel = color;
                    pixel.a = static_cast<std::uint8_t>(static_cast<unsigned>(color.a) *
                                                        bitmap[y * width + x] / 255);
                    image.blend(pen + x + x_offset, baseline + y + y_offset, pixel);
                }
            }
            stbtt_FreeBitmap(bitmap, nullptr);
        }
        if (style.underline || style.strikeout) {
            int y = style.underline ? baseline + 2 : baseline - static_cast<int>(ascent * scale * 0.35);
            for (int x = pen; x < pen + advance_pixels; ++x) {
                image.blend(x, y, color);
            }
        }
        pen += advance_pixels;
    }
}
} // namespace paint
