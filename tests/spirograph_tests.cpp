#include "codecs.hpp"
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
} // namespace
int main(int argc, char** argv) {
    try {
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
