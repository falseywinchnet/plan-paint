#include "document.hpp"
#include "conv.hpp"
#include <algorithm>
#include <stdexcept>
namespace paint {
void FloatingSelection::composite_onto(Image& target) const {
    // Putting an untouched lifted selection back must preserve its original
    // RGBA bytes, including feathered edges and invisible RGB under alpha zero.
    bool original = source && x == (*source).x && y == (*source).y && image.width == (*source).image.width &&
                    image.height == (*source).image.height && coverage.size() == image.pixels.size();
    for (std::size_t index = 0; original && index < image.pixels.size(); ++index) {
        Color expected = (*source).image.pixels[index];
        if ((*source).feathered) {
            expected.a = static_cast<std::uint8_t>((expected.a * coverage[index] + 127) / 255);
        }
        if (!coverage[index] || ((*source).transparent && equal(expected, (*source).secondary))) {
            expected = {0, 0, 0, 0};
        }
        original = equal(expected, image.pixels[index]);
    }
    if (!original) {
        composite(target, image, x, y);
        return;
    }
    for (int row = 0; row < image.height; ++row) {
        for (int column = 0; column < image.width; ++column) {
            const std::size_t index = static_cast<std::size_t>(row) * image.width + column;
            if (coverage[index]) {
                target.set(x + column, y + row, (*source).image.pixels[index]);
            }
        }
    }
}
Document::Document() {
    image.reset(960, 640);
}
bool Document::dirty() const {
    return revision != saved_revision || selection.active;
}
std::size_t Snapshot::bytes() const {
    // Counting shared bases conservatively keeps the history bound predictable.
    return image.pixels.size() * sizeof(Color) + atlas.bytes() + path.nodes.size() * sizeof(Point) +
           path.segments.size() * sizeof(PathSegment) + path.runs.size() * sizeof(PathRun) +
           (path.base ? (*path.base).pixels.size() * sizeof(Color) : 0) +
           (curve.base ? (*curve.base).pixels.size() * sizeof(Color) : 0);
}
void Document::checkpoint() {
    Snapshot snapshot{image, atlas, revision, path, curve};
    history_bytes += snapshot.bytes();
    undo_history.push_back(std::move(snapshot));
    redo_history.clear();
    while (history_bytes > 256u * 1024u * 1024u && undo_history.size() > 1) {
        history_bytes -= undo_history.front().bytes();
        undo_history.pop_front();
    }
    revision = next_revision++;
}
void Document::undo() {
    if (undo_history.empty()) {
        return;
    }
    commit_selection();
    redo_history.push_back({std::move(image), std::move(atlas), revision, path, curve});
    history_bytes -= undo_history.back().bytes();
    // Escape and tool changes end a session permanently. Later image undo must
    // not resurrect its controls; undo within that session keeps them editable.
    restore_path(undo_history.back().path);
    restore_curve(undo_history.back().curve);
    image = std::move(undo_history.back().image);
    atlas = std::move(undo_history.back().atlas);
    ++atlas_epoch;
    revision = undo_history.back().revision;
    undo_history.pop_back();
}
void Document::redo() {
    if (redo_history.empty()) {
        return;
    }
    selection = {};
    undo_history.push_back({std::move(image), std::move(atlas), revision, path, curve});
    history_bytes += undo_history.back().bytes();
    restore_path(redo_history.back().path);
    restore_curve(redo_history.back().curve);
    image = std::move(redo_history.back().image);
    atlas = std::move(redo_history.back().atlas);
    ++atlas_epoch;
    revision = redo_history.back().revision;
    redo_history.pop_back();
}
void Document::replace(Image replacement, const std::string& path_name) {
    image = std::move(replacement);
    filename = path_name;
    atlas = {};
    ++atlas_epoch;
    selection = {};
    path = {};
    curve = {};
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
    commit_curve();
    checkpoint();
    if (atlas.kind == AtlasKind::None && (pasted.width > image.width || pasted.height > image.height)) {
        Image larger;
        larger.reset(std::max(image.width, pasted.width), std::max(image.height, pasted.height),
                     ink.secondary);
        composite(larger, image, 0, 0);
        image = std::move(larger);
    }
    selection = {pasted, x, y, true, std::vector<std::uint8_t>(pasted.pixels.size(), 1), {}, {}};
    tool = Tool::Select;
}
void Document::select(Rect bounds, const std::vector<Point>& lasso) {
    require_rgba_transform();
    commit_path();
    commit_curve();
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
    const std::shared_ptr<const SelectionSource> source = std::make_shared<SelectionSource>(
        SelectionSource{lifted, bounds.x, bounds.y, false, transparent_selection, ink.secondary});
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
    selection = {std::move(lifted), bounds.x, bounds.y, true, std::move(coverage), {}, source};
    selection.outline = lasso;
    for (Point& point : selection.outline) {
        point.x -= bounds.x;
        point.y -= bounds.y;
    }
}
void Document::select_mask(const SelectionMask& mask) {
    if (mask.bounds.w < 1 || mask.bounds.h < 1 ||
        mask.coverage.size() != static_cast<std::size_t>(mask.bounds.w) * mask.bounds.h) {
        return;
    }
    require_rgba_transform();
    commit_path();
    commit_curve();
    commit_selection();
    Image lifted = cropped(image, mask.bounds);
    const std::shared_ptr<const SelectionSource> source = std::make_shared<SelectionSource>(
        SelectionSource{lifted, mask.bounds.x, mask.bounds.y, true, false, ink.secondary});
    checkpoint();
    for (int y = 0; y < mask.bounds.h; ++y) {
        for (int x = 0; x < mask.bounds.w; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * mask.bounds.w + x;
            const std::uint8_t coverage = mask.coverage[index];
            Color color = lifted.pixels[index];
            color.a = static_cast<std::uint8_t>((color.a * coverage + 127) / 255);
            if (!coverage) {
                color = {0, 0, 0, 0};
            }
            lifted.pixels[index] = color;
            if (coverage) {
                image.set(mask.bounds.x + x, mask.bounds.y + y, ink.secondary);
            }
        }
    }
    selection = {std::move(lifted), mask.bounds.x, mask.bounds.y, true, mask.coverage, mask.outline, source};
}
void Document::edit_selection(const std::vector<Point>& polygon, bool subtract) {
    if (polygon.size() < 3) {
        return;
    }
    SelectionMask mask;
    mask.bounds = {0, 0, image.width, image.height};
    mask.coverage.assign(image.pixels.size(), 0);
    const bool feathered = selection.source && (*selection.source).feathered;
    if (selection.active) {
        for (int y = 0; y < selection.image.height; ++y) {
            for (int x = 0; x < selection.image.width; ++x) {
                const int px = selection.x + x, py = selection.y + y;
                if (!image.contains(px, py)) {
                    continue;
                }
                const std::size_t index = static_cast<std::size_t>(y) * selection.image.width + x;
                const std::uint8_t coverage = selection.coverage.empty() ? 255 : selection.coverage[index];
                mask.coverage[static_cast<std::size_t>(py) * image.width + px] = feathered  ? coverage
                                                                                 : coverage ? 255
                                                                                            : 0;
            }
        }
    }
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            if (inside_polygon(polygon, x + 0.5, y + 0.5)) {
                mask.coverage[static_cast<std::size_t>(y) * image.width + x] = subtract ? 0 : 255;
            }
        }
    }
    // Reunite untouched lifted RGBA first, then lift the edited union. This also
    // keeps subtraction from erasing pixels or darkening a feathered boundary.
    commit_selection();
    trim_selection_mask(mask);
    select_mask(mask);
}
void Document::commit_selection() {
    if (!selection.active) {
        return;
    }
    selection.composite_onto(image);
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
    selection = {std::move(lifted), 0, 0, true, std::move(coverage), {}, {}};
}
void Document::crop() {
    if (atlas.kind == AtlasKind::Sheet) {
        throw std::runtime_error(
            "Leave Atlas before cropping the sheet. Sprite dimensions belong to the grid.");
    }
    if (!selection.active) {
        return;
    }
    Image replacement = selection.image;
    selection = {};
    image = std::move(replacement);
}
void Document::resize(int width, int height, bool scale) {
    require_rgba_transform();
    commit_path();
    commit_curve();
    if (!selection.active && atlas.kind == AtlasKind::Sheet &&
        (width != image.width || height != image.height)) {
        throw std::runtime_error(
            "Leave Atlas before resizing the sheet. Resize a selection to scale artwork inside a sprite.");
    }
    if (!selection.active && (atlas.kind == AtlasKind::Icon || atlas.kind == AtlasKind::Cursor) &&
        (width > 256 || height > 256)) {
        throw std::runtime_error("ICO and CUR images must be at most 256 by 256 pixels.");
    }
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
    commit_path();
    commit_curve();
    if (selection.active) {
        selection.image = rotate_quarter(selection.image, turns);
        selection.coverage.clear();
        selection.outline.clear();
    } else {
        sync_atlas();
        Image replacement = rotate_quarter(image, turns);
        checkpoint();
        if ((atlas.kind == AtlasKind::Icon || atlas.kind == AtlasKind::Cursor) && atlas.active >= 0) {
            IconFrame& frame = atlas.icons[atlas.active];
            int normalized = (turns % 4 + 4) % 4;
            for (int i = 0; i < normalized; ++i) {
                int x = frame.hotspot_x;
                frame.hotspot_x = frame.image.height - 1 - frame.hotspot_y;
                frame.hotspot_y = x;
                std::swap(frame.image.width, frame.image.height);
            }
            frame.image = image;
            if (!frame.xor_pixels.empty()) {
                Image mask = image;
                mask.pixels = frame.xor_pixels;
                frame.xor_pixels = rotate_quarter(mask, turns).pixels;
            }
            frame.image = replacement;
        }
        assign_canvas(std::move(replacement));
    }
}
void Document::flip(bool horizontal) {
    commit_path();
    commit_curve();
    if (selection.active) {
        selection.image = flipped(selection.image, horizontal);
        selection.coverage.clear();
        selection.outline.clear();
    } else {
        sync_atlas();
        Image replacement = flipped(image, horizontal);
        checkpoint();
        if ((atlas.kind == AtlasKind::Icon || atlas.kind == AtlasKind::Cursor) && atlas.active >= 0) {
            IconFrame& frame = atlas.icons[atlas.active];
            if (horizontal) {
                frame.hotspot_x = image.width - 1 - frame.hotspot_x;
            } else {
                frame.hotspot_y = image.height - 1 - frame.hotspot_y;
            }
            if (!frame.xor_pixels.empty()) {
                Image mask = image;
                mask.pixels = frame.xor_pixels;
                frame.xor_pixels = flipped(mask, horizontal).pixels;
            }
            frame.image = replacement;
        }
        image = std::move(replacement);
    }
}
void Document::invert_colors() {
    commit_path();
    commit_curve();
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
Ink Document::primary_ink() const {
    Ink result = ink;
    result.alternate.reset();
    result.transparent_pattern = true;
    if (alt_enabled() && !alt_carries_body) {
        result.alternate = std::make_shared<const Ink>(alternate_ink());
    }
    return result;
}
bool Document::alt_enabled() const {
    return !solid_material(ink);
}
Ink Document::body_ink() const {
    return alt_carries_body && alt_enabled() ? alternate_ink() : primary_ink();
}
Ink Document::alternate_ink() const {
    Ink result = alt_ink;
    result.primary = ink.secondary;
    result.secondary = ink.primary;
    result.alternate.reset();
    result.secondary.a = 0;
    result.transparent_pattern = true;
    if (!alt_enabled()) {
        result.pattern = Pattern::None;
        result.brush = Brush::Round;
    }
    result.size = ink.size;
    result.smooth = ink.smooth;
    return result;
}
void Document::commit_path() {
    sync_path();
    path = {};
}
void Document::restore_path(const EditablePath& previous) {
    if (path.session == 0 || path.session != previous.session) {
        std::uint64_t session = path.session;
        path = {};
        // Undo may pass through edits predating the path. Keep the empty live
        // session so Redo can reach it again, without reviving older sessions.
        path.session = session;
        return;
    }
    path = previous;
    if (!path.nodes.empty()) {
        ink = path.ink;
        ink.alternate.reset();
        alt_carries_body = path.alt_carries_body;
        alt_ink = path.alternate;
        shape_outline = path.outline;
        shape_fill = path.fill;
        continuous_path = path.continuous;
        shape_fill_brush = path.fill_brush;
    }
}
void Document::add_path_node(Point point) {
    commit_curve();
    if (path.session == 0) {
        path.session = next_path_session++;
    }
    sync_path();
    checkpoint();
    if (!path.base) {
        path.base = std::make_shared<const Image>(image);
    }
    if (!path.extending) {
        path.start = path.nodes.size();
    }
    path.nodes.push_back(point);
    path.extending = true;
    sync_path();
}
void Document::end_path_geometry() {
    if (!path.extending) {
        return;
    }
    if (path.nodes.size() - path.start == 1) {
        // An unplaced line owns no geometry: discard its initial anchor as well.
        path.nodes.resize(path.start);
        if (!undo_history.empty() && undo_history.back().path.nodes.size() == path.start &&
            undo_history.back().path.session == path.session) {
            revision = undo_history.back().revision;
            history_bytes -= undo_history.back().bytes();
            undo_history.pop_back();
        }
        path.extending = false;
        image = path_image();
        return;
    }
    sync_path();
    path.runs.push_back({path.start, path.nodes.size() - path.start, path.ink, path.alternate, path.outline,
                         path.fill, path.continuous, path.fill_brush, path.body, path.alt_carries_body});
    path.extending = false;
}
void Document::move_path_node(std::size_t index, Point point) {
    if (index >= path.nodes.size()) {
        return;
    }
    Point previous = path.nodes[index];
    // Snapped nodes are one junction: all connected runs follow its movement.
    for (Point& node : path.nodes) {
        if (node.x == previous.x && node.y == previous.y) {
            node = point;
        }
    }
    for (std::size_t segment = 0; segment < path.segments.size(); ++segment) {
        PathSegment& edge = path.segments[segment];
        if (edge.first >= path.nodes.size() || edge.last >= path.nodes.size()) {
            continue;
        }
        const Point first = path.nodes[edge.first], last = path.nodes[edge.last];
        edge.geometry.first_control.x += first.x - edge.geometry.start.x;
        edge.geometry.first_control.y += first.y - edge.geometry.start.y;
        edge.geometry.second_control.x += last.x - edge.geometry.end.x;
        edge.geometry.second_control.y += last.y - edge.geometry.end.y;
        edge.geometry.start = first;
        edge.geometry.end = last;
    }
    sync_path();
}
Image Document::path_image(const Point* next) const {
    if (!path.base) {
        return image;
    }
    Image result = *path.base;
    for (const PathRun& saved : path.runs) {
        std::vector<Point> run = path_contour(saved.start, saved.count, !saved.continuous);
        Ink material = saved.ink;
        polygon(result, run, material, saved.outline, saved.fill, !saved.continuous, saved.body.brush,
                &saved.body);
    }
    if (path.extending) {
        std::vector<Point> run =
            path_contour(path.start, path.nodes.size() - path.start, !continuous_path && !next);
        if (next) {
            run.push_back(*next);
        }
        if (run.size() > 1) {
            Ink body = body_ink();
            polygon(result, run, primary_ink(), shape_outline, shape_fill, !continuous_path, body.brush,
                    &body);
        }
    }
    return result;
}
void Document::sync_path() {
    if (!path.base) {
        return;
    }
    if (path.extending) {
        path.ink = primary_ink();
        path.alternate = alt_ink;
        path.body = body_ink();
        path.alt_carries_body = alt_carries_body;
        path.outline = shape_outline;
        path.fill = shape_fill;
        path.continuous = continuous_path;
        path.fill_brush = shape_fill_brush;
    }
    Image rendered = path_image();
    if (!std::equal(image.pixels.begin(), image.pixels.end(), rendered.pixels.begin(), rendered.pixels.end(),
                    equal)) {
        image = std::move(rendered);
        // A live path can be saved without releasing its nodes. Subsequent
        // appearance changes must make that saved document dirty again.
        if (revision == saved_revision) {
            revision = next_revision++;
        }
    }
}
Image Document::visible_image() const {
    Image result = image;
    if (selection.active) {
        selection.composite_onto(result);
    }
    return result;
}
} // namespace paint
