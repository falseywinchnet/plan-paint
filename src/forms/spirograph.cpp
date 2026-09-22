#include "forms/display.hpp"
#include "forms/editor.hpp"
#include "forms/spirograph_view.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace paint::forms {
namespace gf = gui_forms;
namespace {
constexpr double tau = 2 * std::numbers::pi;
void disc(gf::Painter& painter, gf::Point c, double r, gf::Color color) {
    painter.fill_rounded_rect({c.x - r, c.y - r, 2 * r, 2 * r}, r, color);
}
void circle(gf::Painter& painter, gf::Point c, double r, gf::Color color, double width) {
    painter.stroke_rounded_rect({c.x - r, c.y - r, 2 * r, 2 * r}, r, color, width);
}
gf::Point polar(gf::Point center, double radius, double angle) {
    return {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
}
void peg_cap(gf::Painter& painter, gf::Point center, double r, const SpiroPeg& peg) {
    disc(painter, {center.x + 1, center.y + 2}, r + 1, gf::Color::rgba(10, 25, 50, 80));
    const Color ink = peg.loaded ? peg.ink : Color{222, 235, 245, 255};
    disc(painter, center, r, gf::Color::rgba(ink.r, ink.g, ink.b, 190));
    circle(painter, center, r, gf::Color::rgba(245, 255, 255, 210), 1.3);
    circle(painter, center, r + 1, gf::Color::rgba(20, 60, 90, 210), 1);
    disc(painter, {center.x - r * .24, center.y - r * .32}, r * .28, gf::Color::rgba(255, 255, 255, 160));
    disc(painter, center, 1.3, gf::Color::rgba(20, 45, 65, 210));
}
} // namespace
void paint_spiro_part(gf::Painter& painter, gf::Rect bounds, int kind, int index) {
    const gf::Point c{bounds.x + bounds.width * .5, bounds.y + bounds.height * .5};
    const double r = std::min(bounds.width, bounds.height) * .34;
    if (kind == 2) {
        SpiroPeg peg;
        peg.seated = true;
        peg_cap(painter, c, r * (.6 + .15 * index), peg);
        return;
    }
    if (kind == 0) {
        circle(painter, {c.x + 1, c.y + 2}, r, gf::Color::rgba(15, 75, 35, 100), 7);
        circle(painter, c, r, gf::Color::rgba(80, 235, 65, 150), 6);
        circle(painter, c, r + 3, gf::Color::rgba(220, 255, 165, 230), 1);
    } else {
        disc(painter, {c.x + 1, c.y + 2}, r, gf::Color::rgba(10, 60, 90, 80));
        disc(painter, c, r, gf::Color::rgba(0, 174, 255, 125));
        circle(painter, c, r, gf::Color::rgba(10, 110, 215, 235), 1.2);
        for (int i = 0; i < 18 + index * 3; ++i) {
            const double angle = tau * i / (18 + index * 3);
            painter.draw_line(polar(c, r - 1, angle), polar(c, r + 2, angle),
                              gf::Color::rgba(10, 140, 225, 220), 2);
        }
        for (int i = 0; i < 3; ++i) {
            circle(painter, {c.x + r * (.25 + i * .25), c.y}, 1.4, gf::Color::rgba(250, 255, 255, 240), 1);
        }
    }
}
void Editor::paint_spiro_overlay(gf::Painter& painter) {
    if (!spiro.active) {
        return;
    }
    const double zoom = canvas().zoom(), radius = spiro.guide_radius() * zoom;
    const gf::Point center = screen(spiro.center);
    const double rim = std::max(8.0, 10 * spiro.scale * zoom);
    circle(painter, {center.x + 1, center.y + 2}, radius + rim * .65, gf::Color::rgba(15, 60, 25, 60),
           rim + 2);
    circle(painter, center, radius + rim * .65, gf::Color::rgba(55, 245, 65, 85), rim);
    circle(painter, center, radius + rim * 1.15, gf::Color::rgba(205, 255, 175, 220), 1.6);
    circle(painter, center, radius + rim * .15, gf::Color::rgba(24, 150, 40, 210), 1.3);
    for (int i = 0; i < spiro_guides()[spiro.guide].teeth; ++i) {
        const double a = tau * i / spiro_guides()[spiro.guide].teeth + canvas().view_angle;
        painter.draw_line(polar(center, radius - 1.3 * spiro.scale * zoom, a),
                          polar(center, radius + 2 * spiro.scale * zoom, a),
                          gf::Color::rgba(38, 200, 45, 185), std::max(1.0, 2.2 * spiro.scale * zoom));
    }
    const gf::Point close =
        screen({spiro.center.x + (spiro.guide_radius() + 10 * spiro.scale) * .7071067811865476,
                spiro.center.y - (spiro.guide_radius() + 10 * spiro.scale) * .7071067811865476});
    disc(painter, {close.x + 1, close.y + 2}, 10, gf::Color::rgba(70, 20, 20, 100));
    disc(painter, close, 9, gf::Color::rgba(219, 55, 55, 250));
    circle(painter, close, 9, gf::Color::rgba(255, 184, 162, 240), 1.4);
    painter.draw_line({close.x - 3, close.y - 3}, {close.x + 3, close.y + 3}, gf::Color::rgba(255, 255, 255),
                      1.8);
    painter.draw_line({close.x - 3, close.y + 3}, {close.x + 3, close.y - 3}, gf::Color::rgba(255, 255, 255),
                      1.8);
    if (spiro.inserted) {
        const gf::Point wheel = screen(spiro.wheel_center(spiro.angle));
        const double r = spiro.wheel_radius() * zoom;
        disc(painter, {wheel.x + 1, wheel.y + 2}, r, gf::Color::rgba(15, 45, 90, 30));
        disc(painter, wheel, r, gf::Color::rgba(0, 170, 255, 70));
        circle(painter, wheel, r - 1, gf::Color::rgba(175, 240, 255, 220), 1.7);
        circle(painter, wheel, r, gf::Color::rgba(15, 110, 215, 220), 1.2);
        const int teeth = spiro_inserts()[spiro.insert].teeth;
        for (int i = 0; i < teeth; ++i) {
            const double a = tau * i / teeth + spiro.wheel_rotation(spiro.angle) + canvas().view_angle;
            painter.draw_line(polar(wheel, r - 1.3 * spiro.scale * zoom, a),
                              polar(wheel, r + 1.3 * spiro.scale * zoom, a),
                              gf::Color::rgba(5, 151, 245, 215), std::max(1.0, 2.2 * spiro.scale * zoom));
        }
        disc(painter, wheel, 8, gf::Color::rgba(145, 228, 255, 180));
        circle(painter, wheel, 8, gf::Color::rgba(10, 100, 170, 230), 1);
        for (int axis = 0; axis < 2; ++axis) {
            painter.draw_line({wheel.x - (axis ? 0 : 4), wheel.y - (axis ? 4 : 0)},
                              {wheel.x + (axis ? 0 : 4), wheel.y + (axis ? 4 : 0)},
                              gf::Color::rgba(25, 100, 145), 1);
        }
        for (int i = 0; i < spiro.hole_count(); ++i) {
            const gf::Point hole = screen(spiro.hole(i, spiro.angle));
            if (spiro.pegs[i].seated) {
                peg_cap(painter, hole, 7, spiro.pegs[i]);
            } else {
                disc(painter, hole, 4, gf::Color::rgba(240, 250, 255, 160));
                circle(painter, hole, 4, gf::Color::rgba(10, 100, 155, 200), 1);
                if (spiro_drag_ == SpiroDrag::Peg && spiro_target_hole_ == i) {
                    circle(painter, hole, 9, gf::Color::rgba(255, 195, 30, 250), 2);
                }
            }
        }
    }
    if (spiro_drag_ == SpiroDrag::Peg) {
        peg_cap(painter, screen(spiro_pointer_), 9, spiro_carried_);
    }
}
void Editor::start_spirograph() {
    finish_controls(false);
    if (!spiro.active) {
        spiro.open(document.image.width, document.image.height);
    }
    document.tool = Tool::Spirograph;
    (*ribbon_).show_tool_context();
    refresh();
}
void Editor::spiro_choice(const std::string& id) {
    if (id == "spirograph") {
        start_spirograph();
        return;
    }
    if (!spiro.active) {
        return;
    }
    release_gesture();
    if (id.starts_with("spiro-guide-")) {
        spiro.set_guide(std::stoi(id.substr(12)));
    } else if (id.starts_with("spiro-insert-")) {
        spiro.set_insert(std::stoi(id.substr(13)));
    } else if (id == "spiro-remove") {
        spiro.remove_insert();
    } else if (id == "spiro-center") {
        spiro.center = {document.image.width * .5, document.image.height * .5};
    } else if (id == "spiro-clear-pegs") {
        spiro.pegs = {};
    } else if (id == "spiro-fill") {
        document.tool = Tool::Fill;
    } else if (id == "spiro-operate") {
        document.tool = Tool::Spirograph;
    } else if (id == "spiro-close") {
        spiro = {};
        document.tool = Tool::Pencil;
    }
    (*ribbon_).show_tool_context();
    refresh();
}
int Editor::spiro_hole_at(Point point, bool empty_only) const {
    int best = -1;
    double distance = 11 / (*canvas_).zoom();
    for (int i = 0; i < spiro.hole_count(); ++i) {
        if (empty_only && spiro.pegs[i].seated) {
            continue;
        }
        const Point hole = spiro.hole(i, spiro.angle);
        const double d = std::hypot(point.x - hole.x, point.y - hole.y);
        if (d < distance) {
            distance = d;
            best = i;
        }
    }
    return best;
}
void Editor::cancel_spiro_drag() {
    if (spiro_drag_ == SpiroDrag::Peg && spiro_origin_hole_ >= 0 && spiro.active) {
        spiro.seat(spiro_origin_hole_, spiro_carried_);
    }
    spiro_drag_ = SpiroDrag::None;
    spiro_origin_hole_ = spiro_target_hole_ = -1;
    spiro_checkpoint_ = false;
    if (ribbon_) {
        (*ribbon_).cancel_spiro_drag();
    }
}
void Editor::drop_spiro_peg() {
    if (spiro_target_hole_ >= 0) {
        spiro.seat(spiro_target_hole_, spiro_carried_);
    } else if (spiro_origin_hole_ >= 0) {
        const Point center = spiro.wheel_center(spiro.angle);
        if (std::hypot(spiro_pointer_.x - center.x, spiro_pointer_.y - center.y) <= spiro.wheel_radius()) {
            spiro.seat(spiro_origin_hole_, spiro_carried_);
        }
    }
    // Releasing away from a socket removes the peg and its ink.
    spiro_origin_hole_ = -1;
    spiro_drag_ = SpiroDrag::None;
    spiro_target_hole_ = -1;
    canvas().invalidate(gf::Dirty::paint);
}
void Editor::spiro_tray_pointer(int width, const gf::PointerEvent& event) {
    if (!spiro.active || !spiro.inserted) {
        return;
    }
    const gui_drawing::PointF mapped = canvas().client_to_bitmap(canvas().point_from_window(event.position));
    spiro_pointer_ = {mapped.x, mapped.y};
    if (event.action == gf::PointerAction::down) {
        release_gesture();
        spiro_carried_ = {true, false, {}, width};
        spiro_origin_hole_ = -1;
        spiro_drag_ = SpiroDrag::Peg;
        if (window()) {
            static_cast<void>((*window()).request_focus(canvas_));
        }
    }
    spiro_target_hole_ = spiro_hole_at(spiro_pointer_, true);
    if (event.action == gf::PointerAction::up) {
        drop_spiro_peg();
    }
    canvas().invalidate(gf::Dirty::paint);
}
bool Editor::spiro_pointer(const gf::PointerEvent& event, Point point) {
    if (!spiro.active || event.button == gf::PointerButton::middle ||
        event.action == gf::PointerAction::wheel) {
        return false;
    }
    if (spiro_drag_ != SpiroDrag::None) {
        if (event.action == gf::PointerAction::move || event.action == gf::PointerAction::up) {
            if (spiro_drag_ == SpiroDrag::Guide) {
                spiro.center = {point.x + spiro_grab_.x, point.y + spiro_grab_.y};
            } else if (spiro_drag_ == SpiroDrag::Peg) {
                spiro_pointer_ = point;
                spiro_target_hole_ = spiro_hole_at(point, true);
                if (event.action == gf::PointerAction::up) {
                    drop_spiro_peg();
                }
            } else {
                const double x = point.x + spiro_grab_.x - spiro.center.x,
                             y = point.y + spiro_grab_.y - spiro.center.y;
                if (std::hypot(x, y) > std::max(2.0, spiro.guide_radius() * .08)) {
                    const double target = spiro.angle + std::remainder(std::atan2(y, x) - spiro.angle, tau);
                    if (spiro.loaded() && std::abs(target - spiro.angle) > 1e-9 && !spiro_checkpoint_) {
                        document.settle_selection();
                        paint_base_ = document.image;
                        document.checkpoint();
                        spiro_checkpoint_ = true;
                    }
                    const std::vector<SpiroTrace> traces = spiro.advance(target);
                    for (const SpiroTrace& trace : traces) {
                        Ink ink;
                        ink.primary = trace.ink;
                        ink.size = trace.width;
                        ink.brush = Brush::Round;
                        ink.pattern = Pattern::Solid;
                        stroke(document.image, trace.start, trace.end, ink);
                    }
                    if (!traces.empty()) {
                        constrain_paint(document.image, paint_base_, guide,
                                        atlas_painting() && atlas_preserve_alpha);
                        document.constrain_selection(document.image, paint_base_);
                        ++canvas_revision;
                        double left = document.image.width, top = document.image.height, right = 0,
                               bottom = 0;
                        for (const SpiroTrace& trace : traces) {
                            const double margin = trace.width + 2;
                            left = std::min(left, std::min(trace.start.x, trace.end.x) - margin);
                            top = std::min(top, std::min(trace.start.y, trace.end.y) - margin);
                            right = std::max(right, std::max(trace.start.x, trace.end.x) + margin);
                            bottom = std::max(bottom, std::max(trace.start.y, trace.end.y) + margin);
                        }
                        const int x = static_cast<int>(std::floor(left)),
                                  y = static_cast<int>(std::floor(top));
                        const Rect damage{x, y, static_cast<int>(std::ceil(right)) - x,
                                          static_cast<int>(std::ceil(bottom)) - y};
                        publish_image(document.selection.active && !document.selection.on_canvas
                                          ? document.visible_image()
                                          : document.image,
                                      *canvas_, damage);
                        update_status();
                    }
                }
            }
            if (event.action == gf::PointerAction::up) {
                spiro_drag_ = SpiroDrag::None;
                spiro_checkpoint_ = false;
                canvas().set_pointer_capture(false);
            }
            canvas().invalidate(gf::Dirty::paint);
        }
        return true;
    }
    const Point close{spiro.center.x + (spiro.guide_radius() + 10 * spiro.scale) * .7071067811865476,
                      spiro.center.y - (spiro.guide_radius() + 10 * spiro.scale) * .7071067811865476};
    const double radial = std::hypot(point.x - spiro.center.x, point.y - spiro.center.y);
    const Point wheel = spiro.wheel_center(spiro.angle);
    const bool on_wheel = spiro.inserted && std::hypot(point.x - wheel.x, point.y - wheel.y) <=
                                                spiro.wheel_radius() + 3 * spiro.scale;
    const bool on_ring =
        std::abs(radial - (spiro.guide_radius() + 6 * spiro.scale)) < 10 * spiro.scale + 4 / canvas().zoom();
    const bool on_close = std::hypot(point.x - close.x, point.y - close.y) * canvas().zoom() < 11;
    const int hole = spiro_hole_at(point, false);
    if (event.action == gf::PointerAction::down && (on_wheel || on_ring || on_close || hole >= 0)) {
        if (on_close) {
            spiro_choice("spiro-close");
            return true;
        }
        if (document.tool == Tool::Fill && hole >= 0) {
            const Ink ink = event.button == gf::PointerButton::secondary ? document.alternate_ink()
                                                                         : document.primary_ink();
            spiro.fill(hole, ink.pattern == Pattern::None ? Color{0, 0, 0, 0} : ink.primary);
            canvas().invalidate(gf::Dirty::paint);
            return true;
        }
        if (document.tool == Tool::Fill && on_wheel &&
            std::hypot(point.x - wheel.x, point.y - wheel.y) * canvas().zoom() > 10) {
            return true;
        }
        if (event.button != gf::PointerButton::primary) {
            return true;
        }
        release_gesture();
        if (hole >= 0 && spiro.pegs[hole].seated) {
            spiro_carried_ = spiro.pegs[hole];
            spiro.pegs[hole] = {};
            spiro_origin_hole_ = hole;
            spiro_pointer_ = point;
            spiro_drag_ = SpiroDrag::Peg;
        } else if (on_wheel && std::hypot(point.x - wheel.x, point.y - wheel.y) * canvas().zoom() <= 10) {
            spiro_drag_ = SpiroDrag::Wheel;
            spiro_grab_ = {wheel.x - point.x, wheel.y - point.y};
        } else {
            spiro_drag_ = SpiroDrag::Guide;
            spiro_grab_ = {spiro.center.x - point.x, spiro.center.y - point.y};
        }
        if (window()) {
            static_cast<void>((*window()).request_focus(canvas_));
        }
        canvas().set_pointer_capture(true);
        canvas().invalidate(gf::Dirty::paint);
        return true;
    }
    if (event.action == gf::PointerAction::move && (on_wheel || on_ring || on_close || hole >= 0)) {
        canvas().set_cursor(gf::CursorKind::hand);
    }
    return document.tool == Tool::Spirograph;
}
} // namespace paint::forms
