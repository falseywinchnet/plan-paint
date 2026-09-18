#include "conv.hpp"
#include "forms/editor.hpp"
#include <algorithm>
#include <cmath>
namespace paint::forms {
namespace gf = gui_forms;
namespace {
const double handle_x[] = {0, 0.5, 1, 1, 1, 0.5, 0, 0};
const double handle_y[] = {0, 0, 0, 0.5, 1, 1, 1, 0.5};
} // namespace
bool Editor::resize_pointer(const gf::PointerEvent& event, Point point) {
    if (text.active || document.curve.base || warp_active() || panning_ || dragging_ ||
        (!document.selection.active && document.fixed_canvas())) {
        return false;
    }
    Rect bounds = document.selection.active
                      ? Rect{document.selection.x, document.selection.y, document.selection.image.width,
                             document.selection.image.height}
                      : Rect{0, 0, document.image.width, document.image.height};
    if (resize_handle_ < 0) {
        int hovered = -1;
        for (int i = 0; i < 8; ++i) {
            if (!document.selection.active && i != 3 && i != 4 && i != 5) {
                continue;
            }
            Point handle{bounds.x + bounds.w * handle_x[i], bounds.y + bounds.h * handle_y[i]};
            if (std::abs(point.x - handle.x) * canvas().zoom() <= 6 &&
                std::abs(point.y - handle.y) * canvas().zoom() <= 6) {
                hovered = i;
                break;
            }
        }
        canvas().set_cursor(hovered == 1 || hovered == 5   ? gf::CursorKind::resize_vertical
                            : hovered == 3 || hovered == 7 ? gf::CursorKind::resize_horizontal
                            : hovered == 0 || hovered == 4 ? gf::CursorKind::resize_diagonal_down
                            : hovered == 2 || hovered == 6 ? gf::CursorKind::resize_diagonal_up
                            : document.tool == Tool::Text  ? gf::CursorKind::text
                                                           : gf::CursorKind::crosshair);
        if (hovered < 0 || event.action != gf::PointerAction::down ||
            event.button != gf::PointerButton::primary) {
            return false;
        }
        resize_handle_ = hovered;
        resize_selection_ = document.selection.active;
        resize_original_ = resize_preview_ = bounds;
        resize_start_ = point;
        canvas().set_pointer_capture(true);
        if (window()) {
            static_cast<void>((*window()).request_focus(canvas_));
        }
    }
    int dx = static_cast<int>(std::round(point.x - resize_start_.x)),
        dy = static_cast<int>(std::round(point.y - resize_start_.y));
    resize_preview_ = resize_original_;
    if (resize_handle_ == 0 || resize_handle_ == 6 || resize_handle_ == 7) {
        resize_preview_.x += dx;
        resize_preview_.w -= dx;
    }
    if (resize_handle_ == 2 || resize_handle_ == 3 || resize_handle_ == 4) {
        resize_preview_.w += dx;
    }
    if (resize_handle_ == 0 || resize_handle_ == 1 || resize_handle_ == 2) {
        resize_preview_.y += dy;
        resize_preview_.h -= dy;
    }
    if (resize_handle_ == 4 || resize_handle_ == 5 || resize_handle_ == 6) {
        resize_preview_.h += dy;
    }
    resize_preview_.w = std::clamp(resize_preview_.w, 1, 32768);
    resize_preview_.h = std::clamp(resize_preview_.h, 1, 32768);
    canvas().invalidate(gf::Dirty::paint);
    if (event.action == gf::PointerAction::up && event.button == gf::PointerButton::primary) {
        if (resize_selection_) {
            document.resize(resize_preview_.w, resize_preview_.h, true);
            document.selection.x = resize_preview_.x;
            document.selection.y = resize_preview_.y;
        } else {
            document.resize(resize_preview_.w, resize_preview_.h, false);
        }
        resize_handle_ = -1;
        canvas().set_pointer_capture(false);
        refresh();
    }
    return true;
}
void Editor::paint_resize_overlay(gf::Painter& painter) {
    if (text.active || document.curve.base || warp_active() ||
        (!document.selection.active && document.fixed_canvas())) {
        return;
    }
    Rect bounds = document.selection.active
                      ? Rect{document.selection.x, document.selection.y, document.selection.image.width,
                             document.selection.image.height}
                      : Rect{0, 0, document.image.width, document.image.height};
    if (resize_handle_ >= 0) {
        bounds = resize_preview_;
    }
    for (int i = 0; i < 8; ++i) {
        if (!document.selection.active && i != 3 && i != 4 && i != 5) {
            continue;
        }
        gf::Point point = screen({bounds.x + bounds.w * handle_x[i], bounds.y + bounds.h * handle_y[i]});
        painter.fill_rect({point.x - 3, point.y - 3, 6, 6}, gf::Color::rgba(255, 255, 255));
        painter.stroke_rect({point.x - 3, point.y - 3, 6, 6}, gf::Color::rgba(30, 95, 160), 1);
    }
    if (resize_handle_ >= 0) {
        gf::Point point = screen({static_cast<double>(bounds.x), static_cast<double>(bounds.y)});
        double width = bounds.w * canvas().zoom(), height = bounds.h * canvas().zoom();
        painter.stroke_rect({point.x, point.y, width, height}, gf::Color::rgba(30, 95, 160), 2);
        painter.draw_text_utf8({point.x + width + 8, point.y + height + 16},
                               std::to_string(bounds.w) + " × " + std::to_string(bounds.h) + " px",
                               {gf::FontRole::control, 12, 400, false}, gf::Color::rgba(30, 65, 95));
    }
}
} // namespace paint::forms
