#include "forms/editor.hpp"
#include "raster.hpp"
#include <algorithm>
#include <cmath>
namespace paint::forms {
namespace gf = gui_forms;
void PaintCanvas::on_attached_to_window() {
    RasterCanvas::on_attached_to_window();
    // A restrained, deterministic fiber tile; screen-space grain never alters artwork.
    std::vector<std::byte> pixels(128 * 128 * 4);
    std::uint32_t noise = 0x5241494eU;
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            noise = noise * 1664525U + 1013904223U;
            int fiber = static_cast<int>((noise >> 24) % 7) - 3;
            fiber += ((x + 3 * y) % 17 == 0 ? 1 : 0);
            std::size_t offset = static_cast<std::size_t>(y * 128 + x) * 4;
            pixels[offset] = static_cast<std::byte>(226 + fiber);
            pixels[offset + 1] = static_cast<std::byte>(218 + fiber);
            pixels[offset + 2] = static_cast<std::byte>(208 + fiber);
            pixels[offset + 3] = std::byte{255};
        }
    }
    felt_ = (*attached_window()).load_bgra32_premultiplied(128, 128, 512, pixels).image;
}
void PaintCanvas::on_detaching_from_window(gf::Window& former_window) noexcept {
    if (felt_.value) {
        static_cast<void>(former_window.remove_image(felt_));
    }
    felt_ = {};
    RasterCanvas::on_detaching_from_window(former_window);
}
void PaintCanvas::on_dispose() noexcept {
    if (felt_.value && window()) {
        static_cast<void>((*window()).remove_image(felt_));
    }
    felt_ = {};
    RasterCanvas::on_dispose();
}
void PaintCanvas::on_paint(gf::Painter& painter, gf::Rect damage) {
    gf::Rect bounds = client_rectangle();
    if (felt_.value) {
        painter.fill_image_pattern(felt_, {128, 128}, bounds, {128, 128});
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
}
void Editor::paint_tool_preview(gf::Painter& painter) {
    if (!cursor_client_ || (document.tool != Tool::Pencil && document.tool != Tool::Eraser &&
                             document.tool != Tool::Magnifier)) {
        return;
    }
    gui_drawing::PointF point = (*canvas_).client_to_bitmap(*cursor_client_);
    int x = static_cast<int>(std::floor(point.x)), y = static_cast<int>(std::floor(point.y));
    if (!document.image.contains(x, y)) {
        return;
    }
    if (document.tool == Tool::Magnifier) {
        gf::Rect viewport = (*canvas_).client_rectangle();
        double left = (*cursor_client_).x + 24, top = (*cursor_client_).y + 24;
        if (left + 132 > viewport.width) {
            left = (*cursor_client_).x - 156;
        }
        if (top + 152 > viewport.height) {
            top = (*cursor_client_).y - 176;
        }
        left = std::clamp(left, 2.0, std::max(2.0, viewport.width - 134));
        top = std::clamp(top, 2.0, std::max(2.0, viewport.height - 154));
        gf::Rect lens{left, top, 132, 152};
        painter.draw_box_shadow(lens, 2, {2, 3}, 5, 0, gf::Color::rgba(30, 40, 55, 90));
        painter.fill_rect(lens, gf::Color::rgba(250, 252, 255));
        Image lens_pixels;
        lens_pixels.reset(32, 32, {0, 0, 0, 0});
        for (int row = 0; row < 32; ++row) {
            for (int column = 0; column < 32; ++column) {
                int px = x + column - 16, py = y + row - 16;
                Color color = document.image.get(px, py);
                Color base = ((px / 4 + py / 4) & 1) ? Color{225, 225, 225, 255}
                                                       : Color{255, 255, 255, 255};
                lens_pixels.set(column, row, base);
                lens_pixels.blend(column, row, color);
                if (document.selection.active) {
                    Color selected = document.selection.image.get(px - document.selection.x,
                                                                  py - document.selection.y);
                    lens_pixels.blend(column, row, selected);
                }
                color = lens_pixels.get(column, row);
                painter.fill_rect({left + 2 + column * 4, top + 2 + row * 4, 4, 4},
                                  gf::Color::rgba(color.r, color.g, color.b));
            }
        }
        painter.stroke_rect(lens, gf::Color::rgba(45, 90, 140), 1);
        painter.draw_text_utf8({left + 6, top + 146}, "4×", {gf::FontRole::control, 12, 400, false},
                               gf::Color::rgba(35, 60, 90));
        return;
    }
    double scale = (*canvas_).zoom();
    painter.save();
    painter.clip_rect((*canvas_).bitmap_to_client({0, 0, document.image.width, document.image.height}));
    if (document.tool == Tool::Pencil && !dragging_) {
        Color color = patterned(document.ink, x, y);
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
