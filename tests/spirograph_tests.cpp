#include "codecs.hpp"
#include "paint_tools.hpp"
#include "raster.hpp"
#include "spirograph.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
namespace {
void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
double distance(paint::Point a, paint::Point b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}
void extended_geometry() {
    paint::Spirograph s;
    s.open(640, 480);
    int checked = 0;
    for (int external = 0; external < 2; ++external) {
        s.outside = external != 0;
        for (int g = 0; g < static_cast<int>(paint::spiro_guides().size()); ++g) {
            for (int w = 0; w < static_cast<int>(paint::spiro_inserts().size()); ++w) {
                if (!s.compatible(g, w)) {
                    continue;
                }
                // Install together: compatibility is checked for the pair under test.
                s.guide = g;
                s.insert = w;
                s.angle = 0;
                s.pegs = {};
                ++checked;
                const paint::SpiroProfile& profile = paint::spiro_inserts()[w].profile;
                for (int i = 0; i < 16; ++i) {
                    const double a = .013 + i * .33, epsilon = 1e-5;
                    const paint::Point c = s.wheel_center(a), contact = s.guide_point(a);
                    const double rotation = s.wheel_rotation(a);
                    const double normal =
                        (s.rack() ? std::numbers::pi / 2 : a + (external ? std::numbers::pi : 0)) - rotation;
                    const paint::Point local = profile.point(normal);
                    const paint::Point world{c.x + s.wheel_radius() * (local.x * std::cos(rotation) -
                                                                       local.y * std::sin(rotation)),
                                             c.y + s.wheel_radius() * (local.x * std::sin(rotation) +
                                                                       local.y * std::cos(rotation))};
                    require(distance(contact, world) < 1e-8, "noncircular pitch curves lose contact");
                    const paint::Point before = s.wheel_center(a - epsilon),
                                       after = s.wheel_center(a + epsilon);
                    const double spin =
                        (s.wheel_rotation(a + epsilon) - s.wheel_rotation(a - epsilon)) / (2 * epsilon);
                    const paint::Point velocity{
                        (after.x - before.x) / (2 * epsilon) - spin * (contact.y - c.y),
                        (after.y - before.y) / (2 * epsilon) + spin * (contact.x - c.x)};
                    require(std::hypot(velocity.x, velocity.y) < 3e-4, "noncircular rolling contact slips");
                    if (!external && !s.rack()) {
                        const paint::SpiroProfile& frame = paint::spiro_guides()[g].profile;
                        for (int q = 0; q < 32; ++q) {
                            const paint::Point edge = profile.point(q * 2 * std::numbers::pi / 32);
                            const paint::Point p{c.x + s.wheel_radius() * (edge.x * std::cos(rotation) -
                                                                           edge.y * std::sin(rotation)),
                                                 c.y + s.wheel_radius() * (edge.x * std::sin(rotation) +
                                                                           edge.y * std::cos(rotation))};
                            for (int n = 0; n < 32; ++n) {
                                const double normal_angle = n * 2 * std::numbers::pi / 32;
                                require((p.x - s.center.x) * std::cos(normal_angle) +
                                                (p.y - s.center.y) * std::sin(normal_angle) <=
                                            s.guide_radius() * frame.support(normal_angle) + 1e-8,
                                        "compatible wheel crosses the frame");
                            }
                        }
                    }
                }
                if (!s.rack()) {
                    const double cycle = 2 * std::numbers::pi * s.closing_turns();
                    for (int h = 0; h < s.hole_count(); ++h) {
                        require(distance(s.hole(h, 0), s.hole(h, cycle)) < 1e-7,
                                "noncircular pattern fails closure");
                    }
                }
                s.seat(0, {true, true, {25, 90, 170, 255}, 2});
                const std::vector<paint::SpiroTrace> traces = s.advance(.2);
                for (const paint::SpiroTrace& trace : traces) {
                    require(distance(trace.start, trace.end) <= .501,
                            "expanded geometry produces long pen chords");
                }
                const double target = s.rack() ? .25 : .27;
                const double projected = s.project(s.wheel_center(target), .2);
                require(std::abs(projected - target) < 1e-6, "pointer projection misses nearby wheel center");
            }
        }
    }
    require(checked > 600, "expanded component set has too few working pairs");
    std::cout << checked
              << " inside/outside component pairs pass contact, no-slip, closure and projection checks\n";
}
void gel_and_peg_media() {
    paint::Spirograph s;
    s.open(320, 240);
    require(!s.assign_brush(paint::Brush::Gel), "unselected peg accepts brush");
    s.seat(0, {true, true, {25, 70, 160, 255}, 2});
    s.seat(1, {true, true, {170, 30, 60, 255}, 4});
    require(s.select_peg(0) && s.assign_brush(paint::Brush::Gel), "selected peg cannot use gel");
    require(s.pegs[1].effect.brush == paint::Brush::Round && s.pegs[0].width == 2 && s.pegs[0].ink.b == 160,
            "assigning medium changes another peg, its width or ink");
    paint::Ink ink = s.peg_ink(0);
    ink.size = 3;
    paint::Image a, b;
    a.reset(160, 80, {0, 0, 0, 0});
    b = a;
    paint::DynamicBrushStroke whole, split;
    whole.segment(a, {10, 20}, {140, 46}, ink, false);
    for (int i = 0; i < 26; ++i) {
        split.segment(b, {10.0 + 5 * i, 20.0 + i}, {15.0 + 5 * i, 21.0 + i}, ink, false);
    }
    for (std::size_t i = 0; i < a.pixels.size(); ++i) {
        require(paint::equal(a.pixels[i], b.pixels[i]), "gel changes with mouse event subdivision");
    }
    const paint::MaterialSurface surface(ink, ink.brush);
    require(surface.sample(32, 20, 1).a == 255, "gel body is transparent");
    paint::Image base;
    base.reset(160, 80, {245, 241, 235, 255});
    paint::Image first = base, second = base;
    paint::SpirographStroke combined, batched;
    s.pegs[0].width = s.pegs[1].width = 6;
    s.select_peg(1);
    s.assign_brush(paint::Brush::Watercolor);
    const std::vector<paint::SpiroTrace> cross{{{10, 20}, {140, 46}, s.pegs[0].ink, 6, 0},
                                               {{10, 46}, {140, 20}, s.pegs[1].ink, 6, 1}};
    combined.render(first, base, s, cross);
    for (int i = 0; i < 26; ++i) {
        const std::vector<paint::SpiroTrace> part{
            {{10.0 + 5 * i, 20.0 + i}, {15.0 + 5 * i, 21.0 + i}, s.pegs[0].ink, 6, 0},
            {{10.0 + 5 * i, 46.0 - i}, {15.0 + 5 * i, 45.0 - i}, s.pegs[1].ink, 6, 1}};
        batched.render(second, base, s, part);
    }
    for (std::size_t i = 0; i < first.pixels.size(); ++i) {
        require(paint::equal(first.pixels[i], second.pixels[i]),
                "crossed peg media change with event batching");
    }
    s.remove_insert();
    require(s.selected_peg == -1 && !s.assign_brush(paint::Brush::Watercolor),
            "removed insert retains peg selection");
}
void draw_gallery(const char* path) {
    paint::Image image;
    image.reset(1080, 720, {250, 250, 247, 255});
    const std::array<int, 6> guides{6, 8, 9, 12, 0, 14};
    const std::array<int, 6> inserts{15, 0, 17, 22, 1, 18};
    const std::array<paint::Brush, 6> media{paint::Brush::Gel, paint::Brush::Watercolor, paint::Brush::Marker,
                                            paint::Brush::Gel, paint::Brush::Pencil,     paint::Brush::Gel};
    for (int cell = 0; cell < 6; ++cell) {
        paint::Spirograph s;
        s.open(300, 300);
        s.guide = guides[cell];
        s.insert = inserts[cell];
        s.outside = cell == 4;
        s.center = {180.0 + (cell % 3) * 360, 180.0 + (cell / 3) * 360};
        s.scale = (s.outside ? 80.0 : 120.0) / paint::spiro_guides()[s.guide].teeth;
        s.angle = s.rack() ? -1.25 : 0;
        s.seat(0, {true, true, {25, 80, 172, 255}, 2});
        s.seat(2, {true, true, {160, 32, 108, 255}, 2});
        s.select_peg(0);
        s.assign_brush(media[cell]);
        s.select_peg(2);
        s.assign_brush(media[cell]);
        paint::SpirographStroke brushes;
        const paint::Image base = image;
        const double limit = s.rack() ? 1.25 : 2 * std::numbers::pi * s.closing_turns();
        const std::vector<paint::SpiroTrace> traces = s.advance(limit);
        brushes.render(image, base, s, traces);
    }
    paint::save_image(image, path);
}
} // namespace
int main(int argc, char** argv) {
    try {
        extended_geometry();
        gel_and_peg_media();
        paint::Spirograph s;
        s.open(960, 640);
        for (int guide = 0; guide < 2; ++guide) {
            s.set_guide(guide);
            for (int insert = 0; insert < 3; ++insert) {
                s.set_insert(insert);
                const double cycle = 2 * std::numbers::pi * s.closing_turns();
                for (int h = 0; h < s.hole_count(); ++h) {
                    require(distance(s.hole(h, 0), s.hole(h, cycle)) < 1e-9,
                            "gear pattern fails exact closure");
                }
                for (int i = 0; i < 100; ++i) {
                    const double a = i * .17;
                    require(std::abs(distance(s.center, s.wheel_center(a)) + s.wheel_radius() -
                                     s.guide_radius()) < 1e-10,
                            "wheel loses ring contact");
                    const double travel = (s.guide_radius() - s.wheel_radius()) * a;
                    require(std::abs(travel + s.wheel_radius() * s.wheel_rotation(a)) < 1e-10,
                            "rolling contact slips");
                }
            }
        }
        s.set_insert(0);
        require(s.advance(1).empty(), "empty insert draws ink");
        require(!s.fill(0, {200, 20, 30, 255}), "empty hole accepts ink without a peg");
        require(s.seat(0, {true, false, {}, 1}), "peg will not seat");
        require(!s.seat(0, {true, false, {}, 2}), "occupied hole replaces an existing peg");
        require(s.fill(0, {200, 20, 30, 255}), "peg will not load ink");
        s.seat(2, {true, true, {20, 60, 200, 255}, 3});
        const std::vector<paint::SpiroTrace> trace = s.advance(1.4);
        require(trace.size() > 20 && trace.size() % 2 == 0, "loaded pegs do not trace together");
        for (const paint::SpiroTrace& t : trace) {
            require(distance(t.start, t.end) <= .501, "underresolved trace connects a coarse chord");
        }
        const paint::Point before = s.hole(0, s.angle);
        s.center.x += 30;
        s.center.y -= 15;
        const paint::Point after = s.hole(0, s.angle);
        require(std::abs(after.x - before.x - 30) < 1e-10 && std::abs(after.y - before.y + 15) < 1e-10,
                "guide fails to carry its insert");
        s.remove_insert();
        require(!s.loaded() && !s.pegs[0].seated, "insert removal retains ink or pegs");
        s.set_insert(1);
        require(!s.loaded(), "replacement insert inherits old ink");
        if (argc > 2) {
            draw_gallery(argv[2]);
        }
        if (argc > 1) {
            paint::Image image;
            image.reset(640, 480);
            paint::Spirograph example;
            example.open(640, 480);
            example.seat(0, {true, true, {190, 44, 75, 255}, 1});
            example.seat(2, {true, true, {24, 99, 181, 255}, 1});
            example.seat(3, {true, true, {18, 145, 108, 255}, 1});
            const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            const std::vector<paint::SpiroTrace> traces =
                example.advance(2 * std::numbers::pi * example.closing_turns());
            for (const paint::SpiroTrace& trace : traces) {
                paint::Ink ink;
                ink.primary = trace.ink;
                ink.size = trace.width;
                paint::stroke(image, trace.start, trace.end, ink);
            }
            const double ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
            paint::save_image(image, argv[1]);
            std::cout << traces.size() << " pen segments, full three-peg closed pattern: " << ms
                      << " ms in core renderer\n";
        }
        std::cout << "Spirograph closure, no-slip contact, peg loading, multi-pen tracing and removal pass\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
