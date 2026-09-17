#pragma once
#include "image.hpp"
namespace paint {
struct TextStyle {
    int size = 24;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikeout = false;
    bool mono = false;
    bool opaque = false;
};
void draw_text(Image& image, Point origin, const std::string& text, const TextStyle& style, Color color,
               Color background, const std::string& font_path);
extern const unsigned char embedded_font[];
extern const unsigned int embedded_font_size;
} // namespace paint
