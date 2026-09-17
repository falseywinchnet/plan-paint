#pragma once
#include "image.hpp"
namespace paint {
struct TextStyle {
    int size = 24;
    std::string face_path;
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
extern const unsigned char embedded_font_bold[];
extern const unsigned int embedded_font_bold_size;
extern const unsigned char embedded_font_italic[];
extern const unsigned int embedded_font_italic_size;
extern const unsigned char embedded_font_bolditalic[];
extern const unsigned int embedded_font_bolditalic_size;
extern const unsigned char embedded_font_mono[];
extern const unsigned int embedded_font_mono_size;
struct EmbeddedFont {
    const unsigned char* data = nullptr;
    unsigned int size = 0;
};
EmbeddedFont portsmouth_face(const TextStyle& style);
} // namespace paint
