#include "conv.hpp"
#include "document.hpp"
#include <algorithm>
#include <stdexcept>
namespace paint {
bool Document::has_legacy_xor() const {
    if ((atlas.kind != AtlasKind::Icon && atlas.kind != AtlasKind::Cursor) || atlas.active < 0) {
        return false;
    }
    const IconFrame& frame = atlas.icons[atlas.active];
    if (image.width != frame.image.width || image.height != frame.image.height) {
        return false;
    }
    for (std::size_t i = 0; i < image.pixels.size(); ++i) {
        if (legacy_xor_pixel(frame, i) && equal(image.pixels[i], {255, 255, 255, 0})) {
            return true;
        }
    }
    return false;
}
void Document::require_rgba_transform() const {
    if (has_legacy_xor()) {
        throw std::runtime_error(
            "This legacy cursor contains background-dependent XOR pixels. Paint over or erase those pixels "
            "before selecting or resampling artwork. Pixel-exact flips and quarter-turns preserve them.");
    }
}
bool Document::fixed_canvas() const {
    return atlas.kind == AtlasKind::Sheet;
}
void Document::assign_canvas(Image replacement) {
    commit_path();
    bool icon = atlas.kind == AtlasKind::Icon || atlas.kind == AtlasKind::Cursor;
    if ((fixed_canvas() || (icon && (replacement.width > 256 || replacement.height > 256))) &&
        (replacement.width != image.width || replacement.height != image.height)) {
        Image bounded;
        bounded.reset(image.width, image.height, {0, 0, 0, 0});
        composite(bounded, replacement, (bounded.width - replacement.width) / 2,
                  (bounded.height - replacement.height) / 2);
        image = std::move(bounded);
    } else {
        image = std::move(replacement);
    }
}
void Document::replace_container(ImageContainer replacement, const std::string& path_name) {
    if (replacement.frames.empty()) {
        throw std::runtime_error("The image container is empty.");
    }
    int best = 0;
    for (std::size_t i = 1; i < replacement.frames.size(); ++i) {
        if (replacement.frames[i].image.width * replacement.frames[i].image.height >
            replacement.frames[best].image.width * replacement.frames[best].image.height) {
            best = static_cast<int>(i);
        }
    }
    replace(replacement.frames[best].image, path_name);
    if (replacement.kind != ContainerKind::Image) {
        atlas.kind = replacement.kind == ContainerKind::Cursor ? AtlasKind::Cursor : AtlasKind::Icon;
        atlas.icons = std::move(replacement.frames);
        atlas.active = best;
        ++atlas_epoch;
    }
}
void Document::sync_atlas() {
    if (atlas.kind == AtlasKind::None) {
        return;
    }
    if (atlas.kind == AtlasKind::Sheet) {
        if (atlas.active < 0) {
            if (!std::equal(image.pixels.begin(), image.pixels.end(), atlas.sheet.pixels.begin(),
                            atlas.sheet.pixels.end(), equal)) {
                atlas.sheet = image;
                ++atlas_epoch;
            }
            return;
        }
        Rect rect = atlas.grid.frame(atlas.sheet, atlas.active);
        if (image.width != rect.w || image.height != rect.h) {
            throw std::runtime_error("Sprite dimensions no longer match the Atlas grid.");
        }
        bool changed = false;
        for (int y = 0; y < rect.h; ++y) {
            for (int x = 0; x < rect.w; ++x) {
                Color value = image.get(x, y);
                if (!equal(value, atlas.sheet.get(rect.x + x, rect.y + y))) {
                    atlas.sheet.set(rect.x + x, rect.y + y, value);
                    changed = true;
                }
            }
        }
        if (changed) {
            ++atlas_epoch;
        }
    } else if (atlas.active >= 0) {
        IconFrame& frame = atlas.icons[atlas.active];
        if (image.width != frame.image.width || image.height != frame.image.height ||
            !std::equal(image.pixels.begin(), image.pixels.end(), frame.image.pixels.begin(),
                        frame.image.pixels.end(), equal)) {
            if (image.width > 256 || image.height > 256) {
                throw std::runtime_error("ICO/CUR images cannot exceed 256 by 256 pixels.");
            }
            if (image.width != frame.image.width || image.height != frame.image.height) {
                frame.xor_pixels.clear();
            }
            frame.image = image;
            frame.hotspot_x = std::clamp(frame.hotspot_x, 0, image.width - 1);
            frame.hotspot_y = std::clamp(frame.hotspot_y, 0, image.height - 1);
            ++atlas_epoch;
        }
    }
}
void Document::configure_atlas(const AtlasGrid& grid) {
    commit_selection();
    commit_path();
    sync_atlas();
    Image sheet = atlas.kind == AtlasKind::Sheet ? atlas.sheet : image;
    grid.validate(sheet);
    std::uint64_t previous_revision = revision;
    checkpoint();
    revision = previous_revision;
    atlas = {};
    atlas.kind = AtlasKind::Sheet;
    atlas.sheet = std::move(sheet);
    atlas.grid = grid;
    atlas.active = 0;
    image = atlas.frame_image(0);
    ++atlas_epoch;
}
void Document::leave_atlas() {
    commit_selection();
    commit_path();
    sync_atlas();
    std::uint64_t previous_revision = revision;
    checkpoint();
    if (atlas.kind == AtlasKind::Sheet) {
        revision = previous_revision;
    }
    if (atlas.kind == AtlasKind::Sheet) {
        image = std::move(atlas.sheet);
    }
    atlas = {};
    ++atlas_epoch;
}
void Document::atlas_select(int index, bool control) {
    if (atlas.kind == AtlasKind::None || index < -1 || index >= atlas.count() ||
        (index < 0 && atlas.kind != AtlasKind::Sheet)) {
        return;
    }
    commit_selection();
    commit_path();
    sync_atlas();
    if (control && index >= 0) {
        if (atlas.sequence.empty() && atlas.active >= 0 && atlas.active != index) {
            atlas.sequence.push_back(atlas.active);
        }
        std::vector<int>::iterator found = std::find(atlas.sequence.begin(), atlas.sequence.end(), index);
        if (found == atlas.sequence.end()) {
            atlas.sequence.push_back(index);
        } else {
            atlas.sequence.erase(found);
        }
        std::sort(atlas.sequence.begin(), atlas.sequence.end());
    } else {
        atlas.sequence.clear();
    }
    atlas.active = index;
    image = index < 0 ? atlas.sheet : atlas.frame_image(index);
}
void Document::atlas_step(int direction) {
    if (atlas.count() < 1) {
        return;
    }
    std::vector<int> sequence = atlas.sequence;
    int next = 0;
    if (sequence.empty()) {
        next = (std::max(0, atlas.active) + direction + atlas.count()) % atlas.count();
    } else {
        std::vector<int>::iterator found = std::find(sequence.begin(), sequence.end(), atlas.active);
        int current =
            found == sequence.end() ? (direction > 0 ? -1 : 0) : static_cast<int>(found - sequence.begin());
        next = sequence[(current + direction + static_cast<int>(sequence.size())) % sequence.size()];
    }
    atlas_select(next, false);
    atlas.sequence = std::move(sequence);
}
void Document::make_icon_sizes(const std::vector<int>& sizes, bool cursor) {
    if (sizes.empty()) {
        throw std::runtime_error("Choose at least one icon size.");
    }
    commit_selection();
    commit_path();
    sync_atlas();
    ImageContainer result;
    result.kind = cursor ? ContainerKind::Cursor : ContainerKind::Icon;
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        if (sizes[i] < 1 || sizes[i] > 256) {
            throw std::runtime_error("Icon sizes must be between 1 and 256 pixels.");
        }
        IconFrame frame;
        conv_resize(image, sizes[i], sizes[i], frame.image);
        result.frames.push_back(std::move(frame));
    }
    checkpoint();
    atlas = {};
    atlas.kind = cursor ? AtlasKind::Cursor : AtlasKind::Icon;
    atlas.icons = std::move(result.frames);
    atlas.active = static_cast<int>(atlas.icons.size()) - 1;
    image = atlas.icons.back().image;
    ++atlas_epoch;
}
void Document::set_hotspot(int x, int y) {
    if (atlas.kind != AtlasKind::Cursor || atlas.active < 0) {
        return;
    }
    x = std::clamp(x, 0, image.width - 1);
    y = std::clamp(y, 0, image.height - 1);
    IconFrame& frame = atlas.icons[atlas.active];
    if (frame.hotspot_x == x && frame.hotspot_y == y) {
        return;
    }
    checkpoint();
    frame.hotspot_x = x;
    frame.hotspot_y = y;
    ++atlas_epoch;
}
Image Document::output_image() const {
    Image current = visible_image();
    if (atlas.kind != AtlasKind::Sheet || atlas.active < 0) {
        return current;
    }
    Image result = atlas.sheet;
    Rect rect = atlas.grid.frame(result, atlas.active);
    for (int y = 0; y < rect.h; ++y) {
        for (int x = 0; x < rect.w; ++x) {
            result.set(rect.x + x, rect.y + y, current.get(x, y));
        }
    }
    return result;
}
ImageContainer Document::output_container(bool cursor) const {
    ImageContainer result;
    result.kind = cursor ? ContainerKind::Cursor : ContainerKind::Icon;
    if (atlas.kind == AtlasKind::Icon || atlas.kind == AtlasKind::Cursor) {
        result.frames = atlas.icons;
        if (atlas.active >= 0) {
            result.frames[atlas.active].image = visible_image();
        }
    } else {
        result.frames.push_back({visible_image(), 0, 0, {}});
    }
    return result;
}
} // namespace paint
