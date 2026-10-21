#include "document.hpp"
#include "geometry_style.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
bool near(paint::Point a, paint::Point b) {
    return std::hypot(a.x - b.x, a.y - b.y) < 1e-8;
}
void corner_geometry() {
    const std::vector<paint::Point> square{{0, 0}, {100, 0}, {100, 100}, {0, 100}};
    const std::vector<paint::Point> unchanged = paint::soften_corners(square, 0, true);
    require(unchanged.size() == square.size(), "zero softness must preserve original vertices");
    for (std::size_t i = 0; i < square.size(); ++i) {
        require(near(square[i], unchanged[i]), "zero radius changed a point");
    }
    const std::vector<paint::Point> rounded = paint::soften_corners(square, 10, true);
    require(rounded.size() > 12 && near(rounded.front(), {0, 10}),
            "fillet must start at the incoming tangent");
    bool found_tangent = false;
    for (paint::Point point : rounded) {
        require(point.x >= -1e-8 && point.y >= -1e-8 && point.x <= 100 + 1e-8 && point.y <= 100 + 1e-8,
                "rounded polygon escaped its original bounds");
        if (point.x <= 10 && point.y <= 10) {
            require(std::abs(std::hypot(point.x - 10, point.y - 10) - 10) < 1e-8,
                    "corner is not a circular arc of the requested radius");
        }
        found_tangent = found_tangent || near(point, {10, 0});
    }
    require(found_tangent, "outgoing fillet tangent was omitted");
    const std::vector<paint::Point> concave{{0, 0}, {100, 0}, {100, 50}, {50, 50}, {50, 100}, {0, 100}};
    const std::vector<paint::Point> softened = paint::soften_corners(concave, 10, true);
    bool found_concave_arc = false;
    for (paint::Point point : softened) {
        if (point.x > 50 && point.x < 60 && point.y > 50 && point.y < 60) {
            require(std::abs(std::hypot(point.x - 60, point.y - 60) - 10) < 1e-8,
                    "concave corner must follow the opposite circular arc");
            found_concave_arc = true;
        }
    }
    require(found_concave_arc, "concave corner remained sharp");
    const std::vector<paint::Point> short_path{{0, 0}, {0, 2}, {4, 2}};
    const std::vector<paint::Point> limited = paint::soften_corners(short_path, 4096, false);
    require(near(limited.front(), short_path.front()) && near(limited.back(), short_path.back()),
            "softening must preserve open path endpoints");
    for (paint::Point point : limited) {
        require(point.x >= -1e-8 && point.x <= 4 && point.y >= -1e-8 && point.y <= 2,
                "large radius reversed a short edge");
    }
    const std::vector<paint::Point> duplicates{{0, 0}, {0, 0}, {100, 0}, {100, 100}, {0, 100}, {0, 0}};
    require(paint::soften_corners(duplicates, 10, true).size() == rounded.size(),
            "duplicate vertices changed the fillets");
    require(paint::soften_corners({{0, 0}, {10, std::numeric_limits<double>::infinity()}, {20, 20}}, 10, true)
                .empty(),
            "nonfinite corner geometry must be rejected before rasterization");
}
void filled_geometry() {
    for (bool supersample : {false, true}) {
        paint::Image image;
        image.reset(100, 100, {0, 0, 0, 0});
        paint::Ink ink;
        ink.geometry.corner_radius = 20;
        ink.supersample = supersample;
        ink.secondary = {30, 80, 190, 128};
        paint::draw_shape(image, paint::Shape::Rectangle, {10, 10}, {90, 90}, ink, false, true);
        require(image.get(11, 11).a == 0 && image.get(40, 40).a == 128,
                "rounded fill must omit corners and preserve interior opacity");
        require(image.get(10, 45).a > 0, "softening must retain straight edge tangency");
    }
    paint::Document document;
    document.ink.geometry.corner_radius = 17;
    require(document.alternate_ink().geometry.corner_radius == 17,
            "alternate-color geometry must use the same corner setting");
}
void path_history() {
    paint::Document document;
    document.new_image(100, 100);
    document.ink.geometry.corner_radius = 15;
    document.shape_fill = true;
    document.continuous_path = false;
    document.add_path_node({10, 10});
    document.add_path_node({90, 10});
    document.add_path_node({90, 90});
    document.add_path_node({10, 90});
    document.end_path_geometry();
    const paint::Image original = document.image;
    document.ink.geometry.corner_radius = 0;
    document.sync_path();
    require(std::equal(original.pixels.begin(), original.pixels.end(), document.image.pixels.begin(),
                       paint::equal),
            "later corner settings must not change retained path runs");
    document.checkpoint();
    document.move_path_node(0, {25, 25});
    const paint::Image moved = document.image;
    require(!std::equal(original.pixels.begin(), original.pixels.end(), moved.pixels.begin(), paint::equal),
            "moving retained rounded geometry must redraw its pixels");
    document.undo();
    require(document.path.runs.front().ink.geometry.corner_radius == 15 &&
                std::equal(original.pixels.begin(), original.pixels.end(), document.image.pixels.begin(),
                           paint::equal),
            "undo must restore rounded path style and its original pixels");
    document.redo();
    require(std::equal(moved.pixels.begin(), moved.pixels.end(), document.image.pixels.begin(), paint::equal),
            "redo must restore the rounded node edit exactly");
}
} // namespace
int main() {
    try {
        corner_geometry();
        filled_geometry();
        path_history();
        std::cout << "Corner geometry and raster contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
