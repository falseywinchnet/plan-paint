#pragma once
#include "image.hpp"
#include <array>
namespace paint {
struct SpiroInsert {
    const char* name;
    int teeth;
    std::vector<Point> holes;
};
struct SpiroGuide {
    const char* name;
    int teeth;
};
const std::array<SpiroGuide, 2>& spiro_guides();
const std::array<SpiroInsert, 3>& spiro_inserts();
struct SpiroPeg {
    bool seated = false, loaded = false;
    Color ink{};
    int width = 1;
};
struct SpiroTrace {
    Point start, end;
    Color ink;
    int width;
};
struct Spirograph {
    bool active = false, inserted = true;
    int guide = 0, insert = 0;
    double scale = 1.35, angle = 0;
    Point center{};
    std::array<SpiroPeg, 8> pegs{};
    void open(int width, int height);
    void set_insert(int index);
    void remove_insert();
    void set_guide(int index);
    double guide_radius() const;
    double wheel_radius() const;
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
} // namespace paint
