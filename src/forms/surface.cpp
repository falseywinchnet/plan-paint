#include "forms/editor.hpp"
#include "raster.hpp"
#include <algorithm>
#include <cmath>
namespace paint::forms {
namespace gf = gui_forms;
void PaintCanvas::on_attached_to_window() {
    RasterCanvas::on_attached_to_window();
    update_backing();
}
void PaintCanvas::update_backing() {
    const std::shared_ptr<Editor> editor = editor_.lock();
    const CanvasBacking choice = editor ? (*editor).settings.canvas_backing : CanvasBacking::PaleFelt;
    if (!attached_window() || (backing_.value && choice == loaded_backing_)) {
        return;
    }
    const Image texture = canvas_backing_texture(choice);
    std::vector<std::byte> pixels(texture.pixels.size() * 4);
    for (std::size_t index = 0; index < texture.pixels.size(); ++index) {
        const Color color = texture.pixels[index];
        pixels[index * 4] = static_cast<std::byte>(color.b);
        pixels[index * 4 + 1] = static_cast<std::byte>(color.g);
        pixels[index * 4 + 2] = static_cast<std::byte>(color.r);
        pixels[index * 4 + 3] = std::byte{255};
    }
    const gf::ImageLoadResult result =
        backing_.value
            ? (*attached_window())
                  .replace_bgra32_premultiplied(backing_, texture.width, texture.height, texture.width * 4,
                                                pixels, *this)
            : (*attached_window())
                  .load_bgra32_premultiplied(texture.width, texture.height, texture.width * 4, pixels);
    if (result) {
        backing_ = result.image;
        loaded_backing_ = choice;
    }
}
void PaintCanvas::on_detaching_from_window(gf::Window& former_window) noexcept {
    if (backing_.value) {
        static_cast<void>(former_window.remove_image(backing_));
    }
    if (repeated_.value) {
        static_cast<void>(former_window.remove_image(repeated_));
    }
    if (reference_.value) {
        static_cast<void>(former_window.remove_image(reference_));
    }
    repeated_ = {};
    reference_ = {};
    atlas_revision_ = 0;
    backing_ = {};
    loaded_backing_ = CanvasBacking::Count;
    RasterCanvas::on_detaching_from_window(former_window);
}
void PaintCanvas::on_dispose() noexcept {
    if (backing_.value && window()) {
        static_cast<void>((*window()).remove_image(backing_));
    }
    if (window() && repeated_.value) {
        static_cast<void>((*window()).remove_image(repeated_));
    }
    if (window() && reference_.value) {
        static_cast<void>((*window()).remove_image(reference_));
    }
    repeated_ = {};
    reference_ = {};
    atlas_revision_ = 0;
    backing_ = {};
    loaded_backing_ = CanvasBacking::Count;
    RasterCanvas::on_dispose();
}
void PaintCanvas::on_paint(gf::Painter& painter, gf::Rect damage) {
    gf::Rect bounds = client_rectangle();
    const std::shared_ptr<Editor> editor = editor_.lock();
    update_backing();
    if (backing_.value) {
        const double tile = canvas_backing_tile_size(loaded_backing_);
        const double pixels = loaded_backing_ == CanvasBacking::PaleFelt ? 128 : 512;
        painter.fill_image_pattern(backing_, {pixels, pixels}, bounds, {tile, tile});
    } else {
        painter.fill_rect(bounds, gf::Color::rgba(208, 218, 226));
    }
    const gf::GradientStop light[] = {{0, gf::Color::rgba(255, 255, 255, 22)},
                                      {1, gf::Color::rgba(60, 77, 95, 15)}};
    painter.fill_linear_gradient(bounds, {0, 0}, {0, bounds.height}, light);
    std::shared_ptr<gui_drawing::Bitmap> image = bitmap();
    if (image) {
        gf::Rect sheet =
            bitmap_to_client({0, 0, static_cast<int>((*image).width()), static_cast<int>((*image).height())});
        painter.draw_box_shadow(sheet, 0, {1.5, 2.5}, 6, 0, gf::Color::rgba(42, 53, 67, 75));
        painter.stroke_rect({sheet.x - 0.5, sheet.y - 0.5, sheet.width + 1, sheet.height + 1},
                            gf::Color::rgba(93, 111, 130, 155), 1);
    }
    RasterCanvas::on_paint(painter, damage);
    if (editor) {
        paint_atlas_context(painter, *editor);
    }
}
namespace {
gf::ImageId update_atlas_image(gf::Window& window, gf::Control& owner, gf::ImageId previous,
                               const Image& image) {
    if (image.pixels.empty()) {
        if (previous.value) {
            static_cast<void>(window.remove_image(previous));
        }
        return {};
    }
    std::vector<std::byte> pixels(image.pixels.size() * 4);
    for (std::size_t index = 0; index < image.pixels.size(); ++index) {
        const Color color = image.pixels[index];
        pixels[index * 4] = static_cast<std::byte>((color.b * color.a + 127) / 255);
        pixels[index * 4 + 1] = static_cast<std::byte>((color.g * color.a + 127) / 255);
        pixels[index * 4 + 2] = static_cast<std::byte>((color.r * color.a + 127) / 255);
        pixels[index * 4 + 3] = static_cast<std::byte>(color.a);
    }
    const gf::ImageLoadResult result =
        previous.value ? window.replace_bgra32_premultiplied(previous, image.width, image.height,
                                                             image.width * 4, pixels, owner)
                       : window.load_bgra32_premultiplied(image.width, image.height, image.width * 4, pixels);
    return result ? result.image : previous;
}
} // namespace
void PaintCanvas::paint_atlas_context(gf::Painter& painter, const Editor& editor) {
    if (editor.document.atlas.kind == AtlasKind::None || editor.document.atlas.active < 0) {
        return;
    }
    if (atlas_revision_ != editor.canvas_revision) {
        repeated_ = update_atlas_image(*window(), *this, repeated_,
                                       editor.atlas_wrap ? editor.document.image : Image{});
        reference_ = update_atlas_image(*window(), *this, reference_, editor.atlas_reference);
        atlas_revision_ = editor.canvas_revision;
    }
    const int width = editor.document.image.width, height = editor.document.image.height;
    if (repeated_.value) {
        painter.save();
        painter.clip_rect(client_rectangle());
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                if (x == 0 && y == 0) {
                    continue;
                }
                const gf::Rect bounds = bitmap_to_client({x * width, y * height, width, height});
                painter.fill_rect(bounds, gf::Color::rgba(242, 242, 242));
                painter.draw_image(repeated_, bounds);
            }
        }
        painter.stroke_rect(bitmap_to_client({0, 0, width, height}), gf::Color::rgba(34, 107, 172, 155), 1);
        painter.restore();
    }
    if (reference_.value) {
        painter.draw_image(reference_, bitmap_to_client({0, 0, width, height}), 0.5);
    }
}
void Editor::paint_tool_preview(gf::Painter& painter) {
    if (cursor_client_ && document.tool == Tool::Brush && brush_family == BrushFamily::Heal) {
        const gf::Point center = *cursor_client_;
        const double radius = std::max(2.0, document.ink.size * canvas().zoom() * 0.5);
        const gf::Color white = gf::Color::rgba(255, 255, 255), blue = gf::Color::rgba(25, 98, 162);
        painter.stroke_rounded_rect({center.x - radius, center.y - radius, radius * 2, radius * 2}, radius,
                                    white, 3);
        painter.stroke_rounded_rect({center.x - radius, center.y - radius, radius * 2, radius * 2}, radius,
                                    blue, 1);
        if (healing_brush_.has_source()) {
            const gui_drawing::PointF point = canvas().client_to_bitmap(center);
            const gf::Point source = screen(healing_brush_.source_for({point.x, point.y}));
            painter.draw_line({source.x - 7, source.y}, {source.x + 7, source.y}, white, 3);
            painter.draw_line({source.x, source.y - 7}, {source.x, source.y + 7}, white, 3);
            painter.draw_line({source.x - 7, source.y}, {source.x + 7, source.y}, blue, 1);
            painter.draw_line({source.x, source.y - 7}, {source.x, source.y + 7}, blue, 1);
        }
        return;
    }
    if (!cursor_client_ ||
        (document.tool != Tool::Pencil && document.tool != Tool::Eraser && document.tool != Tool::Magnifier &&
         !(document.tool == Tool::Picker && picker_magnifier))) {
        return;
    }
    gui_drawing::PointF point = (*canvas_).client_to_bitmap(*cursor_client_);
    int x = static_cast<int>(std::floor(point.x)), y = static_cast<int>(std::floor(point.y));
    if (!document.image.contains(x, y)) {
        return;
    }
    if (document.tool == Tool::Magnifier || document.tool == Tool::Picker) {
        const double radius = 64;
        const gf::Point center = *cursor_client_;
        const gf::Rect lens{center.x - radius, center.y - radius, radius * 2, radius * 2};
        painter.draw_box_shadow(lens, radius, {1, 2}, 5, 0, gf::Color::rgba(30, 40, 55, 90));
        // Scanline intersections with the circle clip every enlarged pixel,
        // including edge pixels, without an off-center rectangular lens.
        for (int row = -64; row < 64; ++row) {
            const double half = std::sqrt(radius * radius - (row + 0.5) * (row + 0.5));
            const int py = static_cast<int>(std::floor(point.y + (row + 0.5) / 8));
            for (int column = -64; column < 64; column += 1) {
                const double left = std::max(static_cast<double>(column), -half);
                const double right = std::min(static_cast<double>(column + 1), half);
                if (right <= left) {
                    continue;
                }
                const int px = static_cast<int>(std::floor(point.x + (column + 0.5) / 8));
                Color color = document.image.get(px, py);
                const int checker = ((px / 4 + py / 4) & 1) ? 225 : 255;
                const double alpha = color.a / 255.0;
                if (document.selection.active && !document.selection.on_canvas) {
                    const Color selected =
                        document.selection.image.get(px - document.selection.x, py - document.selection.y);
                    const double sa = selected.a / 255.0;
                    color.r = static_cast<std::uint8_t>(selected.r * sa +
                                                        (color.r * alpha + checker * (1 - alpha)) * (1 - sa));
                    color.g = static_cast<std::uint8_t>(selected.g * sa +
                                                        (color.g * alpha + checker * (1 - alpha)) * (1 - sa));
                    color.b = static_cast<std::uint8_t>(selected.b * sa +
                                                        (color.b * alpha + checker * (1 - alpha)) * (1 - sa));
                } else {
                    color.r = static_cast<std::uint8_t>(color.r * alpha + checker * (1 - alpha));
                    color.g = static_cast<std::uint8_t>(color.g * alpha + checker * (1 - alpha));
                    color.b = static_cast<std::uint8_t>(color.b * alpha + checker * (1 - alpha));
                }
                painter.fill_rect({center.x + left, center.y + row, right - left, 1},
                                  gf::Color::rgba(color.r, color.g, color.b));
            }
        }
        painter.stroke_rounded_rect(lens, radius, gf::Color::rgba(45, 90, 140), 1.5);
        painter.draw_line({center.x - 5, center.y}, {center.x + 5, center.y},
                          gf::Color::rgba(255, 255, 255, 200), 1);
        painter.draw_line({center.x, center.y - 5}, {center.x, center.y + 5},
                          gf::Color::rgba(30, 40, 55, 200), 1);
        painter.draw_text_utf8({center.x - 8, center.y + 51}, "8×", {gf::FontRole::control, 12, 600, false},
                               gf::Color::rgba(35, 60, 90));
        return;
    }
    double scale = (*canvas_).zoom();
    painter.save();
    painter.clip_rect((*canvas_).bitmap_to_client({0, 0, document.image.width, document.image.height}));
    if (document.tool == Tool::Pencil && !dragging_) {
        Color color = patterned(pencil_ink(document.primary_ink()), x, y);
        gf::Rect pixel = (*canvas_).bitmap_to_client({x, y, 1, 1});
        painter.fill_rect(pixel, gf::Color::rgba(color.r, color.g, color.b, color.a));
        if (scale >= 4) {
            painter.stroke_rect(pixel, gf::Color::rgba(255, 255, 255, 230), 1);
            painter.stroke_rect({pixel.x - 1, pixel.y - 1, pixel.width + 2, pixel.height + 2},
                                gf::Color::rgba(30, 30, 30, 220), 1);
        }
    } else if (document.tool == Tool::Eraser) {
        double radius = std::max(0.5, document.ink.size * 0.5) * scale;
        gf::Point center = *cursor_client_;
        gf::Rect circle{center.x - radius, center.y - radius, radius * 2, radius * 2};
        if (eraser_soft) {
            double previous = 0;
            for (int ring = 32; ring > 0; --ring) {
                double fraction = (ring - 0.5) / 32;
                double alpha = 0.42 * std::sqrt(1 - fraction * fraction);
                int layer = static_cast<int>(255 * (alpha - previous) / (1 - previous));
                double r = radius * ring / 32;
                painter.fill_rounded_rect({center.x - r, center.y - r, r * 2, r * 2}, r,
                                          gf::Color::rgba(245, 65, 118, static_cast<std::uint8_t>(layer)));
                previous = alpha;
            }
        } else {
            painter.fill_rounded_rect(circle, radius, gf::Color::rgba(245, 65, 118, 100));
        }
        painter.stroke_rounded_rect(circle, radius, gf::Color::rgba(199, 37, 89, 220), 1.25);
    }
    painter.restore();
}
} // namespace paint::forms
