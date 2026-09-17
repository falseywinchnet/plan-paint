#pragma once
#include "image.hpp"
#include <array>
#include <cstddef>
#include <vector>

namespace paint {
// Source-to-target map of pixel-centre coordinates. Identity preserves samples.
struct AffineMap {
    double xx = 1.0, xy = 0.0, tx = 0.0;
    double yx = 0.0, yy = 1.0, ty = 0.0;
};
struct WarpSample {
    double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
};

// Owns an immutable double-precision, shared tensor-quintic CONV* atlas.
// Compilation allocates ~800 bytes/source pixel plus temporary work storage.
// Inputs are capped at 1,048,576 padded source pixels before allocation.
// Compile once per selected material; reuse for every rotation/mesh preview.
// compile() prepares a replacement before publishing; old state survives failure.
class ConvWarpField {
  public:
    void compile(const Image &source);
    [[nodiscard]] WarpSample sample_premultiplied(Point source_position) const;
    [[nodiscard]] Color sample(Point source_position) const;
    [[nodiscard]] int width() const {
        return width_;
    }
    [[nodiscard]] int height() const {
        return height_;
    }
    [[nodiscard]] std::size_t storage_bytes() const;

  private:
    int width_ = 0, height_ = 0, lattice_width_ = 0;
    std::vector<double> controls_;
};

enum class WarpSampling { Point, Area };

// Area uses positive bounded 2/4/8 point Gauss rules per target axis, an
// approximation to the pixel footprint integral. Integer lattice isometries
// use exact point samples. Transparent outside the
// source pixel footprint; edge centres extend constantly for half a pixel.
// All output pixels are replaced. Invalid maps leave destination unchanged.
// affine_bounds includes every target pixel basin intersecting the transformed
// source footprint. Subtract returned x/y from map.tx/map.ty before rendering.
[[nodiscard]] Rect affine_bounds(const ConvWarpField &field, const AffineMap &source_to_target);
void render_affine(const ConvWarpField &field, const AffineMap &source_to_target, int width, int height,
                   Image &destination, WarpSampling sampling = WarpSampling::Area);

struct MeshNode {
    Point source;
    Point target;
    bool boundary = false;
};
struct MeshTriangle {
    std::array<std::size_t, 3> nodes = {};
};
struct ReshapeMesh {
    std::vector<MeshNode> nodes;
    std::vector<MeshTriangle> triangles;
};

// A simple lasso outline in local image coordinates. Counterclockwise and
// clockwise input are accepted; self-intersections and zero area are rejected.
// Boundary edges are subdivided and interior points inserted at spacing.
[[nodiscard]] ReshapeMesh make_reshape_mesh(const std::vector<Point> &outline, double spacing);
// Rectangle helper; use the polygon overload for the actual lasso outline.
[[nodiscard]] ReshapeMesh make_reshape_mesh(const Image &source, double spacing);
// Rejects inverted/near-degenerate triangles and intersecting boundary edges.
// Mesh topology must come from make_reshape_mesh; only target nodes may change.
// Failure preserves the previous target point.
[[nodiscard]] bool move_reshape_node(ReshapeMesh &mesh, std::size_t node, Point target);
[[nodiscard]] bool reshape_mesh_valid(const ReshapeMesh &mesh);
// Mesh geometry is piecewise affine. Pixel values always use the CONV* atlas.
void render_mesh(const ConvWarpField &field, const ReshapeMesh &mesh, int width, int height,
                 Image &destination, WarpSampling sampling = WarpSampling::Area);
} // namespace paint
