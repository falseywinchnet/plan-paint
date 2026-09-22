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
gf::Point profile_point(gf::Point center, double radius, const SpiroProfile& profile, double a,
                        double rotation) {
    const Point p = profile.point(a);
    const double c = std::cos(rotation), sn = std::sin(rotation);
    return {center.x + radius * (p.x * c - p.y * sn), center.y + radius * (p.x * sn + p.y * c)};
}
void profile_shape(gf::Painter& painter, gf::Point center, double r, const SpiroProfile& profile,
                   double rotation, gf::Color fill, gf::Color edge, double width) {
    if (profile.circular()) {
        if (fill.alpha > 0) {
            disc(painter, center, r, fill);
        }
        circle(painter, center, r, edge, width);
        return;
    }
    std::vector<gf::Point> points;
    double top = center.y, bottom = center.y;
    for (int i = 0; i < 96; ++i) {
        const gf::Point p = profile_point(center, r, profile, tau * i / 96, rotation);
        points.push_back(p);
        top = std::min(top, p.y);
        bottom = std::max(bottom, p.y);
    }
    if (fill.alpha > 0) {
        // Convex scan conversion keeps the temporary sheet transparent without overlapping fans.
        const double step = std::max(1.0, (bottom - top) / 600);
        for (double y = top; y < bottom; y += step) {
            double left = 1e20, right = -1e20;
            for (int i = 0; i < 96; ++i) {
                const gf::Point a = points[i], b = points[(i + 1) % 96];
                if ((a.y <= y + step * .5 && b.y > y + step * .5) ||
                    (b.y <= y + step * .5 && a.y > y + step * .5)) {
                    const double x = a.x + (b.x - a.x) * (y + step * .5 - a.y) / (b.y - a.y);
                    left = std::min(left, x);
                    right = std::max(right, x);
                }
            }
            if (right > left) {
                painter.fill_rect({left, y, right - left, step}, fill);
            }
        }
    }
    for (int i = 0; i < 96; ++i) {
        painter.draw_line(points[i], points[(i + 1) % 96], edge, width);
    }
}
double profile_clearance(Point point, Point center, double radius, const SpiroProfile& profile,
                         double rotation) {
    const double x = (point.x - center.x) * std::cos(rotation) + (point.y - center.y) * std::sin(rotation);
    const double y = -(point.x - center.x) * std::sin(rotation) + (point.y - center.y) * std::cos(rotation);
    double result = -1e20;
    for (int i = 0; i < 96; ++i) {
        const double a = tau * i / 96;
        result = std::max(result, x * std::cos(a) + y * std::sin(a) - radius * profile.support(a));
    }
    return result;
}
void peg_cap(gf::Painter& painter, gf::Point center, double r, const SpiroPeg& peg) {
    disc(painter, {center.x + 1, center.y + 2}, r + 1, gf::Color::rgba(10, 25, 50, 80));
    const Color ink = peg.loaded ? peg.ink : Color{222, 235, 245, 255};
    disc(painter, center, r, gf::Color::rgba(ink.r, ink.g, ink.b, 190));
    if (!peg.preview.pixels.empty()) {
        const double pixel = r * 1.6 / peg.preview.width;
        for (int y = 0; y < peg.preview.height; ++y) {
            for (int x = 0; x < peg.preview.width; ++x) {
                const Color c = peg.preview.get(x, y);
                if (c.a == 0) {
                    continue;
                }
                painter.fill_rect({center.x + (x - peg.preview.width * .5) * pixel,
                                   center.y + (y - peg.preview.height * .5) * pixel, pixel, pixel},
                                  gf::Color::rgba(c.r, c.g, c.b, c.a));
            }
        }
    }
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
    const SpiroProfile& profile = kind == 0 ? spiro_guides()[index].profile : spiro_inserts()[index].profile;
    if (kind == 0 && spiro_guides()[index].rack) {
        painter.draw_line({c.x - r * 1.4, c.y}, {c.x + r * 1.4, c.y}, gf::Color::rgba(60, 215, 70, 200), 7);
        for (int i = -6; i <= 6; ++i) {
            painter.draw_line({c.x + i * r / 5, c.y - 5}, {c.x + i * r / 5, c.y},
                              gf::Color::rgba(20, 130, 40), 1);
        }
        return;
    }
    profile_shape(painter, c, r, profile, 0,
                  kind == 0 ? gf::Color::rgba(0, 0, 0, 0) : gf::Color::rgba(0, 170, 255, 100),
                  kind == 0 ? gf::Color::rgba(60, 215, 70, 200) : gf::Color::rgba(10, 120, 225, 220),
                  kind == 0 ? 5 : 1.5);
    if (kind == 1) {
        for (const Point hole : spiro_inserts()[index].holes) {
            circle(painter, {c.x + hole.x * r, c.y + hole.y * r}, 1.4, gf::Color::rgba(250, 255, 255), 1);
        }
    }
}
void Editor::paint_spiro_overlay(gf::Painter& painter) {
    if (!spiro.active) {
        return;
    }
    const double zoom = canvas().zoom(), radius = spiro.guide_radius() * zoom;
    const gf::Point center = screen(spiro.center);
    const SpiroGuide& frame = spiro_guides()[spiro.guide];
    const double rim = std::max(8.0, 10 * spiro.scale * zoom);
    if (spiro.rack()) {
        const gf::Point a = screen(spiro.guide_point(-1.5)), b = screen(spiro.guide_point(1.5));
        painter.draw_line(a, b, gf::Color::rgba(55, 235, 65, 150), rim);
        const int marks = std::max(1, static_cast<int>(3 * frame.teeth / tau));
        for (int i = 0; i <= marks; ++i) {
            const Point p = spiro.guide_point(-1.5 + 3.0 * i / marks);
            painter.draw_line(screen({p.x, p.y - 3 * spiro.scale}), screen({p.x, p.y + 3 * spiro.scale}),
                              gf::Color::rgba(25, 140, 40, 230), std::max(1.0, spiro.scale * zoom));
        }
    } else {
        profile_shape(painter, center, radius + rim * .65, frame.profile, canvas().view_angle,
                      gf::Color::rgba(0, 0, 0, 0), gf::Color::rgba(55, 235, 65, 105), rim);
        profile_shape(painter, center, radius, frame.profile, canvas().view_angle,
                      gf::Color::rgba(0, 0, 0, 0), gf::Color::rgba(25, 145, 40, 210), 1.3);
        for (int i = 0; i < frame.teeth; ++i) {
            const double a = frame.profile.normal_at_arc(tau * i / frame.teeth);
            const gf::Point p = screen(spiro.guide_point(a));
            painter.draw_line(polar(p, -1.5 * spiro.scale * zoom, a + canvas().view_angle),
                              polar(p, 2 * spiro.scale * zoom, a + canvas().view_angle),
                              gf::Color::rgba(38, 200, 45, 185), std::max(1.0, 2 * spiro.scale * zoom));
        }
    }
    const gf::Point close = screen(spiro.close_position());
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
        const SpiroInsert& part = spiro_inserts()[spiro.insert];
        const double rotation = spiro.wheel_rotation(spiro.angle) + canvas().view_angle;
        profile_shape(painter, {wheel.x + 1, wheel.y + 2}, r, part.profile, rotation,
                      gf::Color::rgba(15, 45, 90, 25), gf::Color::rgba(15, 45, 90, 50), 1);
        profile_shape(painter, wheel, r, part.profile, rotation, gf::Color::rgba(0, 170, 255, 70),
                      gf::Color::rgba(15, 110, 215, 220), 1.3);
        for (int i = 0; i < part.teeth; ++i) {
            const double a = part.profile.normal_at_arc(tau * i / part.teeth);
            const gf::Point p = profile_point(wheel, r, part.profile, a, rotation);
            painter.draw_line(polar(p, -1.3 * spiro.scale * zoom, a + rotation),
                              polar(p, 1.3 * spiro.scale * zoom, a + rotation),
                              gf::Color::rgba(5, 151, 245, 215), std::max(1.0, 2 * spiro.scale * zoom));
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
                if (spiro.selected_peg == i) {
                    circle(painter, hole, 11, gf::Color::rgba(255, 200, 30, 65), 5);
                    circle(painter, hole, 10, gf::Color::rgba(255, 184, 20, 255), 2);
                }
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
    } else if (id == "spiro-outside") {
        spiro.outside = !spiro.outside;
        if (!spiro.compatible(spiro.guide, spiro.insert)) {
            spiro.outside = true;
        }
        spiro.angle = 0;
    } else if (id == "spiro-deselect") {
        spiro.selected_peg = -1;
    } else if (id == "spiro-center") {
        spiro.center = {document.image.width * .5, document.image.height * .5};
    } else if (id == "spiro-clear-pegs") {
        spiro.pegs = {};
        spiro.selected_peg = -1;
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
        spiro.select_peg(spiro_origin_hole_);
    }
    spiro_drag_ = SpiroDrag::None;
    spiro_origin_hole_ = spiro_target_hole_ = -1;
    spiro_checkpoint_ = false;
    spiro_stroke_.clear();
    if (ribbon_) {
        (*ribbon_).cancel_spiro_drag();
    }
}
void Editor::drop_spiro_peg() {
    if (spiro_target_hole_ >= 0) {
        spiro.seat(spiro_target_hole_, spiro_carried_);
        spiro.select_peg(spiro_target_hole_);
    } else if (spiro_origin_hole_ >= 0) {
        const Point center = spiro.wheel_center(spiro.angle);
        if (profile_clearance(spiro_pointer_, center, spiro.wheel_radius(),
                              spiro_inserts()[spiro.insert].profile,
                              spiro.wheel_rotation(spiro.angle)) <= 0) {
            spiro.seat(spiro_origin_hole_, spiro_carried_);
            spiro.select_peg(spiro_origin_hole_);
        }
    }
    // Releasing away from a socket removes the peg and its ink.
    spiro_origin_hole_ = -1;
    spiro_drag_ = SpiroDrag::None;
    spiro_target_hole_ = -1;
    spiro.select_peg(spiro.selected_peg);
    (*ribbon_).synchronize();
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
            if (spiro_drag_ == SpiroDrag::PegPending) {
                if (std::hypot(point.x - spiro_pointer_.x, point.y - spiro_pointer_.y) * canvas().zoom() >
                    4) {
                    spiro_carried_ = spiro.pegs[spiro_origin_hole_];
                    spiro.pegs[spiro_origin_hole_] = {};
                    spiro.selected_peg = -1;
                    spiro_drag_ = SpiroDrag::Peg;
                } else if (event.action == gf::PointerAction::up) {
                    spiro_drag_ = SpiroDrag::None;
                    spiro_origin_hole_ = -1;
                }
            }
            if (spiro_drag_ == SpiroDrag::None || spiro_drag_ == SpiroDrag::PegPending) {
                if (event.action == gf::PointerAction::up) {
                    canvas().set_pointer_capture(false);
                }
                return true;
            }
            if (spiro_drag_ == SpiroDrag::Guide) {
                spiro.center = {point.x + spiro_grab_.x, point.y + spiro_grab_.y};
            } else if (spiro_drag_ == SpiroDrag::Peg) {
                spiro_pointer_ = point;
                spiro_target_hole_ = spiro_hole_at(point, true);
                if (event.action == gf::PointerAction::up) {
                    drop_spiro_peg();
                }
            } else {
                const Point target_point{point.x + spiro_grab_.x, point.y + spiro_grab_.y};
                if (spiro.rack() ||
                    std::hypot(target_point.x - spiro.center.x, target_point.y - spiro.center.y) > 2) {
                    double target = spiro.project(target_point, spiro.angle);
                    if (std::abs(target - spiro.angle) <= 1e-9) {
                        target = spiro.angle;
                    }
                    if (spiro.loaded() && std::abs(target - spiro.angle) > 1e-9 && !spiro_checkpoint_) {
                        document.settle_selection();
                        paint_base_ = document.image;
                        document.checkpoint();
                        spiro_checkpoint_ = true;
                        spiro_stroke_.clear();
                    }
                    const std::vector<SpiroTrace> traces = spiro.advance(target);
                    const Rect damage = spiro_stroke_.render(document.image, paint_base_, spiro, traces);
                    if (!traces.empty()) {
                        constrain_paint(document.image, paint_base_, guide,
                                        atlas_painting() && atlas_preserve_alpha);
                        document.constrain_selection(document.image, paint_base_);
                        ++canvas_revision;
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
                spiro_stroke_.clear();
                canvas().set_pointer_capture(false);
            }
            canvas().invalidate(gf::Dirty::paint);
        }
        return true;
    }
    const Point close = spiro.close_position();
    const Point wheel = spiro.wheel_center(spiro.angle);
    const bool on_wheel =
        spiro.inserted &&
        profile_clearance(point, wheel, spiro.wheel_radius(), spiro_inserts()[spiro.insert].profile,
                          spiro.wheel_rotation(spiro.angle)) < 3 * spiro.scale;
    const double margin = 10 * spiro.scale + 4 / canvas().zoom();
    const bool on_ring = spiro.rack()
                             ? std::abs(point.y - spiro.center.y) < margin &&
                                   std::abs(point.x - spiro.center.x) < 1.5 * spiro.guide_radius() + margin
                             : std::abs(profile_clearance(point, spiro.center, spiro.guide_radius(),
                                                          spiro_guides()[spiro.guide].profile, 0) -
                                        6 * spiro.scale) < margin;
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
            if (spiro.fill(hole, ink.pattern == Pattern::None ? Color{0, 0, 0, 0} : ink.primary)) {
                spiro.select_peg(hole);
                (*ribbon_).synchronize();
            }
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
            spiro.select_peg(hole);
            (*ribbon_).synchronize();
            spiro_origin_hole_ = hole;
            spiro_pointer_ = point;
            spiro_drag_ = SpiroDrag::PegPending;
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
    if (event.action == gf::PointerAction::down && spiro.selected_peg >= 0) {
        spiro.selected_peg = -1;
        (*ribbon_).synchronize();
        canvas().invalidate(gf::Dirty::paint);
    }
    return document.tool == Tool::Spirograph;
}
} // namespace paint::forms
