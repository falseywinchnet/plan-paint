#include "conv.hpp"
#include "forms/editor.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace paint::forms {
namespace gf = gui_forms;
namespace {
const double handle_x[] = {0, 0.5, 1, 1, 1, 0.5, 0, 0, 0.5, 1, 0.5, 0};
const double handle_y[] = {0, 0, 0, 0.5, 1, 1, 1, 0.5, 0, 0.5, 1, 0.5};
} // namespace
// Interactive pixels are display-only. The document keeps its original samples
// until CONV produces the final transform. Sampling work is bounded by the viewport.
void Editor::clear_transform_preview() {
    if (transform_image_.value && window()) {
        static_cast<void>((*window()).remove_image(transform_image_));
    }
    transform_image_ = {};
}
void Editor::update_transform_preview() {
    if (!window()) {
        return;
    }
    const bool rotating = warp_mode_ == WarpMode::rotation;
    const bool shearing = resize_handle_ >= 8;
    const Image& source = rotating ? warp_original_.image : document.selection.image;
    if (source.pixels.empty()) {
        return;
    }
    const double angle = rotating ? rotation_angle * std::numbers::pi / 180 : 0;
    const double cosine = std::cos(angle), sine = std::sin(angle);
    const double width = rotating ? source.width : resize_preview_.w;
    const double height = rotating ? source.height : resize_preview_.h;
    const double center_x =
        rotating ? warp_original_.x + source.width * 0.5 : resize_preview_.x + width * 0.5;
    const double center_y =
        rotating ? warp_original_.y + source.height * 0.5 : resize_preview_.y + height * 0.5;
    const double half_x = (std::abs(cosine) * width + std::abs(sine) * height) * 0.5;
    const double half_y = (std::abs(sine) * width + std::abs(cosine) * height) * 0.5;
    gf::Point first = screen({center_x - half_x, center_y - half_y});
    gf::Point last = screen({center_x + half_x, center_y + half_y});
    gf::Rect viewport = canvas().committed_arranged_bounds();
    double left = std::clamp(std::floor(first.x), 0.0, viewport.width);
    double top = std::clamp(std::floor(first.y), 0.0, viewport.height);
    double right = std::clamp(std::ceil(last.x), left, viewport.width);
    double bottom = std::clamp(std::ceil(last.y), top, viewport.height);
    transform_destination_ = {left, top, right - left, bottom - top};
    if (right <= left || bottom <= top) {
        clear_transform_preview();
        return;
    }
    const double step = std::max(1.0, std::sqrt((right - left) * (bottom - top) / 1048576.0));
    const int columns = std::max(1, static_cast<int>(std::ceil((right - left) / step)));
    const int rows = std::max(1, static_cast<int>(std::ceil((bottom - top) / step)));
    std::vector<std::byte> pixels(static_cast<std::size_t>(columns) * rows * 4);
    const gui_drawing::PointF origin = canvas().view_origin();
    const double inverse_zoom = 1.0 / canvas().zoom();
    const double pixel_width = (right - left) / columns;
    const double pixel_height = (bottom - top) / rows;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            double dx = origin.x + (left + (x + 0.5) * pixel_width) * inverse_zoom - center_x;
            double dy = origin.y + (top + (y + 0.5) * pixel_height) * inverse_zoom - center_y;
            double sx = (cosine * dx + sine * dy) * source.width / width + source.width * 0.5 - 0.5;
            double sy = (-sine * dx + cosine * dy) * source.height / height + source.height * 0.5 - 0.5;
            if (shearing) {
                const double px = origin.x + (left + (x + 0.5) * pixel_width) * inverse_zoom -
                                  resize_original_.x - 0.5 - shear_map_.tx;
                const double py = origin.y + (top + (y + 0.5) * pixel_height) * inverse_zoom -
                                  resize_original_.y - 0.5 - shear_map_.ty;
                sx = px - shear_map_.xy * py;
                sy = py - shear_map_.yx * px;
            }
            int ix = static_cast<int>(std::floor(sx)), iy = static_cast<int>(std::floor(sy));
            double fx = sx - ix, fy = sy - iy;
            double channels[4] = {};
            for (int j = 0; j < 2; ++j) {
                for (int i = 0; i < 2; ++i) {
                    Color color = source.get(ix + i, iy + j);
                    double weight = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
                    channels[0] += color.b * color.a / 255.0 * weight;
                    channels[1] += color.g * color.a / 255.0 * weight;
                    channels[2] += color.r * color.a / 255.0 * weight;
                    channels[3] += color.a * weight;
                }
            }
            std::size_t offset = (static_cast<std::size_t>(y) * columns + x) * 4;
            for (int channel = 0; channel < 4; ++channel) {
                pixels[offset + channel] = static_cast<std::byte>(
                    static_cast<unsigned>(std::clamp(std::round(channels[channel]), 0.0, 255.0)));
            }
        }
    }
    gf::ImageLoadResult result =
        transform_image_.value == 0
            ? (*window()).load_bgra32_premultiplied(columns, rows, columns * 4, pixels)
            : (*window()).replace_bgra32_premultiplied(transform_image_, columns, rows, columns * 4, pixels,
                                                       canvas());
    if (!result) {
        throw std::runtime_error("The transform preview exceeds the display resource budget.");
    }
    transform_image_ = result.image;
    canvas().invalidate(gf::Dirty::paint);
}
bool Editor::resize_pointer(const gf::PointerEvent& event, Point point) {
    if (document.selection.on_canvas && document.tool != Tool::Select && document.tool != Tool::Lasso) {
        return false;
    }
    if ((document.tool == Tool::Lasso && (control_ || alt_)) || document.tool == Tool::Guide || text.active ||
        document.curve.base || warp_active() || warp_worker_.busy() || panning_ || dragging_ ||
        (!document.selection.active && document.fixed_canvas())) {
        return false;
    }
    Rect bounds = document.selection.active
                      ? Rect{document.selection.x, document.selection.y, document.selection.image.width,
                             document.selection.image.height}
                      : Rect{0, 0, document.image.width, document.image.height};
    if (resize_handle_ < 0) {
        int hovered = -1;
        for (int i = 0; i < 12; ++i) {
            if (!document.selection.active && i != 3 && i != 4 && i != 5) {
                continue;
            }
            Point handle{bounds.x + bounds.w * handle_x[i], bounds.y + bounds.h * handle_y[i]};
            if (i >= 8) {
                handle.x += (i == 9 ? 16 : i == 11 ? -16 : 0) / canvas().zoom();
                handle.y += (i == 10 ? 16 : i == 8 ? -16 : 0) / canvas().zoom();
            }
            if (std::abs(point.x - handle.x) * canvas().zoom() <= 6 &&
                std::abs(point.y - handle.y) * canvas().zoom() <= 6) {
                hovered = i;
                break;
            }
        }
        canvas().set_cursor(hovered == 9 || hovered == 11   ? gf::CursorKind::resize_vertical
                            : hovered == 8 || hovered == 10 ? gf::CursorKind::resize_horizontal
                            : hovered == 1 || hovered == 5  ? gf::CursorKind::resize_vertical
                            : hovered == 3 || hovered == 7  ? gf::CursorKind::resize_horizontal
                            : hovered == 0 || hovered == 4  ? gf::CursorKind::resize_diagonal_down
                            : hovered == 2 || hovered == 6  ? gf::CursorKind::resize_diagonal_up
                            : document.tool == Tool::Text   ? gf::CursorKind::text
                                                            : gf::CursorKind::crosshair);
        if (hovered < 0 || event.action != gf::PointerAction::down ||
            event.button != gf::PointerButton::primary) {
            return false;
        }
        unset_guide();
        if (document.selection.active) {
            document.lift_selection();
        }
        resize_handle_ = hovered;
        resize_selection_ = document.selection.active;
        resize_original_ = resize_preview_ = bounds;
        resize_start_ = point;
        shear_map_ = {};
        refresh();
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
    if (resize_handle_ == 0 || resize_handle_ == 6 || resize_handle_ == 7) {
        resize_preview_.x = resize_original_.x + resize_original_.w - resize_preview_.w;
    }
    if (resize_handle_ == 0 || resize_handle_ == 1 || resize_handle_ == 2) {
        resize_preview_.y = resize_original_.y + resize_original_.h - resize_preview_.h;
    }
    if (resize_handle_ >= 8) {
        // Coordinates are pixel centres. The half-pixel translation anchors the
        // opposite footprint edge, rather than its first row of pixel centres.
        const bool horizontal = resize_handle_ == 8 || resize_handle_ == 10;
        const bool far_edge = resize_handle_ == 9 || resize_handle_ == 10;
        const double extent = horizontal ? resize_original_.h : resize_original_.w;
        const double displacement =
            std::clamp(static_cast<double>(horizontal ? dx : dy), -4 * extent, 4 * extent);
        const double slope = displacement / extent * (far_edge ? 1 : -1);
        const double offset = far_edge ? slope * 0.5 : displacement + slope * 0.5;
        shear_map_ = horizontal ? AffineMap{1, slope, offset, 0, 1, 0} : AffineMap{1, 0, 0, slope, 1, offset};
        const int low = static_cast<int>(std::floor(std::min(0.0, displacement)));
        const int extra = static_cast<int>(std::ceil(std::abs(displacement)));
        resize_preview_ = resize_original_;
        resize_preview_.x += horizontal ? low : 0;
        resize_preview_.y += horizontal ? 0 : low;
        resize_preview_.w += horizontal ? extra : 0;
        resize_preview_.h += horizontal ? 0 : extra;
    }
    if (resize_selection_) {
        update_transform_preview();
    }
    canvas().invalidate(gf::Dirty::paint);
    update_status();
    if (event.action == gf::PointerAction::up && event.button == gf::PointerButton::primary) {
        if (resize_handle_ >= 8 && (dx != 0 || dy != 0)) {
            const Rect transformed{resize_preview_.x - resize_original_.x,
                                   resize_preview_.y - resize_original_.y, resize_preview_.w,
                                   resize_preview_.h};
            warp_original_ = document.selection;
            warp_coverage_ = warp_original_.transformed_mask(shear_map_, transformed);
            warp_whole_image_ = false;
            warp_mode_ = WarpMode::transform;
            warp_commit_ = true;
            ++warp_generation_;
            try {
                warp_worker_.transform(warp_original_.image, shear_map_, transformed, warp_generation_);
            } catch (...) {
                cancel_warp();
                throw;
            }
        } else if (resize_selection_ && resize_handle_ < 8) {
            document.resize(resize_preview_.w, resize_preview_.h, true);
            document.selection.x = resize_preview_.x;
            document.selection.y = resize_preview_.y;
        } else if (!resize_selection_) {
            document.resize(resize_preview_.w, resize_preview_.h, false);
        }
        clear_transform_preview();
        resize_handle_ = -1;
        canvas().set_pointer_capture(false);
        refresh();
    }
    return true;
}
void Editor::paint_resize_overlay(gf::Painter& painter) {
    if (document.selection.on_canvas && document.tool != Tool::Select && document.tool != Tool::Lasso) {
        return;
    }
    if (document.tool == Tool::Guide || text.active || document.curve.base || warp_active() ||
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
    for (int i = 0; i < 12; ++i) {
        if (resize_handle_ >= 8) {
            break;
        }
        if (!document.selection.active && i != 3 && i != 4 && i != 5) {
            continue;
        }
        gf::Point point = screen({bounds.x + bounds.w * handle_x[i], bounds.y + bounds.h * handle_y[i]});
        if (i >= 8) {
            point.x += i == 9 ? 16 : i == 11 ? -16 : 0;
            point.y += i == 10 ? 16 : i == 8 ? -16 : 0;
            const gf::Color gold = gf::Color::rgba(166, 112, 22);
            painter.fill_rounded_rect({point.x - 4, point.y - 4, 8, 8}, 4, gf::Color::rgba(255, 224, 144));
            painter.stroke_rounded_rect({point.x - 4, point.y - 4, 8, 8}, 4, gold, 1);
            const bool horizontal = i == 8 || i == 10;
            painter.draw_line({point.x - (horizontal ? 8 : 0), point.y - (horizontal ? 0 : 8)},
                              {point.x + (horizontal ? 8 : 0), point.y + (horizontal ? 0 : 8)}, gold, 1);
            continue;
        }
        painter.fill_rect({point.x - 3, point.y - 3, 6, 6}, gf::Color::rgba(255, 255, 255));
        painter.stroke_rect({point.x - 3, point.y - 3, 6, 6}, gf::Color::rgba(30, 95, 160), 1);
    }
    if (resize_handle_ >= 8) {
        gf::Point corners[4];
        for (int i = 0; i < 4; ++i) {
            const double x = (i == 1 || i == 2 ? resize_original_.w : 0) - 0.5;
            const double y = (i >= 2 ? resize_original_.h : 0) - 0.5;
            corners[i] = screen({resize_original_.x + x + shear_map_.xy * y + shear_map_.tx + 0.5,
                                 resize_original_.y + y + shear_map_.yx * x + shear_map_.ty + 0.5});
        }
        for (int i = 0; i < 4; ++i) {
            painter.draw_line(corners[i], corners[(i + 1) % 4], gf::Color::rgba(166, 112, 22), 2);
        }
    } else if (resize_handle_ >= 0) {
        gf::Point point = screen({static_cast<double>(bounds.x), static_cast<double>(bounds.y)});
        double width = bounds.w * canvas().zoom(), height = bounds.h * canvas().zoom();
        painter.stroke_rect({point.x, point.y, width, height}, gf::Color::rgba(30, 95, 160), 2);
        painter.draw_text_utf8({point.x + width + 8, point.y + height + 16},
                               std::to_string(bounds.w) + " × " + std::to_string(bounds.h) + " px",
                               {gf::FontRole::control, 12, 400, false}, gf::Color::rgba(30, 65, 95));
    }
}
} // namespace paint::forms

namespace paint::forms {
void Editor::update_selection_contours() {
    const FloatingSelection& selection = document.selection;
    if (!selection.active) {
        selection_contours_.clear();
        selection_contour_mask_.clear();
        selection_frame_.disconnect();
        return;
    }
    std::vector<std::uint8_t> mask = selection.coverage;
    if (mask.empty()) {
        mask.resize(selection.image.pixels.size());
        for (std::size_t index = 0; index < mask.size(); ++index) {
            mask[index] = selection.image.pixels[index].a >= 128 ? 1 : 0;
        }
    } else if (selection.source && (*selection.source).feathered) {
        for (std::size_t index = 0; index < mask.size(); ++index) {
            mask[index] = mask[index] >= 128 ? 1 : 0;
        }
    }
    if (selection_contour_width_ != selection.image.width ||
        selection_contour_height_ != selection.image.height || selection_contour_mask_ != mask) {
        selection_contour_width_ = selection.image.width;
        selection_contour_height_ = selection.image.height;
        selection_contour_mask_ = std::move(mask);
        selection_contours_ =
            mask_contours(selection_contour_mask_, selection_contour_width_, selection_contour_height_);
    }
    selection_frame();
}
void Editor::selection_frame() {
    selection_frame_.disconnect();
    if (document.selection.active && window()) {
        canvas().invalidate(gf::Dirty::paint);
        selection_frame_ =
            (*window()).schedule_paint(canvas_, gf::FrameClock::now() + std::chrono::milliseconds(100));
    }
}
void Editor::paint_selection_contours(gf::Painter& painter) {
    if (!document.selection.active || resize_handle_ >= 0 || warp_active()) {
        return;
    }
    const double phase = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                 std::chrono::steady_clock::now().time_since_epoch())
                                                 .count() /
                                             100 % 8);
    const gf::Rect viewport = canvas().client_rectangle();
    for (std::size_t contour = 0; contour < selection_contours_.size(); ++contour) {
        const std::vector<Point>& loop = selection_contours_[contour];
        double travelled = 0;
        for (std::size_t index = 0; index < loop.size(); ++index) {
            const Point first = loop[index], last = loop[(index + 1) % loop.size()];
            const gf::Point a = screen({first.x + document.selection.x, first.y + document.selection.y}),
                            b = screen({last.x + document.selection.x, last.y + document.selection.y});
            const double dx = b.x - a.x, dy = b.y - a.y, length = std::hypot(dx, dy);
            if (length == 0) {
                continue;
            }
            // Clip before subdividing dashes, so zooming a long edge does not
            // create work proportional to off-screen pixels.
            double low = 0, high = 1;
            const double p[] = {-dx, dx, -dy, dy};
            const double q[] = {a.x + 2, viewport.width + 2 - a.x, a.y + 2, viewport.height + 2 - a.y};
            for (int edge = 0; edge < 4; ++edge) {
                if (p[edge] == 0) {
                    if (q[edge] < 0) {
                        high = -1;
                    }
                } else if (p[edge] < 0) {
                    low = std::max(low, q[edge] / p[edge]);
                } else {
                    high = std::min(high, q[edge] / p[edge]);
                }
            }
            if (low <= high) {
                painter.draw_line({a.x + dx * low, a.y + dy * low}, {a.x + dx * high, a.y + dy * high},
                                  gf::Color::rgba(255, 255, 255), 2);
                const double offset = std::fmod(travelled - phase + 8, 8.0);
                double start = std::floor((low * length + offset) / 8) * 8 - offset;
                for (; start < high * length; start += 8) {
                    const double from = std::max(start, low * length),
                                 to = std::min(start + 4, high * length);
                    if (to > from) {
                        painter.draw_line({a.x + dx * from / length, a.y + dy * from / length},
                                          {a.x + dx * to / length, a.y + dy * to / length},
                                          gf::Color::rgba(20, 25, 30), 1);
                    }
                }
            }
            travelled += length;
        }
    }
}
} // namespace paint::forms
