#pragma once
#include "image.hpp"
namespace paint {
struct GeometryStyle {
    double corner_radius = 0;
};
// Circular fillets are tangent to both adjacent edges. Each consumes at most
// half of either edge, so large radii cannot reverse short edges or overlap.
std::vector<Point> soften_corners(const std::vector<Point>& points, double radius, bool closed,
                                  double tolerance = 0.125);
} // namespace paint
