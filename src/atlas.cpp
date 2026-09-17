#include "atlas.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
namespace paint {
void AtlasGrid::validate(const Image& sheet) const {
    if (columns < 1 || rows < 1 || columns > 4096 || rows > 4096 || columns * rows > 4096 || margin_x < 0 ||
        margin_y < 0 || spacing_x < 0 || spacing_y < 0 || margin_x > sheet.width / 2 ||
        margin_y > sheet.height / 2 || spacing_x > sheet.width || spacing_y > sheet.height) {
        throw std::runtime_error("Choose a grid with 1 to 4096 sprites and nonnegative margins and spacing.");
    }
    if (sheet.width - 2 * margin_x - (columns - 1) * spacing_x < columns ||
        sheet.height - 2 * margin_y - (rows - 1) * spacing_y < rows) {
        throw std::runtime_error("The rows, columns, margins and spacing do not fit this image.");
    }
}
Rect AtlasGrid::frame(const Image& sheet, int index) const {
    validate(sheet);
    if (index < 0 || index >= columns * rows) {
        throw std::runtime_error("Sprite index is outside the grid.");
    }
    int width = (sheet.width - 2 * margin_x - (columns - 1) * spacing_x) / columns;
    int height = (sheet.height - 2 * margin_y - (rows - 1) * spacing_y) / rows;
    return {margin_x + (index % columns) * (width + spacing_x),
            margin_y + (index / columns) * (height + spacing_y), width, height};
}
int AtlasState::count() const {
    return kind == AtlasKind::Sheet ? grid.columns * grid.rows : static_cast<int>(icons.size());
}
Image AtlasState::frame_image(int index) const {
    if (index < 0 || index >= count()) {
        throw std::runtime_error("Atlas frame is unavailable.");
    }
    if (kind == AtlasKind::Sheet) {
        return cropped(sheet, grid.frame(sheet, index));
    }
    return icons[index].image;
}
std::size_t AtlasState::bytes() const {
    std::size_t result = sheet.pixels.size() * sizeof(Color);
    for (std::size_t i = 0; i < icons.size(); ++i) {
        result += (icons[i].image.pixels.size() + icons[i].xor_pixels.size()) * sizeof(Color);
    }
    return result;
}
bool sprite_sheet_filename(const std::string& filename) {
    std::u8string stem =
        std::filesystem::path(std::u8string(filename.begin(), filename.end())).stem().u8string();
    std::string name;
    for (std::size_t i = 0; i < stem.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(stem[i]);
        name += std::isalnum(c) ? static_cast<char>(std::tolower(c)) : ' ';
    }
    name = " " + name + " ";
    const char* tokens[] = {"sprite", "sprites", "spritesheet", "spritesheets", "spritemap", "sheet",
                            "atlas",  "tileset", "tilemap",     "strip",        "animation", "anim",
                            "idle",   "walk",    "run",         "attack"};
    for (std::size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); ++i) {
        if (name.find(std::string(" ") + tokens[i] + " ") != std::string::npos) {
            return true;
        }
    }
    return false;
}
bool legacy_xor_pixel(const IconFrame& frame, std::size_t index) {
    return index < frame.xor_pixels.size() && frame.xor_pixels[index].a &&
           equal(frame.image.pixels[index], {255, 255, 255, 0});
}
} // namespace paint
