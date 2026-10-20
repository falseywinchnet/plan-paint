#pragma once
#include "image.hpp"
namespace paint {
enum class ContainerKind { Image, Icon, Cursor };
struct IconFrame {
    Image image;
    int hotspot_x = 0, hotspot_y = 0;
    // Legacy AND/XOR pixels have no background-independent RGBA equivalent.
    // Nonzero entries preserve the XOR color; their canvas pixels use a transparent white sentinel.
    std::vector<Color> xor_pixels;
};
struct ImageContainer {
    ContainerKind kind = ContainerKind::Image;
    std::vector<IconFrame> frames;
};
enum class IconSizePolicy { AddMissing, ReplaceSelected, ExactProfile };
struct IconSizeStep {
    int existing_index = -1;
    int size = 0;
    bool generate = false;
};
struct IconSizePlan {
    std::vector<IconSizeStep> steps;
    int kept = 0, added = 0, replaced = 0, removed = 0;
};
// Exact profiles keep every matching representation, including duplicate sizes.
// Replacement affects each matching representation; unrelated images stay intact.
IconSizePlan plan_icon_sizes(const std::vector<IconFrame>& existing, const std::vector<int>& sizes,
                             IconSizePolicy policy);
struct AtlasGrid {
    int columns = 1, rows = 1;
    int margin_x = 0, margin_y = 0, spacing_x = 0, spacing_y = 0;
    Rect frame(const Image& sheet, int index) const;
    void validate(const Image& sheet) const;
};
enum class AtlasKind { None, Sheet, Icon, Cursor };
struct AtlasState {
    AtlasKind kind = AtlasKind::None;
    Image sheet;
    AtlasGrid grid;
    std::vector<IconFrame> icons;
    int active = -1;
    std::vector<int> sequence;
    int count() const;
    Image frame_image(int index) const;
    std::size_t bytes() const;
};
bool sprite_sheet_filename(const std::string& filename);
bool legacy_xor_pixel(const IconFrame& frame, std::size_t index);
} // namespace paint
