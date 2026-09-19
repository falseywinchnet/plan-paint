#pragma once
#include "raster.hpp"
namespace paint {
enum class LassoMode { Free, Tighten, InnerVoid };
struct SelectionMask {
    Rect bounds;
    std::vector<std::uint8_t> coverage;
    std::vector<Point> outline;
};
SelectionMask tighten_lasso(const Image& image, const std::vector<Point>& polygon, bool inner_void,
                            double tolerance = 0.025);
std::vector<Point> mask_outline(const std::vector<std::uint8_t>& mask, int width, int height);
struct Guide {
    std::vector<Point> nodes;
    bool closed = false, fill = true;
    double width = 2;
    // A selection-derived mask preserves holes and feathered silhouettes until
    // a node edit replaces it with the editable polygon.
    SelectionMask selection;
    void clear();
    bool active() const;
    double blocked(int x, int y) const;
    void translate(Point delta);
};
void constrain_paint(Image& image, const Image& base, const Guide& guide, bool preserve_alpha);
void stencil_flood(Image& image, Point point, const Ink& ink, const Guide& guide, bool wrap);
} // namespace paint
