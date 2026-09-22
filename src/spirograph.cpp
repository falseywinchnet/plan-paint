#include "spirograph.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>
namespace paint {
const std::array<SpiroGuide, 2>& spiro_guides() {
    static const std::array<SpiroGuide, 2> parts{{{"Ring 96", 96}, {"Ring 120", 120}}};
    return parts;
}
const std::array<SpiroInsert, 3>& spiro_inserts() {
    static const std::array<SpiroInsert, 3> parts{
        {{"Wheel 36", 36, {{.28, 0}, {.54, 0}, {.8, 0}, {-.4, .4}, {-.5, -.3}}},
         {"Wheel 40", 40, {{.25, 0}, {.52, 0}, {.8, 0}, {-.32, .38}, {-.4, -.4}, {.1, -.65}}},
         {"Wheel 48", 48, {{.25, 0}, {.5, 0}, {.78, 0}, {-.25, .35}, {-.55, .3}, {-.5, -.35}, {.1, -.65}}}}};
    return parts;
}
void Spirograph::open(int width, int height) {
    *this = {};
    active = true;
    center = {width * .5, height * .5};
    scale = std::clamp(std::min(width, height) / 280.0, .12, 1.35);
}
void Spirograph::set_insert(int index) {
    if (index < 0 || index >= static_cast<int>(spiro_inserts().size())) {
        throw std::out_of_range("Spirograph insert");
    }
    insert = index;
    inserted = true;
    angle = 0;
    pegs = {};
}
void Spirograph::remove_insert() {
    inserted = false;
    pegs = {};
}
void Spirograph::set_guide(int index) {
    if (index < 0 || index >= static_cast<int>(spiro_guides().size())) {
        throw std::out_of_range("Spirograph guide");
    }
    guide = index;
}
double Spirograph::guide_radius() const {
    return spiro_guides().at(guide).teeth * scale;
}
double Spirograph::wheel_radius() const {
    return spiro_inserts().at(insert).teeth * scale;
}
double Spirograph::wheel_rotation(double phase) const {
    return -(guide_radius() - wheel_radius()) / wheel_radius() * phase;
}
Point Spirograph::wheel_center(double phase) const {
    const double d = guide_radius() - wheel_radius();
    return {center.x + d * std::cos(phase), center.y + d * std::sin(phase)};
}
Point Spirograph::hole(int index, double phase) const {
    const Point local = spiro_inserts().at(insert).holes.at(index);
    const Point wheel = wheel_center(phase);
    const double rotation = wheel_rotation(phase), c = std::cos(rotation), s = std::sin(rotation),
                 r = wheel_radius();
    return {wheel.x + r * (c * local.x - s * local.y), wheel.y + r * (s * local.x + c * local.y)};
}
int Spirograph::hole_count() const {
    return inserted ? static_cast<int>(spiro_inserts().at(insert).holes.size()) : 0;
}
int Spirograph::closing_turns() const {
    const int outer = spiro_guides().at(guide).teeth, inner = spiro_inserts().at(insert).teeth;
    return inner / std::gcd(outer, inner);
}
bool Spirograph::loaded() const {
    if (!active || !inserted) {
        return false;
    }
    for (int i = 0; i < hole_count(); ++i) {
        if (pegs[i].seated && pegs[i].loaded && pegs[i].ink.a) {
            return true;
        }
    }
    return false;
}
bool Spirograph::seat(int index, SpiroPeg peg) {
    if (!active || index < 0 || index >= hole_count() || pegs[index].seated) {
        return false;
    }
    peg.seated = true;
    peg.width = std::clamp(peg.width, 1, 8);
    pegs[index] = peg;
    return true;
}
bool Spirograph::fill(int index, Color ink) {
    if (index < 0 || index >= hole_count() || !pegs[index].seated) {
        return false;
    }
    pegs[index].ink = ink;
    pegs[index].loaded = ink.a != 0;
    return true;
}
std::vector<SpiroTrace> Spirograph::advance(double target) {
    if (!std::isfinite(target) || !std::isfinite(angle) || !std::isfinite(scale) || scale <= 0 ||
        std::abs(target - angle) > 100 * std::numbers::pi) {
        throw std::invalid_argument("Invalid spirograph motion");
    }
    std::vector<SpiroTrace> result;
    if (!active || !inserted) {
        return result;
    }
    const double delta = target - angle;
    // Bound pen travel to half an image pixel, including the insert's counter-rotation.
    const double speed = 2 * (guide_radius() - wheel_radius());
    const double count = std::ceil(std::abs(delta) * speed / .5);
    if (!std::isfinite(count) || count > 200000) {
        throw std::invalid_argument("Spirograph motion too large");
    }
    const int steps = std::max(1, static_cast<int>(count));
    if (loaded() && std::abs(delta) > 1e-12) {
        for (int i = 1; i <= steps; ++i) {
            const double a = angle + delta * (i - 1) / steps, b = angle + delta * i / steps;
            for (int h = 0; h < hole_count(); ++h) {
                const SpiroPeg& peg = pegs[h];
                if (peg.seated && peg.loaded && peg.ink.a) {
                    result.push_back({hole(h, a), hole(h, b), peg.ink, peg.width});
                }
            }
        }
    }
    angle = target;
    return result;
}
} // namespace paint
