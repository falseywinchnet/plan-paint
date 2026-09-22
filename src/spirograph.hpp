#pragma once
#include "paint_tools.hpp"
#include <array>
namespace paint {
// Convex pitch curves parameterized by outward normal, with exact arc length.
struct SpiroProfile {
    double oval = 0, triangle = 0, square = 0, pentagon = 0, hexagon = 0;
    bool circular() const;
    double support(double normal) const;
    double derivative(double normal) const;
    double curvature_radius(double normal) const;
    double arc(double normal) const;
    double normal_at_arc(double arc) const;
    Point point(double normal) const;
};
struct SpiroInsert {
    const char* name;
    int teeth;
    std::vector<Point> holes;
    SpiroProfile profile{};
};
struct SpiroGuide {
    const char* name;
    int teeth;
    SpiroProfile profile{};
    bool rack = false;
};
const std::vector<SpiroGuide>& spiro_guides();
const std::vector<SpiroInsert>& spiro_inserts();
struct SpiroPeg {
    bool seated = false, loaded = false;
    Color ink{};
    int width = 1;
    Ink effect{};
    bool glitter = false;
    Image preview{};
};
struct SpiroTrace {
    Point start, end;
    Color ink;
    int width;
    int peg = 0;
};
struct Spirograph {
    bool active = false, inserted = true;
    int guide = 0, insert = 0, selected_peg = -1;
    bool outside = false;
    double scale = 1.35, angle = 0;
    Point center{};
    std::array<SpiroPeg, 8> pegs{};
    void open(int width, int height);
    void set_insert(int index);
    void remove_insert();
    void set_guide(int index);
    double guide_radius() const;
    double wheel_radius() const;
    Point guide_point(double normal) const;
    Point close_position() const;
    double project(Point point, double near_phase) const;
    bool rack() const;
    bool compatible(int guide_index, int insert_index) const;
    bool select_peg(int index);
    bool assign_brush(Brush brush);
    Ink peg_ink(int index) const;
    void refresh_peg(int index);
    double wheel_rotation(double phase) const;
    Point wheel_center(double phase) const;
    Point hole(int index, double phase) const;
    int hole_count() const;
    int closing_turns() const;
    bool loaded() const;
    bool seat(int hole_index, SpiroPeg peg);
    bool fill(int hole_index, Color ink);
    std::vector<SpiroTrace> advance(double target);
};
// Sparse independent coats give crossings a stable peg order without eight full canvases.
class SpirographStroke {
    Image scratch_;
    std::array<std::unordered_map<int, Color>, 8> layers_;
    std::array<MaterialStroke, 8> material_;
    std::array<DynamicBrushStroke, 8> dynamic_;

  public:
    void clear();
    Rect render(Image& image, const Image& base, const Spirograph& apparatus,
                const std::vector<SpiroTrace>& traces);
};
} // namespace paint
