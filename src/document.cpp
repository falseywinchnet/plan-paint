#include "document.hpp"
#include "conv.hpp"
#include <algorithm>
namespace paint {
Document::Document() {
    image.reset(960, 640);
}
bool Document::dirty() const {
    return revision != saved_revision || selection.active || !path.empty();
}
void Document::checkpoint() {
    Snapshot snapshot{image, revision};
    history_bytes += snapshot.image.pixels.size() * sizeof(Color);
    undo_history.push_back(std::move(snapshot));
    redo_history.clear();
    while (history_bytes > 256u * 1024u * 1024u && undo_history.size() > 1) {
        history_bytes -= undo_history.front().image.pixels.size() * sizeof(Color);
        undo_history.pop_front();
    }
    revision = next_revision++;
}
void Document::undo() {
    if (undo_history.empty()) {
        return;
    }
    commit_selection();
    commit_path();
    redo_history.push_back({std::move(image), revision});
    history_bytes -= undo_history.back().image.pixels.size() * sizeof(Color);
    image = std::move(undo_history.back().image);
    revision = undo_history.back().revision;
    undo_history.pop_back();
}
void Document::redo() {
    if (redo_history.empty()) {
        return;
    }
    selection = {};
    path.clear();
    history_bytes += image.pixels.size() * sizeof(Color);
    undo_history.push_back({std::move(image), revision});
    image = std::move(redo_history.back().image);
    revision = redo_history.back().revision;
    redo_history.pop_back();
}
void Document::replace(Image replacement, const std::string& path_name) {
    image = std::move(replacement);
    filename = path_name;
    selection = {};
    path.clear();
    undo_history.clear();
    redo_history.clear();
    history_bytes = 0;
    revision = 0;
    saved_revision = 0;
    next_revision = 1;
}
void Document::new_image(int width, int height) {
    Image replacement;
    replacement.reset(width, height);
    replace(std::move(replacement), "");
}
void Document::paste(const Image& pasted, int x, int y) {
    commit_selection();
    commit_path();
    checkpoint();
    if (pasted.width > image.width || pasted.height > image.height) {
        Image larger;
        larger.reset(std::max(image.width, pasted.width), std::max(image.height, pasted.height),
                     ink.secondary);
        composite(larger, image, 0, 0);
        image = std::move(larger);
    }
    selection = {pasted, x, y, true, std::vector<std::uint8_t>(pasted.pixels.size(), 1), {}};
    tool = Tool::Select;
}
void Document::select(Rect bounds, const std::vector<Point>& lasso) {
    commit_selection();
    int right = std::clamp(bounds.x + bounds.w, 0, image.width);
    int bottom = std::clamp(bounds.y + bounds.h, 0, image.height);
    bounds.x = std::clamp(bounds.x, 0, image.width);
    bounds.y = std::clamp(bounds.y, 0, image.height);
    bounds.w = right - bounds.x;
    bounds.h = bottom - bounds.y;
    if (bounds.w < 1 || bounds.h < 1) {
        return;
    }
    Image lifted = cropped(image, bounds);
    std::vector<std::uint8_t> coverage(lifted.pixels.size(), 0);
    checkpoint();
    for (int y = 0; y < bounds.h; ++y) {
        for (int x = 0; x < bounds.w; ++x) {
            bool inside = lasso.empty() || inside_polygon(lasso, bounds.x + x + 0.5, bounds.y + y + 0.5);
            coverage[static_cast<std::size_t>(y) * bounds.w + x] = inside ? 1 : 0;
            if (!inside) {
                lifted.set(x, y, {0, 0, 0, 0});
            } else {
                if (transparent_selection && equal(lifted.get(x, y), ink.secondary)) {
                    lifted.set(x, y, {0, 0, 0, 0});
                }
                image.set(bounds.x + x, bounds.y + y, ink.secondary);
            }
        }
    }
    selection = {std::move(lifted), bounds.x, bounds.y, true, std::move(coverage), {}};
    selection.outline = lasso;
    for (Point& point : selection.outline) {
        point.x -= bounds.x;
        point.y -= bounds.y;
    }
}
void Document::commit_selection() {
    if (!selection.active) {
        return;
    }
    composite(image, selection.image, selection.x, selection.y);
    selection = {};
}
void Document::delete_selection() {
    if (selection.active) {
        selection = {};
    }
}
void Document::select_all() {
    select({0, 0, image.width, image.height});
}
void Document::invert_selection() {
    if (!selection.active) {
        select_all();
        return;
    }
    Image full = visible_image();
    std::vector<std::uint8_t> coverage(full.pixels.size(), 1);
    for (int y = 0; y < selection.image.height; ++y) {
        for (int x = 0; x < selection.image.width; ++x) {
            int dx = x + selection.x, dy = y + selection.y;
            if (!full.contains(dx, dy)) {
                continue;
            }
            std::size_t index = static_cast<std::size_t>(y) * selection.image.width + x;
            bool selected = selection.coverage.empty() || selection.coverage[index] != 0;
            if (selected) {
                coverage[static_cast<std::size_t>(dy) * full.width + dx] = 0;
            }
        }
    }
    commit_selection();
    checkpoint();
    Image lifted = full;
    for (std::size_t index = 0; index < full.pixels.size(); ++index) {
        if (coverage[index]) {
            image.pixels[index] = ink.secondary;
        } else {
            lifted.pixels[index] = {0, 0, 0, 0};
        }
    }
    selection = {std::move(lifted), 0, 0, true, std::move(coverage), {}};
}
void Document::crop() {
    if (!selection.active) {
        return;
    }
    Image replacement = selection.image;
    selection = {};
    image = std::move(replacement);
}
void Document::resize(int width, int height, bool scale) {
    const Image& input = selection.active ? selection.image : image;
    Image replacement;
    if (scale) {
        conv_resize(input, width, height, replacement);
    } else {
        replacement.reset(width, height, ink.secondary);
        composite(replacement, input, 0, 0);
    }
    if (selection.active) {
        selection.image = std::move(replacement);
        selection.coverage.clear();
        selection.outline.clear();
    } else {
        checkpoint();
        image = std::move(replacement);
    }
}
void Document::rotate(int turns) {
    if (selection.active) {
        selection.image = rotate_quarter(selection.image, turns);
        selection.coverage.clear();
        selection.outline.clear();
    } else {
        Image replacement = rotate_quarter(image, turns);
        checkpoint();
        image = std::move(replacement);
    }
}
void Document::flip(bool horizontal) {
    if (selection.active) {
        selection.image = flipped(selection.image, horizontal);
        selection.coverage.clear();
        selection.outline.clear();
    } else {
        Image replacement = flipped(image, horizontal);
        checkpoint();
        image = std::move(replacement);
    }
}
void Document::invert_colors() {
    if (!selection.active) {
        checkpoint();
    }
    Image& target = selection.active ? selection.image : image;
    for (Color& pixel : target.pixels) {
        pixel.r = 255 - pixel.r;
        pixel.g = 255 - pixel.g;
        pixel.b = 255 - pixel.b;
    }
}
void Document::commit_path() {
    if (path.size() > 1) {
        polygon(image, path, ink, shape_outline, shape_fill, !continuous_path, shape_fill_brush);
    }
    path.clear();
}
Image Document::visible_image() const {
    Image result = image;
    if (selection.active) {
        composite(result, selection.image, selection.x, selection.y);
    }
    return result;
}
} // namespace paint
