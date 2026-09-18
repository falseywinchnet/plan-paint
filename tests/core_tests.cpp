#include "codecs.hpp"
#include "conv.hpp"
#include "document.hpp"
#include "fixtures/conv_reference.hpp"
#include "material.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
namespace {
void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
void test_color() {
    for (int r = 0; r <= 255; r += 17) {
        for (int g = 0; g <= 255; g += 17) {
            for (int b = 0; b <= 255; b += 17) {
                paint::Color color{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                                   static_cast<std::uint8_t>(b), 255};
                paint::Color roundtrip = paint::from_oklab(paint::to_oklab(color));
                require(paint::equal(color, roundtrip), "OKLab roundtrip differs");
                paint::Color parsed;
                require(paint::from_hex(paint::to_hex(color), parsed) && paint::equal(parsed, color),
                        "hex roundtrip differs");
            }
        }
    }
    paint::Color invalid;
    require(!paint::from_hex("#12zz34", invalid), "invalid hex accepted");
}
void test_conv() {
    paint::Image input, output;
    input.reset(13, 9, {53, 177, 229, 149});
    const int sizes[6][2] = {{29, 17}, {7, 5}, {1, 1}, {1, 19}, {31, 1}, {13, 9}};
    for (const int (&size)[2] : sizes) {
        paint::conv_resize(input, size[0], size[1], output);
        for (paint::Color pixel : output.pixels) {
            require(paint::equal(pixel, {53, 177, 229, 149}), "CONV constant preservation failed");
        }
    }
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            input.set(x, y, {static_cast<std::uint8_t>(x * 17), static_cast<std::uint8_t>(y * 23), 0, 255});
        }
    }
    paint::conv_resize(input, 25, 17, output);
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            require(paint::equal(input.get(x, y), output.get(x * 2, y * 2)),
                    "CONV nodal interpolation failed");
        }
    }
    // An unchanged dimension must preserve its rows/columns, including when
    // source and destination alias. These fixtures exercise both one-axis paths.
    paint::conv_resize(input, 25, 9, output);
    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 13; ++x) {
            require(paint::equal(input.get(x, y), output.get(x * 2, y)), "width-only resize moved a row");
        }
    }
    paint::Image original = input;
    paint::conv_resize(input, 13, 17, input);
    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 13; ++x) {
            require(paint::equal(original.get(x, y), input.get(x, y * 2)),
                    "height-only alias resize moved a column");
        }
    }
    input.reset(1, 3, {20, 40, 60, 255});
    paint::conv_resize(input, 7, 1, output);
    for (paint::Color pixel : output.pixels) {
        require(paint::equal(pixel, {20, 40, 60, 255}), "short line failed");
    }
}
void test_conv_reference() {
    for (const fixtures::Case& fixture : fixtures::cases) {
        paint::Image input, output;
        input.reset(fixture.width, fixture.height);
        std::memcpy(input.pixels.data(), fixture.input, input.pixels.size() * 4);
        paint::conv_resize(input, fixture.out_width, fixture.out_height, output);
        const std::uint8_t* actual = reinterpret_cast<const std::uint8_t*>(output.pixels.data());
        for (std::size_t i = 0; i < output.pixels.size() * 4; ++i) {
            if (actual[i] != fixture.expected[i]) {
                std::cerr << "reference difference at byte " << i << ": " << static_cast<int>(actual[i])
                          << " vs " << static_cast<int>(fixture.expected[i]) << '\n';
                throw std::runtime_error("Native CONV differs from website JavaScript reference");
            }
        }
    }
}
void test_editing() {
    paint::Document doc;
    doc.new_image(20, 20);
    doc.checkpoint();
    doc.ink.primary = {0, 0, 0, 255};
    doc.ink.pattern = paint::Pattern::Dither50;
    paint::flood(doc.image, 0, 0, doc.ink);
    int dark = 0;
    for (paint::Color pixel : doc.image.pixels) {
        if (pixel.r == 0) {
            ++dark;
        }
    }
    require(dark == 200, "pattern bucket region/density failed");
    doc.undo();
    require(doc.image.get(0, 0).r == 255, "undo failed");
    doc.redo();
    require(doc.image.get(0, 0).r == 0, "redo failed");
    doc.select({0, 0, 10, 10});
    doc.selection.x = 10;
    doc.selection.y = 10;
    doc.commit_selection();
    doc.undo();
    require(doc.image.get(0, 0).r == 0, "selection transaction undo failed");
    paint::Image stamp = paint::make_stamp(doc.image, {0, 0, 10, 10}, paint::StampShape::Circle, false, {});
    require(stamp.get(0, 0).a == 0 && stamp.get(5, 5).a == 255, "stamp mask failed");
    paint::Image textured;
    textured.reset(20, 20);
    paint::Ink fill_ink;
    fill_ink.secondary = {0, 0, 0, 255};
    paint::draw_shape(textured, paint::Shape::Rectangle, {1, 1}, {18, 18}, fill_ink, false, true,
                      paint::Brush::Watercolor);
    require(textured.get(10, 10).r > 0 && textured.get(10, 10).r < 250,
            "watercolor shape fill ignored its medium");
}
void test_materials_and_shapes() {
    paint::Ink ink;
    ink.primary = {42, 88, 139, 210};
    ink.secondary = ink.primary;
    ink.size = 28;
    for (int i = 4; i < paint::brush_count; ++i) {
        ink.brush = static_cast<paint::Brush>(i);
        paint::Image whole, split;
        whole.reset(224, 110, {230, 220, 200, 255});
        split = whole;
        paint::MaterialStroke a, b;
        a.segment(whole, {10, 25}, {210, 75}, ink);
        b.segment(split, {10, 25}, {60, 37.5}, ink);
        b.segment(split, {60, 37.5}, {130, 55}, ink);
        b.segment(split, {130, 55}, {210, 75}, ink);
        require(std::memcmp(whole.pixels.data(), split.pixels.data(), whole.pixels.size() * 4) == 0,
                "material coat depends on collinear mouse event subdivision");
        b.segment(split, {10, 25}, {210, 75}, ink);
        require(std::memcmp(whole.pixels.data(), split.pixels.data(), whole.pixels.size() * 4) == 0,
                "revisiting a material coat multiplied its opacity");
        paint::MaterialSurface surface(ink, ink.brush);
        int minimum = 255, maximum = 0;
        for (int y = 0; y < 40; ++y) {
            for (int x = 0; x < 40; ++x) {
                int alpha = surface.sample(x, y, 12).a;
                minimum = std::min(minimum, alpha);
                maximum = std::max(maximum, alpha);
            }
        }
        require(maximum - minimum > 3, "natural medium has no spatial material variation");
        paint::Ink dry = ink;
        dry.pigment_load = 0;
        paint::MaterialSurface empty(dry, dry.brush);
        require(empty.sample(10, 20, 3).a == 0, "zero paint load deposited pigment");
        paint::Image fill;
        fill.reset(140, 120, {255, 255, 255, 255});
        paint::draw_shape(fill, paint::Shape::Rectangle, {5, 5}, {130, 110}, ink, false, true, ink.brush);
        require(fill.get(60, 60).r < 255 && fill.get(0, 0).r == 255,
                "material fill leaked outside its region");
    }
    ink.brush = paint::Brush::Watercolor;
    paint::MaterialSurface water(ink, ink.brush);
    require(water.sample(20, 20, 0.5).a > water.sample(20, 20, 20).a,
            "watercolor rim did not accumulate pigment");
    paint::MaterialSurface original(ink, ink.brush);
    ++ink.noise;
    paint::MaterialSurface reseeded(ink, ink.brush);
    unsigned changes = 0;
    for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 24; ++x) {
            changes += !paint::equal(original.sample(x, y, 10), reseeded.sample(x, y, 10));
        }
    }
    require(changes > 500, "new grain did not regenerate the procedural surface");
    const paint::Point ends[4] = {{230, 100}, {-70, 100}, {230, -100}, {-70, -100}};
    for (int i = 0; i < 4; ++i) {
        paint::Point start{40, 30};
        std::vector<paint::Point> circle = paint::shape_points(paint::Shape::Circle, start, ends[i]);
        double min_x = circle[0].x, max_x = min_x, min_y = circle[0].y, max_y = min_y;
        for (std::size_t j = 0; j < circle.size(); ++j) {
            min_x = std::min(min_x, circle[j].x);
            max_x = std::max(max_x, circle[j].x);
            min_y = std::min(min_y, circle[j].y);
            max_y = std::max(max_y, circle[j].y);
        }
        require(std::abs((max_x - min_x) - (max_y - min_y)) < 1e-9, "Circle permitted unequal diameters");
        double cx = (min_x + max_x) / 2, cy = (min_y + max_y) / 2, r = (max_x - min_x) / 2;
        for (std::size_t j = 0; j < circle.size(); ++j) {
            require(std::abs(std::hypot(circle[j].x - cx, circle[j].y - cy) - r) < 1e-9,
                    "Circle is not circular");
        }
    }
    for (int i = static_cast<int>(paint::Shape::Circle); i < paint::shape_count; ++i) {
        std::vector<paint::Point> points =
            paint::shape_points(static_cast<paint::Shape>(i), {0, 0}, {100, 100});
        require(points.size() >= (static_cast<paint::Shape>(i) == paint::Shape::Arc ? 2 : 3),
                "shape has no valid outline");
        for (std::size_t j = 0; j < points.size(); ++j) {
            require(std::isfinite(points[j].x) && std::isfinite(points[j].y),
                    "new shape has invalid geometry");
        }
    }
}
void test_eraser_and_pixel_target() {
    paint::Image hard, soft, split;
    hard.reset(64, 64, {40, 100, 170, 200});
    soft = hard;
    split = hard;
    paint::EraserStroke eraser;
    eraser.segment(hard, {20.5, 20.5}, {40.5, 20.5}, 12, false);
    require(hard.get(30, 20).a == 0 && hard.get(20, 20).a == 0, "hard eraser left opacity");
    require(hard.get(15, 15).a == 200, "round eraser used a square footprint");
    eraser.clear();
    eraser.segment(soft, {20.5, 20.5}, {40.5, 20.5}, 12, true);
    require(soft.get(30, 20).a == 0 && soft.get(30, 22).a > 0 && soft.get(30, 25).a > soft.get(30, 22).a,
            "soft eraser has no center-to-edge opacity taper");
    eraser.clear();
    for (int x = 20; x < 40; ++x) {
        eraser.segment(split, {x + .5, 20.5}, {x + 1.5, 20.5}, 12, true);
    }
    require(std::equal(split.pixels.begin(), split.pixels.end(), soft.pixels.begin(), paint::equal),
            "soft eraser strength depends on pointer event frequency");
    eraser.segment(split, {20.5, 20.5}, {40.5, 20.5}, 12, true);
    require(std::equal(split.pixels.begin(), split.pixels.end(), soft.pixels.begin(), paint::equal),
            "same eraser gesture compounded opacity");
    paint::Image pencil;
    pencil.reset(4, 4, {255, 255, 255, 255});
    paint::Ink ink;
    ink.primary = {0, 0, 0, 255};
    paint::pixel_line(pencil, {1.99, 1.99}, {1.99, 1.99}, ink);
    require(pencil.get(1, 1).r == 0 && pencil.get(2, 2).r == 255, "pencil rounded into neighboring pixel");
}
double reference_segment_distance(paint::Point p, paint::Point a, paint::Point b) {
    double dx = b.x - a.x, dy = b.y - a.y, length2 = dx * dx + dy * dy;
    double t = length2 > 0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / length2, 0.0, 1.0) : 0;
    return std::hypot(p.x - a.x - t * dx, p.y - a.y - t * dy);
}
void test_continuous_geometry_coverage() {
    const int widths[3] = {1, 3, 7};
    for (int w = 0; w < 3; ++w) {
        double radius = widths[w] * .5, expected = 80 * widths[w] + std::numbers::pi * radius * radius;
        for (int degrees = 0; degrees < 180; degrees += 15) {
            double angle = degrees * std::numbers::pi / 180;
            paint::Point a{64 - 40 * std::cos(angle), 64 - 40 * std::sin(angle)};
            paint::Point b{64 + 40 * std::cos(angle), 64 + 40 * std::sin(angle)};
            paint::Image image;
            image.reset(128, 128, {0, 0, 0, 0});
            paint::Ink ink;
            ink.size = widths[w];
            paint::MaterialStroke coat;
            coat.segment(image, a, b, ink);
            double area = 0;
            for (std::size_t i = 0; i < image.pixels.size(); ++i) {
                area += image.pixels[i].a / 255.0;
            }
            if (std::abs(area - expected) >= std::max(.8, expected * .009)) {
                std::cerr << "width=" << widths[w] << " angle=" << degrees << " area=" << area
                          << " expected=" << expected << "\n";
            }
            require(std::abs(area - expected) < std::max(.8, expected * .009),
                    "stroke thickness changes with angle");
        }
    }
    // Independent 64x64 area integration checks shared fill/outline edge compositing.
    std::vector<paint::Point> triangle = {{12.2, 8.7}, {57.6, 26.3}, {19.4, 57.1}};
    paint::Image image;
    image.reset(70, 70, {255, 255, 255, 255});
    paint::Ink ink;
    ink.primary = {0, 0, 0, 151};
    ink.secondary = {80, 120, 180, 97};
    ink.size = 1;
    paint::polygon(image, triangle, ink, true, true, true, paint::Brush::Round);
    for (int y = 6; y < 60; ++y) {
        for (int x = 10; x < 60; ++x) {
            double distance = 100;
            for (int edge = 0; edge < 3; ++edge) {
                distance =
                    std::min(distance, reference_segment_distance({double(x), double(y)}, triangle[edge],
                                                                  triangle[(edge + 1) % 3]));
            }
            if (distance > 1.3) {
                continue;
            }
            double expected[3] = {0, 0, 0};
            for (int sy = 0; sy < 64; ++sy) {
                for (int sx = 0; sx < 64; ++sx) {
                    paint::Point sample{x + (sx + .5) / 64 - .5, y + (sy + .5) / 64 - .5};
                    double color[3] = {255, 255, 255};
                    if (paint::inside_polygon(triangle, sample.x, sample.y)) {
                        const int fill[3] = {80, 120, 180};
                        for (int c = 0; c < 3; ++c) {
                            color[c] = fill[c] * (97.0 / 255) + 255 * (1 - 97.0 / 255);
                        }
                    }
                    bool stroke = false;
                    for (int edge = 0; edge < 3; ++edge) {
                        if (reference_segment_distance(sample, triangle[edge], triangle[(edge + 1) % 3]) <=
                            .5) {
                            stroke = true;
                        }
                    }
                    for (int c = 0; c < 3; ++c) {
                        expected[c] += color[c] * (stroke ? 1 - 151.0 / 255 : 1) / 4096;
                    }
                }
            }
            paint::Color actual = image.get(x, y);
            require(std::abs(actual.r - expected[0]) < 3 && std::abs(actual.g - expected[1]) < 3 &&
                        std::abs(actual.b - expected[2]) < 3,
                    "fill/outline edge differs from independent area integration");
        }
    }
}
void test_stamp_masks_and_oblique_edges() {
    paint::Image source;
    source.reset(80, 64, {75, 130, 190, 255});
    for (int i = 0; i < paint::stamp_shape_count; ++i) {
        paint::StampShape shape = static_cast<paint::StampShape>(i);
        std::vector<paint::Point> points = paint::stamp_outline(shape, 58, 42);
        require(points.size() >= 2, "stamp has no usable outline");
        for (const paint::Point& point : points) {
            require(point.x >= 0 && point.y >= 0 && point.x <= 58 && point.y <= 42,
                    "stamp outline escapes its capture bounds");
        }
        for (const paint::Point& point : paint::stamp_outline(shape, 1, 1)) {
            require(point.x >= 0 && point.y >= 0 && point.x <= 1 && point.y <= 1,
                    "minimum stamp size has out-of-bounds geometry");
        }
        paint::Image mask = paint::make_stamp(source, {0, 0, 58, 42}, shape, false, {255, 255, 255, 255});
        int visible = 0;
        for (const paint::Color& pixel : mask.pixels) {
            visible += pixel.a > 0;
        }
        require(visible > 20, "stamp shape produced an empty mask");
        if (shape == paint::StampShape::Bezier || shape == paint::StampShape::Arc) {
            require(points.size() > 10, "curved stamp degenerated to a straight segment");
        }
    }
    paint::Ink ink;
    ink.size = 1;
    // Check geometric support and connectivity at awkward, fractional angles.
    // Antialiasing may soften a boundary but must never deposit isolated islands.
    for (int degrees = 1; degrees < 180; degrees += 7) {
        double angle = (degrees + 0.37) * std::numbers::pi / 180;
        paint::Point a{48.23 - 32 * std::cos(angle), 48.41 - 32 * std::sin(angle)};
        paint::Point b{48.23 + 32 * std::cos(angle), 48.41 + 32 * std::sin(angle)};
        std::vector<paint::Point> nodes{a, b, {44.71, 77.23}};
        for (int closed = 0; closed < 2; ++closed) {
            paint::Image image;
            image.reset(96, 96, {0, 0, 0, 0});
            if (closed) {
                paint::polygon(image, nodes, ink, true, false);
            } else {
                paint::stroke(image, a, b, ink);
            }
            int first = -1, total = 0;
            for (int y = 0; y < 96; ++y) {
                for (int x = 0; x < 96; ++x) {
                    if (image.get(x, y).a == 0) {
                        continue;
                    }
                    if (first < 0) {
                        first = y * 96 + x;
                    }
                    ++total;
                    double distance = reference_segment_distance({double(x), double(y)}, a, b);
                    if (closed) {
                        distance = std::min(distance,
                                            reference_segment_distance({double(x), double(y)}, b, nodes[2]));
                        distance = std::min(distance,
                                            reference_segment_distance({double(x), double(y)}, nodes[2], a));
                    }
                    require(distance <= 0.5 + std::sqrt(0.5),
                            "solid edge grew pixels outside its geometric support");
                }
            }
            require(first >= 0, "smooth line disappeared");
            std::vector<bool> visited(96 * 96);
            std::vector<int> queue{first};
            visited[first] = true;
            for (std::size_t i = 0; i < queue.size(); ++i) {
                int x = queue[i] % 96, y = queue[i] / 96;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx, ny = y + dy, index = ny * 96 + nx;
                        if (nx < 0 || ny < 0 || nx >= 96 || ny >= 96 || visited[index]) {
                            continue;
                        }
                        if (image.get(nx, ny).a) {
                            visited[index] = true;
                            queue.push_back(index);
                        }
                    }
                }
            }
            require(static_cast<int>(queue.size()) == total, "solid edge contains detached pixels");
        }
    }
}
void test_curves() {
    paint::CurveGeometry curve;
    curve.set_line({10, 10}, {110, 10});
    curve.move_handle(0, {10, 110});
    curve.move_handle(1, {110, 110});
    paint::Point quarter = curve.at(0.25), middle = curve.at(0.5);
    require(std::abs(quarter.x - 25.625) < 1e-10 && std::abs(quarter.y - 66.25) < 1e-10 && middle.x == 60 &&
                middle.y == 85 && curve.handle_count() == 2,
            "cubic Bézier does not follow its independent endpoint controls");
    curve.set_line({0, 0}, {100, 0});
    curve.move_handle(0, {-200, 0});
    curve.move_handle(1, {300, 0});
    std::vector<paint::Point> samples = curve.samples();
    double left = 0, right = 100;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        paint::Point point = samples[i];
        left = std::min(left, point.x);
        right = std::max(right, point.x);
    }
    require(left < -40 && right > 140, "Bézier flattening discarded collinear control reversals");

    curve.kind = paint::CurveKind::Arc;
    curve.set_line({-50, 0}, {50, 0});
    curve.move_handle(0, {20, 50});
    require(curve.handle_count() == 1 && curve.handle(0).x == 0 && curve.handle(0).y == 50,
            "arc midpoint left the perpendicular bisector");
    quarter = curve.at(0.25);
    require(std::abs(quarter.x + 50 / std::sqrt(2.0)) < 1e-10 &&
                std::abs(quarter.y - 50 / std::sqrt(2.0)) < 1e-10,
            "arc is not a circular semicircle through its midpoint handle");
    samples = curve.samples();
    for (std::size_t i = 1; i < samples.size(); ++i) {
        paint::Point point = samples[i], previous = samples[i - 1];
        require(std::abs(std::hypot(point.x, point.y) - 50) < 1e-9, "arc samples left the circle");
        double chord_radius = std::hypot((point.x + previous.x) / 2, (point.y + previous.y) / 2);
        require(50 - chord_radius <= 0.125, "arc sampling exceeded its pixel error tolerance");
    }
    curve.move_handle(0, {0, -50});
    require(curve.at(0.5).y == -50 && curve.at(0.25).y < 0, "arc cannot bend across the baseline");
    curve.move_handle(0, {0, 100});
    samples = curve.samples();
    for (std::size_t i = 0; i < samples.size(); ++i) {
        paint::Point point = samples[i];
        require(std::abs(std::hypot(point.x, point.y - 37.5) - 62.5) < 1e-9,
                "major arc radius or center is incorrect");
    }
    curve.move_handle(0, {0, 1e-8});
    require(std::isfinite(curve.at(0.25).y) && std::abs(curve.at(0.25).y) < 1e-7,
            "nearly straight arc is numerically unstable");
    curve.set_line({4, 10}, {4, 110});
    curve.move_handle(0, {-46, 60});
    require(curve.at(0.5).x == -46 && curve.at(0.5).y == 60, "vertical arc handle is misplaced");

    paint::Document doc;
    doc.new_image(128, 128);
    doc.ink.primary = {20, 70, 140, 130};
    doc.ink.size = 4;
    doc.begin_curve(paint::CurveKind::Bezier, {10, 20});
    require(!doc.dirty() && doc.undo_history.empty(), "first curve click changed the picture or history");
    require(!doc.establish_curve({10, 20}) && doc.undo_history.empty(), "zero-length curve was accepted");
    doc.commit_curve();
    require(!doc.curve.base && !doc.dirty(), "releasing a lone anchor preserved a mark");
    doc.begin_curve(paint::CurveKind::Bezier, {10, 20});
    require(doc.establish_curve({110, 20}), "curve starting line was not accepted");
    paint::Image baseline = doc.image;
    doc.checkpoint();
    doc.curve.geometry.move_handle(0, {10, 110});
    doc.sync_curve();
    paint::Image bent = doc.image;
    doc.undo();
    require(doc.curve.line_set && doc.curve.geometry.first_control.y == 20 &&
                std::equal(baseline.pixels.begin(), baseline.pixels.end(), doc.image.pixels.begin(),
                           paint::equal),
            "curve undo failed to restore both its editable handle and its raster");
    doc.redo();
    require(doc.curve.line_set && doc.curve.geometry.first_control.y == 110 &&
                std::equal(bent.pixels.begin(), bent.pixels.end(), doc.image.pixels.begin(), paint::equal),
            "curve redo lost its editable geometry or raster");
    doc.saved_revision = doc.revision;
    doc.sync_curve();
    require(!doc.dirty() && doc.curve.line_set, "saved curve was flattened or left dirty");
    doc.commit_curve();
    require(!doc.curve.base &&
                std::equal(bent.pixels.begin(), bent.pixels.end(), doc.image.pixels.begin(), paint::equal),
            "releasing a curve repainted translucent coverage");
    doc.undo();
    require(!doc.curve.base && std::equal(baseline.pixels.begin(), baseline.pixels.end(),
                                          doc.image.pixels.begin(), paint::equal),
            "undo after curve release resurrected handles or lost the prior raster");
    doc.undo();
    require(!doc.curve.base && doc.image.get(50, 20).r == 255, "curve baseline undo failed");
}
void test_line_smoothing() {
    paint::Image smooth, hard;
    smooth.reset(64, 64, {255, 255, 255, 255});
    hard = smooth;
    paint::Ink ink;
    ink.size = 3;
    paint::stroke(smooth, {7, 9}, {53, 37}, ink);
    ink.smooth = false;
    paint::stroke(hard, {7, 9}, {53, 37}, ink);
    int gray = 0, black = 0;
    for (std::size_t i = 0; i < hard.pixels.size(); ++i) {
        require(hard.pixels[i].r == 0 || hard.pixels[i].r == 255,
                "hard lines contain only whole-pixel edges");
        if (hard.pixels[i].r == 0) {
            ++black;
        }
        if (smooth.pixels[i].r > 0 && smooth.pixels[i].r < 255) {
            ++gray;
        }
    }
    require(gray > 20 && black > 80,
            "smoothing toggle preserves a solid stroke while changing edge coverage");
}
void test_path_history() {
    paint::Document doc;
    doc.new_image(64, 64);
    doc.continuous_path = true;
    doc.ink.primary = {0, 0, 0, 170};
    doc.ink.size = 3;
    doc.add_path_node({8, 8});
    doc.add_path_node({48, 8});
    paint::Image line = doc.image;
    doc.add_path_node({48, 48});
    paint::Image corner = doc.image;
    require(doc.undo_history.size() == 3, "path nodes were grouped into one undo step");
    doc.end_path_geometry();
    require(doc.path.nodes.size() == 3 && !doc.path.extending && doc.undo_history.size() == 3,
            "ending geometry discarded junctions or inserted an empty undo step");
    doc.add_path_node({8, 8});
    require(std::equal(corner.pixels.begin(), corner.pixels.end(), doc.image.pixels.begin(), paint::equal),
            "a new run connected itself to the previous endpoint");
    doc.add_path_node({8, 48});
    require(doc.image.get(8, 30).r < 255, "branch from a retained junction is missing");
    doc.undo();
    require(doc.path.nodes.size() == 4 && doc.path.extending && doc.image.get(8, 30).r == 255 &&
                doc.image.get(48, 30).r < 255,
            "branch undo removed old geometry or kept the new segment");
    doc.undo();
    require(doc.path.nodes.size() == 3 && !doc.path.extending,
            "undo did not remove the new run's starting node");
    doc.undo();
    require(doc.path.nodes.size() == 2 && doc.path.extending &&
                std::equal(line.pixels.begin(), line.pixels.end(), doc.image.pixels.begin(), paint::equal),
            "undo across a stopped run did not restore the prior editable segment");
    doc.redo();
    require(
        doc.path.nodes.size() == 3 && !doc.path.extending &&
            std::equal(corner.pixels.begin(), corner.pixels.end(), doc.image.pixels.begin(), paint::equal),
        "redo lost nodes, the stopped state, or translucent coverage");
    doc.commit_path();
    doc.undo();
    require(doc.path.nodes.empty() && doc.path.session == 0 && doc.image.get(48, 30).r == 255 &&
                doc.image.get(30, 8).r < 255,
            "released path undo resurrected controls or erased multiple segments");
    doc.redo();
    require(doc.path.nodes.empty() && doc.image.get(48, 30).r < 255,
            "released path redo resurrected controls or lost pixels");

    doc.new_image(64, 64);
    doc.add_path_node({8, 8});
    doc.add_path_node({48, 8});
    doc.undo();
    doc.undo();
    require(doc.path.nodes.empty() && !doc.dirty(), "undoing every node did not restore a clean canvas");
    doc.redo();
    require(doc.path.nodes.size() == 1, "redo could not restore the first live node");
    doc.add_path_node({8, 48});
    require(doc.redo_history.empty() && doc.image.get(30, 8).r == 255 && doc.image.get(8, 30).r < 255,
            "a replacement segment retained the abandoned redo branch");
    doc.saved_revision = doc.revision;
    doc.sync_path();
    require(!doc.dirty(), "saved live path remained dirty solely because of its nodes");
    doc.ink.primary = {200, 10, 30, 255};
    doc.sync_path();
    require(doc.dirty(), "changing a saved live path's appearance did not mark the document dirty");

    doc.new_image(64, 64);
    doc.checkpoint();
    doc.image.set(2, 2, {10, 20, 30, 255});
    doc.add_path_node({8, 8});
    doc.add_path_node({48, 8});
    doc.undo();
    doc.undo();
    doc.undo();
    require(doc.path.nodes.empty() && doc.image.get(2, 2).r == 255,
            "undo through the path's first node did not reach earlier image edits");
    doc.redo();
    doc.redo();
    doc.redo();
    require(doc.path.nodes.size() == 2 && doc.path.extending && doc.image.get(2, 2).r == 10,
            "redo through earlier image edits lost the still-active path session");
    paint::Image transformed;
    transformed.reset(32, 32, {40, 80, 120, 255});
    doc.checkpoint();
    doc.assign_canvas(transformed);
    doc.sync_path();
    require(doc.path.session == 0 && doc.image.width == 32 && doc.image.get(8, 8).b == 120,
            "live path controls overwrote a whole-image transform result");
    doc.undo();
    require(doc.path.session == 0 && doc.image.width == 64 && doc.image.get(2, 2).r == 10,
            "transform undo lost the source drawing or restored obsolete path controls");
}
void test_codecs() {
    paint::Image image;
    image.reset(17, 13, {62, 147, 219, 255});
    image.set(4, 7, {197, 27, 55, 255});
    const char* formats[] = {"png", "bmp", "tga", "tiff", "webp", "gif", "jpg"};
    for (const char* format : formats) {
        std::filesystem::path file =
            std::filesystem::temp_directory_path() / (std::string("rainstar-codec-test.") + format);
        paint::save_image(image, file.string());
        paint::save_image(image, file.string()); // Replacing an existing picture must work too.
        paint::Image loaded = paint::load_image(file.string());
        require(loaded.width == 17 && loaded.height == 13, "codec dimensions failed");
        if (std::string(format) != "jpg" && std::string(format) != "gif") {
            require(paint::equal(loaded.get(4, 7), image.get(4, 7)), "lossless codec failed");
        }
        std::filesystem::remove(file);
        std::filesystem::path unicode = std::filesystem::temp_directory_path() /
                                        std::filesystem::path(std::u8string(u8"Rainstar-é-绘画."));
        unicode += format;
        std::u8string encoded_path = unicode.u8string();
        std::string path(encoded_path.begin(), encoded_path.end());
        paint::save_image(image, path);
        paint::save_image(image, path);
        loaded = paint::load_image(path);
        require(loaded.width == image.width && loaded.height == image.height,
                "Unicode filename roundtrip failed");
        std::filesystem::remove(unicode);
    }
}
} // namespace
int main() {
    try {
        test_color();
        test_conv();
        test_conv_reference();
        test_editing();
        test_materials_and_shapes();
        test_eraser_and_pixel_target();
        test_continuous_geometry_coverage();
        test_line_smoothing();
        test_path_history();
        test_stamp_masks_and_oblique_edges();
        test_curves();
        test_codecs();
        std::cout << "Color, CONV, editing and seven-format codec tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
