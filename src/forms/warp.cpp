#include "conv.hpp"
#include "forms/editor.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
namespace paint::forms {
namespace gf = gui_forms;
namespace {
struct WarpDelivery {
    std::weak_ptr<Editor> editor;
    void operator()() const {
        std::shared_ptr<Editor> owner = editor.lock();
        if (owner) {
            (*owner).poll_warp();
        }
    }
};
struct WarpCompletion {
    Editor* editor;
    std::weak_ptr<Editor> owner;
    void operator()() const {
        // Editor joins its worker before destroying the Control dispatcher.
        // The queued callback acquires ownership only on the UI thread.
        static_cast<void>((*editor).begin_invoke(WarpDelivery{owner}));
    }
};
static Rect mesh_bounds(const ReshapeMesh& mesh) {
    double left = std::numeric_limits<double>::infinity(), top = left;
    double right = -left, bottom = -left;
    for (const MeshNode& node : mesh.nodes) {
        left = std::min(left, node.target.x);
        right = std::max(right, node.target.x);
        top = std::min(top, node.target.y);
        bottom = std::max(bottom, node.target.y);
    }
    int x = static_cast<int>(std::floor(left - 0.5));
    int y = static_cast<int>(std::floor(top - 0.5));
    Rect bounds{x, y, std::max(1, static_cast<int>(std::ceil(right + 0.5)) - x + 1),
                std::max(1, static_cast<int>(std::ceil(bottom + 0.5)) - y + 1)};
    return bounds;
}
static double segment_distance(Point point, Point first, Point last) {
    double dx = last.x - first.x, dy = last.y - first.y;
    double squared = dx * dx + dy * dy;
    double t = squared > 0 ? ((point.x - first.x) * dx + (point.y - first.y) * dy) / squared : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(point.x - first.x - t * dx, point.y - first.y - t * dy);
}
static void simplify_segment(const std::vector<Point>& input, std::size_t first, std::size_t last,
                             double tolerance, std::vector<Point>& output) {
    if (last <= first + 1) {
        output.push_back(input[first]);
        return;
    }
    double farthest = 0;
    std::size_t split = first;
    for (std::size_t i = first + 1; i < last; ++i) {
        double distance = segment_distance(input[i], input[first], input[last]);
        if (distance > farthest) {
            farthest = distance;
            split = i;
        }
    }
    if (farthest > tolerance) {
        simplify_segment(input, first, split, tolerance, output);
        simplify_segment(input, split, last, tolerance, output);
    } else {
        output.push_back(input[first]);
    }
}
} // namespace
Editor::~Editor() {
    warp_worker_.wait();
}
void Editor::initialize_warp() {
    warp_worker_.set_completion(WarpCompletion{this, std::static_pointer_cast<Editor>(shared_from_this())});
}
bool Editor::background_busy() const {
    return warp_worker_.busy() || stamp_pending_;
}
bool Editor::warp_active() const {
    return warp_mode_ != WarpMode::none;
}
void Editor::cancel_warp() {
    if (warp_active()) {
        document.selection = std::move(warp_original_);
    }
    warp_mode_ = WarpMode::none;
    warp_field_.reset();
    reshape_mesh_ = {};
    warp_pending_ = warp_commit_ = rotation_dragging_ = warp_whole_image_ = false;
    mesh_node_ = -1;
    ++warp_generation_;
    release_gesture();
}
void Editor::start_reshape() {
    finish_text(true);
    finish_warp(false);
    document.commit_path();
    document.commit_curve();
    if (!document.selection.active) {
        throw std::runtime_error("Select or lasso an object before reshaping it.");
    }
    warp_worker_.wait();
    poll_warp();
    if (warp_worker_.busy()) {
        throw std::runtime_error("The stamp is still preparing. Try reshape after it finishes.");
    }
    std::vector<Point> outline = document.selection.outline;
    if (outline.size() >= 3) {
        if (std::hypot(outline.back().x - outline.front().x, outline.back().y - outline.front().y) > 0.1) {
            outline.push_back(outline.front());
        }
        std::vector<Point> simplified;
        simplify_segment(outline, 0, outline.size() - 1, std::max(1.0, mesh_spacing * 0.08), simplified);
        if (simplified.size() >= 3) {
            outline = std::move(simplified);
        }
        reshape_mesh_ = make_reshape_mesh(outline, mesh_spacing);
    } else {
        reshape_mesh_ = make_reshape_mesh(document.selection.image, mesh_spacing);
    }
    warp_original_ = document.selection;
    warp_mode_ = WarpMode::mesh;
    warp_field_.reset();
    warp_pending_ = warp_commit_ = false;
    ++warp_generation_;
    warp_worker_.compile(WarpTask::CompileSelection, warp_original_.image, warp_generation_);
    (*ribbon_).show_transforms();
}
AffineMap Editor::rotation_map() const {
    double radians = rotation_angle * std::numbers::pi / 180;
    double cosine = std::cos(radians), sine = std::sin(radians);
    double x = (warp_original_.image.width - 1) * 0.5;
    double y = (warp_original_.image.height - 1) * 0.5;
    return {cosine, -sine, x - cosine * x + sine * y, sine, cosine, y - sine * x - cosine * y};
}
void Editor::start_rotation() {
    if (!document.selection.active || warp_active() || warp_worker_.busy()) {
        throw std::runtime_error("Select an object and finish the current transform before rotating.");
    }
    warp_original_ = document.selection;
    warp_mode_ = WarpMode::rotation;
    warp_field_.reset();
    rotation_angle = 0;
    warp_pending_ = warp_commit_ = warp_whole_image_ = false;
    ++warp_generation_;
    warp_worker_.compile(WarpTask::CompileRotation, warp_original_.image, warp_generation_);
}
void Editor::request_rotation(double degrees) {
    if (!std::isfinite(degrees)) {
        throw std::runtime_error("Enter a finite rotation angle.");
    }
    finish_text(true);
    finish_warp(false);
    document.commit_curve();
    document.commit_path();
    bool whole_image = !document.selection.active;
    if (whole_image) {
        document.select_all();
    }
    start_rotation();
    warp_whole_image_ = whole_image;
    rotation_angle = std::remainder(degrees, 360.0);
    ++warp_generation_;
    commit_warp();
}
void Editor::request_skew(int width, int height, bool scale, double horizontal_degrees,
                          double vertical_degrees) {
    if (!std::isfinite(horizontal_degrees) || !std::isfinite(vertical_degrees) ||
        std::abs(horizontal_degrees) > 80 || std::abs(vertical_degrees) > 80) {
        throw std::runtime_error("Skew angles must be between -80 and 80 degrees.");
    }
    finish_text(true);
    finish_warp(false);
    document.require_rgba_transform();
    if (warp_worker_.busy()) {
        throw std::runtime_error("Finish the current stamp transform first.");
    }
    document.commit_curve();
    document.commit_path();
    const Image& input = document.selection.active ? document.selection.image : document.image;
    Image source;
    if (scale) {
        conv_resize(input, width, height, source);
    } else {
        source.reset(width, height, document.ink.secondary);
        composite(source, input, 0, 0);
    }
    double horizontal = std::tan(horizontal_degrees * std::numbers::pi / 180.0);
    double vertical = std::tan(vertical_degrees * std::numbers::pi / 180.0);
    if (std::abs(1 - horizontal * vertical) < 0.05) {
        throw std::runtime_error("This skew collapses the picture. Choose smaller angles.");
    }
    AffineMap map{1, horizontal, 0, vertical, 1, 0};
    Point corners[4] = {{-0.5, -0.5}, {width - 0.5, -0.5}, {width - 0.5, height - 0.5}, {-0.5, height - 0.5}};
    double left = 0, right = 0, top = 0, bottom = 0;
    for (int i = 0; i < 4; ++i) {
        double x = corners[i].x + horizontal * corners[i].y, y = vertical * corners[i].x + corners[i].y;
        if (i == 0) {
            left = x;
            right = x;
            top = y;
            bottom = y;
        } else {
            left = std::min(left, x);
            right = std::max(right, x);
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }
    int x = static_cast<int>(std::floor(left + 0.5)), y = static_cast<int>(std::floor(top + 0.5));
    Rect bounds{x, y, std::max(1, static_cast<int>(std::ceil(right + 0.5)) - x),
                std::max(1, static_cast<int>(std::ceil(bottom + 0.5)) - y)};
    warp_original_ = document.selection;
    warp_whole_image_ = !document.selection.active;
    warp_mode_ = WarpMode::transform;
    warp_commit_ = true;
    ++warp_generation_;
    try {
        warp_worker_.transform(source, map, bounds, warp_generation_);
    } catch (...) {
        cancel_warp();
        throw;
    }
}
void Editor::commit_warp() {
    if (warp_active()) {
        warp_commit_ = warp_pending_ = true;
        mesh_node_ = -1;
        rotation_dragging_ = false;
        poll_warp();
    }
}
void Editor::finish_warp(bool place) {
    if (!warp_active()) {
        return;
    }
    commit_warp();
    while (warp_active() && warp_worker_.busy()) {
        warp_worker_.wait();
        poll_warp();
    }
    if (place) {
        document.commit_selection();
    }
}
void Editor::poll_warp() {
    try {
        WarpResult result;
        if (warp_worker_.take(result)) {
            bool stamp_compile = result.task == WarpTask::CompileStamp;
            bool stamp_render = result.task == WarpTask::Stamp;
            bool compile =
                result.task == WarpTask::CompileRotation || result.task == WarpTask::CompileSelection;
            bool stale = stamp_compile ? result.generation != stamp_source_generation_
                         : stamp_render
                             ? result.generation != stamp_generation_
                             : !warp_active() || (!compile && result.generation != warp_generation_);
            if (!stale && !result.error.empty()) {
                if (stamp_compile || stamp_render) {
                    reset_stamp();
                } else {
                    cancel_warp();
                }
                error(result.error);
                refresh();
                return;
            }
            if (!stale) {
                if (stamp_compile) {
                    stamp_field_ = std::move(result.field);
                } else if (stamp_render) {
                    stamp_preview_ = std::move(result.image);
                    publish_stamp_preview();
                } else if (compile) {
                    warp_field_ = std::move(result.field);
                } else if (!(warp_commit_ && (result.task == WarpTask::PreviewMesh ||
                                              result.task == WarpTask::PreviewRotation))) {
                    bool committed = result.task == WarpTask::Transform ||
                                     result.task == WarpTask::CommitMesh ||
                                     result.task == WarpTask::CommitRotation;
                    if (committed && warp_whole_image_) {
                        if (result.task == WarpTask::Transform) {
                            document.checkpoint();
                        }
                        document.assign_canvas(std::move(result.image));
                        document.selection = {};
                    } else {
                        document.selection.image = std::move(result.image);
                        document.selection.x = warp_original_.x + result.bounds.x;
                        document.selection.y = warp_original_.y + result.bounds.y;
                        document.selection.coverage.clear();
                        document.selection.outline.clear();
                    }
                    if (committed) {
                        if (warp_mode_ == WarpMode::mesh) {
                            document.commit_selection();
                        }
                        warp_mode_ = WarpMode::none;
                        warp_original_ = {};
                        warp_field_.reset();
                        reshape_mesh_ = {};
                        warp_pending_ = warp_commit_ = warp_whole_image_ = false;
                    }
                }
                refresh();
            }
        }
        if (warp_worker_.busy()) {
            return;
        }
        if (warp_active() && warp_field_ && warp_pending_) {
            if (warp_mode_ == WarpMode::mesh) {
                warp_worker_.mesh(warp_commit_ ? WarpTask::CommitMesh : WarpTask::PreviewMesh, warp_field_,
                                  reshape_mesh_, mesh_bounds(reshape_mesh_), warp_generation_);
            } else {
                AffineMap map = rotation_map();
                warp_worker_.affine(warp_commit_ ? WarpTask::CommitRotation : WarpTask::PreviewRotation,
                                    warp_field_, map, affine_bounds(*warp_field_, map), warp_generation_);
            }
            warp_pending_ = false;
        } else if (stamp_pending_ && !document.stamp.pixels.empty()) {
            if (!stamp_field_) {
                warp_worker_.compile(WarpTask::CompileStamp, document.stamp, stamp_source_generation_);
            } else {
                double angle = stamp_angle * std::numbers::pi / 180;
                double cosine = std::cos(angle) * stamp_scale, sine = std::sin(angle) * stamp_scale;
                AffineMap map{cosine, -sine, 0, sine, cosine, 0};
                warp_worker_.affine(WarpTask::Stamp, stamp_field_, map, affine_bounds(*stamp_field_, map),
                                    stamp_generation_);
                stamp_pending_ = false;
            }
        }
    } catch (const std::exception& exception) {
        cancel_warp();
        error(exception.what());
    }
}
void Editor::publish_stamp_preview() {
    if (!window() || stamp_preview_.pixels.empty()) {
        return;
    }
    std::vector<std::byte> pixels(stamp_preview_.pixels.size() * 4);
    for (std::size_t i = 0; i < stamp_preview_.pixels.size(); ++i) {
        Color color = stamp_preview_.pixels[i];
        pixels[i * 4] = static_cast<std::byte>((color.b * color.a + 127U) / 255U);
        pixels[i * 4 + 1] = static_cast<std::byte>((color.g * color.a + 127U) / 255U);
        pixels[i * 4 + 2] = static_cast<std::byte>((color.r * color.a + 127U) / 255U);
        pixels[i * 4 + 3] = static_cast<std::byte>(color.a);
    }
    gf::ImageLoadResult result =
        stamp_image_.value == 0
            ? (*window()).load_bgra32_premultiplied(stamp_preview_.width, stamp_preview_.height,
                                                    stamp_preview_.width * 4, pixels)
            : (*window()).replace_bgra32_premultiplied(stamp_image_, stamp_preview_.width,
                                                       stamp_preview_.height, stamp_preview_.width * 4,
                                                       pixels, *canvas_);
    if (!result) {
        throw std::runtime_error("The stamp preview exceeds the display resource budget.");
    }
    stamp_image_ = result.image;
}
void Editor::reset_stamp() {
    if (stamp_image_.value != 0 && window()) {
        static_cast<void>((*window()).remove_image(stamp_image_));
    }
    stamp_image_ = {};

    document.stamp = {};
    stamp_preview_ = {};
    stamp_field_.reset();
    stamp_pending_ = false;
    ++stamp_generation_;
    ++stamp_source_generation_;
}
void Editor::regenerate_stamp() {
    if (document.stamp.pixels.empty()) {
        return;
    }
    ++stamp_generation_;
    stamp_pending_ = true;
    poll_warp();
}
void Editor::stamp_at(Point point, bool checkpoint) {
    if (document.stamp.pixels.empty()) {
        int height = document.stamp_shape == StampShape::Circle || document.stamp_shape == StampShape::Square
                         ? stamp_width
                         : stamp_height;
        document.stamp = make_stamp(document.image,
                                    {static_cast<int>(point.x - stamp_width / 2.0),
                                     static_cast<int>(point.y - height / 2.0), stamp_width, height},
                                    document.stamp_shape, document.stamp_transparent, document.ink.secondary);
        stamp_scale = 1;
        stamp_angle = 0;
        stamp_field_.reset();
        ++stamp_source_generation_;
        regenerate_stamp();
    } else if (!stamp_pending_ && !warp_worker_.busy() && !stamp_preview_.pixels.empty()) {
        if (checkpoint) {
            document.checkpoint();
        }
        composite(document.image, stamp_preview_, static_cast<int>(point.x - stamp_preview_.width / 2.0),
                  static_cast<int>(point.y - stamp_preview_.height / 2.0));
    }
}
gf::Point Editor::rotation_handle() const {
    const FloatingSelection& selection =
        warp_mode_ == WarpMode::rotation ? warp_original_ : document.selection;
    gf::Point center =
        screen({selection.x + selection.image.width * 0.5, selection.y + selection.image.height * 0.5});
    gf::Point corner =
        screen({static_cast<double>(selection.x + selection.image.width), static_cast<double>(selection.y)});
    double x = corner.x + 18 - center.x, y = corner.y - 18 - center.y;
    double angle = warp_mode_ == WarpMode::rotation ? rotation_angle * std::numbers::pi / 180 : 0;
    return {center.x + std::cos(angle) * x - std::sin(angle) * y,
            center.y + std::sin(angle) * x + std::cos(angle) * y};
}
bool Editor::warp_pointer(const gf::PointerEvent& event, Point point) {
    if (event.button == gf::PointerButton::middle || panning_) {
        return false;
    }
    if (warp_commit_) {
        return true;
    }
    if (warp_mode_ == WarpMode::mesh) {
        if (event.action == gf::PointerAction::down && event.button == gf::PointerButton::primary) {
            Point local{point.x - warp_original_.x, point.y - warp_original_.y};
            for (std::size_t i = 0; i < reshape_mesh_.nodes.size(); ++i) {
                Point target = reshape_mesh_.nodes[i].target;
                if (std::hypot(local.x - target.x, local.y - target.y) * (*canvas_).zoom() < 10) {
                    mesh_node_ = static_cast<int>(i);
                    (*canvas_).set_pointer_capture(true);
                    break;
                }
            }
        } else if ((event.action == gf::PointerAction::move || event.action == gf::PointerAction::up) &&
                   mesh_node_ >= 0) {
            if (move_reshape_node(reshape_mesh_, static_cast<std::size_t>(mesh_node_),
                                  {point.x - warp_original_.x, point.y - warp_original_.y})) {
                ++warp_generation_;
                warp_pending_ = true;
                poll_warp();
            }
            if (event.action == gf::PointerAction::up) {
                mesh_node_ = -1;
                (*canvas_).set_pointer_capture(false);
            }
        }
        (*canvas_).invalidate(gf::Dirty::paint);
        return true;
    }
    if (!document.selection.active || dragging_ || text.active) {
        return false;
    }
    gf::Point pointer = screen(point), handle = rotation_handle();
    bool hit = std::hypot(pointer.x - handle.x, pointer.y - handle.y) <= 11;
    if (hit && !warp_active() && !warp_worker_.busy() && event.action == gf::PointerAction::down &&
        event.button == gf::PointerButton::primary) {
        start_rotation();
        rotation_dragging_ = true;
        Point center{warp_original_.x + warp_original_.image.width * 0.5,
                     warp_original_.y + warp_original_.image.height * 0.5};
        rotation_pointer_start_ = std::atan2(point.y - center.y, point.x - center.x);
        (*canvas_).set_pointer_capture(true);
    }
    if (rotation_dragging_ &&
        (event.action == gf::PointerAction::move || event.action == gf::PointerAction::up)) {
        Point center{warp_original_.x + warp_original_.image.width * 0.5,
                     warp_original_.y + warp_original_.image.height * 0.5};
        rotation_angle =
            std::remainder((std::atan2(point.y - center.y, point.x - center.x) - rotation_pointer_start_) *
                               180 / std::numbers::pi,
                           360.0);
        if (shift_) {
            rotation_angle = std::round(rotation_angle / 15) * 15;
        }
        ++warp_generation_;
        warp_pending_ = true;
        if (event.action == gf::PointerAction::up) {
            (*canvas_).set_pointer_capture(false);
            commit_warp();
        } else {
            poll_warp();
        }
        (*canvas_).invalidate(gf::Dirty::paint);
    }
    return warp_active();
}
void Editor::paint_warp_overlay(gf::Painter& painter) {
    const gf::Color blue = gf::Color::rgba(30, 100, 190), white = gf::Color::rgba(255, 255, 255);
    if (document.tool == Tool::Stamp && cursor_client_) {
        double width = stamp_preview_.pixels.empty() ? stamp_width : stamp_preview_.width;
        double height = stamp_preview_.pixels.empty() ? (document.stamp_shape == StampShape::Circle ||
                                                                 document.stamp_shape == StampShape::Square
                                                             ? stamp_width
                                                             : stamp_height)
                                                      : stamp_preview_.height;
        width *= (*canvas_).zoom();
        height *= (*canvas_).zoom();
        gf::Rect bounds{(*cursor_client_).x - width / 2, (*cursor_client_).y - height / 2, width, height};
        if (stamp_image_.value != 0) {
            painter.draw_image(stamp_image_, bounds, 0.65);
        }
        painter.stroke_rect(bounds, blue, 1);
    }

    if (warp_mode_ == WarpMode::mesh) {
        for (const MeshTriangle& triangle : reshape_mesh_.triangles) {
            for (int edge = 0; edge < 3; ++edge) {
                Point a = reshape_mesh_.nodes[triangle.nodes[edge]].target;
                Point b = reshape_mesh_.nodes[triangle.nodes[(edge + 1) % 3]].target;
                painter.draw_line(screen({a.x + warp_original_.x, a.y + warp_original_.y}),
                                  screen({b.x + warp_original_.x, b.y + warp_original_.y}),
                                  gf::Color::rgba(30, 100, 190, 120), 1);
            }
        }
        for (const MeshNode& node : reshape_mesh_.nodes) {
            gf::Point point = screen({node.target.x + warp_original_.x, node.target.y + warp_original_.y});
            painter.fill_rect({point.x - 4, point.y - 4, 8, 8}, white);
            painter.stroke_rect({point.x - 4, point.y - 4, 8, 8}, blue, 2);
        }
    } else if (document.selection.active) {
        gf::Point handle = rotation_handle();
        painter.fill_rect({handle.x - 10, handle.y - 10, 20, 20}, white);
        painter.stroke_rect({handle.x - 10, handle.y - 10, 20, 20}, blue, 1);
        gf::Point previous{handle.x - 5, handle.y};
        for (int i = 1; i <= 18; ++i) {
            double angle = std::numbers::pi + i * std::numbers::pi * 1.6 / 18;
            gf::Point next{handle.x + 5 * std::cos(angle), handle.y + 5 * std::sin(angle)};
            painter.draw_line(previous, next, blue, 1.5);
            previous = next;
        }
        painter.draw_line(previous, {previous.x + 3, previous.y - 4}, blue, 1.5);
        painter.draw_line(previous, {previous.x - 4, previous.y - 2}, blue, 1.5);
        if (warp_mode_ == WarpMode::rotation) {
            painter.draw_text_utf8({handle.x + 14, handle.y + 4},
                                   std::to_string(static_cast<int>(std::lround(rotation_angle))) + "°",
                                   {gf::FontRole::control, 12, 400, false}, blue);
        }
    }
}
} // namespace paint::forms
