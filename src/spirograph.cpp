#include "spirograph.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <unordered_set>
namespace paint {
namespace {
constexpr double tau = 2 * std::numbers::pi;
std::array<double, 5> harmonics(const SpiroProfile& p) {
    return {p.oval, p.triangle, p.square, p.pentagon, p.hexagon};
}
Point rotated(Point p, double a) {
    return {p.x * std::cos(a) - p.y * std::sin(a), p.x * std::sin(a) + p.y * std::cos(a)};
}
std::vector<Point> sockets(int count) {
    std::vector<Point> result;
    for (int i = 0; i < count; ++i) {
        const double a = i * 2.399963229728653;
        const double r = .22 + .48 * i / std::max(1, count - 1);
        result.push_back({r * std::cos(a), r * std::sin(a)});
    }
    return result;
}
} // namespace
bool SpiroProfile::circular() const {
    return oval == 0 && triangle == 0 && square == 0 && pentagon == 0 && hexagon == 0;
}
double SpiroProfile::support(double normal) const {
    if (circular()) {
        return 1;
    }
    double value = 1;
    const std::array<double, 5> coefficients = harmonics(*this);
    for (int i = 0; i < 5; ++i) {
        value += coefficients[i] * std::cos((i + 2) * normal);
    }
    return value;
}
double SpiroProfile::derivative(double normal) const {
    if (circular()) {
        return 0;
    }
    double value = 0;
    const std::array<double, 5> coefficients = harmonics(*this);
    for (int i = 0; i < 5; ++i) {
        value -= coefficients[i] * (i + 2) * std::sin((i + 2) * normal);
    }
    return value;
}
double SpiroProfile::curvature_radius(double normal) const {
    if (circular()) {
        return 1;
    }
    double value = 1;
    const std::array<double, 5> coefficients = harmonics(*this);
    for (int i = 0; i < 5; ++i) {
        const double n = i + 2;
        value += coefficients[i] * (1 - n * n) * std::cos(n * normal);
    }
    return value;
}
double SpiroProfile::arc(double normal) const {
    if (circular()) {
        return normal;
    }
    double value = normal;
    const std::array<double, 5> coefficients = harmonics(*this);
    for (int i = 0; i < 5; ++i) {
        const double n = i + 2;
        value += coefficients[i] * (1 - n * n) / n * std::sin(n * normal);
    }
    return value;
}
double SpiroProfile::normal_at_arc(double length) const {
    if (circular()) {
        return length;
    }
    // Monotone arc map; retain complete turns so the wheel phase cannot jump at a seam.
    const double turns = std::floor(length / tau), local = length - turns * tau;
    double lo = 0, hi = tau, x = local;
    for (int i = 0; i < 24; ++i) {
        const double residual = arc(x) - local;
        if (std::abs(residual) < 1e-13) {
            break;
        }
        if (residual < 0) {
            lo = x;
        } else {
            hi = x;
        }
        const double next = x - residual / curvature_radius(x);
        x = next > lo && next < hi ? next : (lo + hi) * .5;
    }
    return x + turns * tau;
}
Point SpiroProfile::point(double normal) const {
    const double h = support(normal), d = derivative(normal);
    return {h * std::cos(normal) - d * std::sin(normal), h * std::sin(normal) + d * std::cos(normal)};
}
const std::vector<SpiroGuide>& spiro_guides() {
    static const std::vector<SpiroGuide> parts{{"Ring 96", 96},
                                               {"Ring 120", 120},
                                               {"Ring 144", 144},
                                               {"Ring 180", 180},
                                               {"Ring 210", 210},
                                               {"Ring 240", 240},
                                               {"Oval 240", 240, {.18}},
                                               {"Long oval 300", 300, {.26}},
                                               {"Triangle 288", 288, {0, .07}},
                                               {"Square 288", 288, {0, 0, .036}},
                                               {"Pentagon 300", 300, {0, 0, 0, .022}},
                                               {"Hexagon 360", 360, {0, 0, 0, 0, .015}},
                                               {"Egg 288", 288, {.10, .035}},
                                               {"Shield 300", 300, {-.09, .035, .012}},
                                               {"Rack 96", 96, {}, true},
                                               {"Long rack 144", 144, {}, true}};
    return parts;
}
const std::vector<SpiroInsert>& spiro_inserts() {
    static const std::vector<SpiroInsert> parts{
        {"Wheel 36", 36, {{.28, 0}, {.54, 0}, {.8, 0}, {-.4, .4}, {-.5, -.3}}},
        {"Wheel 40", 40, {{.25, 0}, {.52, 0}, {.8, 0}, {-.32, .38}, {-.4, -.4}, {.1, -.65}}},
        {"Wheel 48", 48, sockets(7)},
        {"Wheel 12", 12, sockets(3)},
        {"Wheel 18", 18, sockets(4)},
        {"Wheel 24", 24, sockets(5)},
        {"Wheel 30", 30, sockets(6)},
        {"Prime 31", 31, sockets(6)},
        {"Prime 37", 37, sockets(7)},
        {"Wheel 45", 45, sockets(7)},
        {"Wheel 56", 56, sockets(8)},
        {"Wheel 60", 60, sockets(8)},
        {"Wheel 64", 64, sockets(8)},
        {"Wheel 72", 72, sockets(8)},
        {"Wheel 80", 80, sockets(8)},
        {"Oval 36", 36, sockets(6), {.18}},
        {"Long oval 40", 40, sockets(6), {.25}},
        {"Triangle 42", 42, sockets(7), {0, .07}},
        {"Square 48", 48, sockets(7), {0, 0, .036}},
        {"Pentagon 48", 48, sockets(7), {0, 0, 0, .022}},
        {"Hexagon 48", 48, sockets(8), {0, 0, 0, 0, .015}},
        {"Egg 40", 40, sockets(6), {.10, .035}},
        {"Shield 42", 42, sockets(7), {-.09, .035, .012}}};
    return parts;
}
void Spirograph::open(int width, int height) {
    *this = {};
    active = true;
    center = {width * .5, height * .5};
    scale = std::clamp(std::min(width, height) * .38 / 96.0, .12, 3.0);
}
void Spirograph::set_insert(int index) {
    if (index < 0 || index >= static_cast<int>(spiro_inserts().size())) {
        throw std::out_of_range("Spirograph insert");
    }
    if (!compatible(guide, index)) {
        return;
    }
    insert = index;
    selected_peg = -1;
    inserted = true;
    angle = 0;
    pegs = {};
}
void Spirograph::remove_insert() {
    inserted = false;
    selected_peg = -1;
    pegs = {};
}
void Spirograph::set_guide(int index) {
    if (index < 0 || index >= static_cast<int>(spiro_guides().size())) {
        throw std::out_of_range("Spirograph guide");
    }
    if (!compatible(index, insert)) {
        return;
    }
    scale *= static_cast<double>(spiro_guides()[guide].teeth) / spiro_guides()[index].teeth;
    guide = index;
    angle = 0;
}
double Spirograph::guide_radius() const {
    return spiro_guides().at(guide).teeth * scale;
}
double Spirograph::wheel_radius() const {
    return spiro_inserts().at(insert).teeth * scale;
}
bool Spirograph::rack() const {
    return spiro_guides().at(guide).rack;
}
bool Spirograph::compatible(int g, int w) const {
    if (outside || spiro_guides().at(g).rack) {
        return true;
    }
    // A conservative curvature bound prevents an insert from cutting across its frame.
    double lower = 1, upper = 1;
    const std::array<double, 5> gc = harmonics(spiro_guides().at(g).profile),
                                ic = harmonics(spiro_inserts().at(w).profile);
    for (int i = 0; i < 5; ++i) {
        const double factor = (i + 2) * (i + 2) - 1;
        lower -= std::abs(gc[i]) * factor;
        upper += std::abs(ic[i]) * factor;
    }
    const double minimum = lower * spiro_guides().at(g).teeth, maximum = upper * spiro_inserts().at(w).teeth;
    return minimum > maximum + 1;
}
Point Spirograph::guide_point(double normal) const {
    const Point p = rack() ? Point{normal, 0} : spiro_guides().at(guide).profile.point(normal);
    return {center.x + guide_radius() * p.x, center.y + guide_radius() * p.y};
}
Point Spirograph::close_position() const {
    const Point p = guide_point(rack() ? -1.5 : -std::numbers::pi / 4);
    return {p.x + (rack() ? 0 : 9 * scale), p.y - 12 * scale};
}
double Spirograph::wheel_rotation(double phase) const {
    const double length = rack() ? phase : spiro_guides().at(guide).profile.arc(phase);
    const double sign = outside || rack() ? -1 : 1;
    const double normal =
        spiro_inserts().at(insert).profile.normal_at_arc(sign * guide_radius() / wheel_radius() * length);
    return (rack() ? std::numbers::pi / 2 : phase + (outside ? std::numbers::pi : 0)) - normal;
}
Point Spirograph::wheel_center(double phase) const {
    const double rotation = wheel_rotation(phase);
    const double normal =
        (rack() ? std::numbers::pi / 2 : phase + (outside ? std::numbers::pi : 0)) - rotation;
    const Point p = rotated(spiro_inserts().at(insert).profile.point(normal), rotation);
    const Point contact = guide_point(phase);
    return {contact.x - wheel_radius() * p.x, contact.y - wheel_radius() * p.y};
}
double Spirograph::project(Point point, double near_phase) const {
    const bool circle = spiro_inserts()[insert].profile.circular();
    if (rack() && circle) {
        return std::clamp((point.x - center.x) / guide_radius(), -1.5, 1.5);
    }
    if (!rack() && circle && spiro_guides()[guide].profile.circular()) {
        return near_phase +
               std::remainder(std::atan2(point.y - center.y, point.x - center.x) - near_phase, tau);
    }
    // Track the nearest local center-path position. Search around the last contact
    // rather than jumping to a remote branch of a non-circular wheel's orbit.
    double best = near_phase, distance = 1e30;
    constexpr int samples = 48;
    for (int i = -samples; i <= samples; ++i) {
        const double a = rack() ? 1.5 * i / samples : near_phase + std::numbers::pi * i / samples;
        const Point c = wheel_center(a);
        const double d = std::hypot(c.x - point.x, c.y - point.y);
        if (d < distance) {
            distance = d;
            best = a;
        }
    }
    double lo = best - std::numbers::pi / samples, hi = best + std::numbers::pi / samples;
    if (rack()) {
        lo = std::max(-1.5, lo);
        hi = std::min(1.5, hi);
    }
    for (int i = 0; i < 40; ++i) {
        const double a = (2 * lo + hi) / 3, b = (lo + 2 * hi) / 3;
        const Point ca = wheel_center(a), cb = wheel_center(b);
        if (std::hypot(ca.x - point.x, ca.y - point.y) < std::hypot(cb.x - point.x, cb.y - point.y)) {
            hi = b;
        } else {
            lo = a;
        }
    }
    return (lo + hi) * .5;
}
bool Spirograph::select_peg(int index) {
    selected_peg = index >= 0 && index < hole_count() && pegs[index].seated ? index : -1;
    return selected_peg >= 0;
}
bool Spirograph::assign_brush(Brush brush) {
    if (!select_peg(selected_peg)) {
        return false;
    }
    select_brush(pegs[selected_peg].effect, brush);
    refresh_peg(selected_peg);
    return true;
}
Ink Spirograph::peg_ink(int index) const {
    const SpiroPeg& peg = pegs.at(index);
    Ink ink = peg.effect;
    ink.primary = peg.ink;
    ink.size = peg.width;
    ink.alternate.reset();
    return ink;
}
void Spirograph::refresh_peg(int index) {
    if (index < 0 || index >= hole_count() || !pegs[index].seated) {
        return;
    }
    SpiroPeg& peg = pegs[index];
    peg.preview.reset(20, 16, {0, 0, 0, 0});
    Ink ink = peg_ink(index);
    if (!peg.loaded) {
        ink.primary = {90, 115, 145, 255};
    }
    ink.size = 5;
    DynamicBrushStroke brush;
    MaterialStroke round;
    for (int i = 0; i < 12; ++i) {
        const Point a{4.0 + i, 8 + 2 * std::sin(i * .4)}, b{5.0 + i, 8 + 2 * std::sin((i + 1) * .4)};
        if (ink.brush == Brush::Round) {
            round.segment(peg.preview, a, b, ink);
        } else {
            brush.segment(peg.preview, a, b, ink, peg.glitter);
        }
    }
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
    if (rack()) {
        return 0;
    }
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
    peg.width = std::clamp(peg.width, 1, 256);
    pegs[index] = peg;
    refresh_peg(index);
    return true;
}
bool Spirograph::fill(int index, Color ink) {
    if (index < 0 || index >= hole_count() || !pegs[index].seated) {
        return false;
    }
    pegs[index].ink = ink;
    pegs[index].loaded = ink.a != 0;
    refresh_peg(index);
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
    if (rack()) {
        target = std::clamp(target, -1.5, 1.5);
    }
    const double delta = target - angle;
    // Bound pen travel to half an image pixel, including the insert's counter-rotation.
    const SpiroProfile& gp = spiro_guides()[guide].profile;
    const SpiroProfile& ip = spiro_inserts()[insert].profile;
    double guide_curvature = 1, insert_curvature = 1;
    const std::array<double, 5> gc = harmonics(gp), ic = harmonics(ip);
    for (int i = 0; i < 5; ++i) {
        const double factor = (i + 2) * (i + 2) - 1;
        guide_curvature += std::abs(gc[i]) * factor;
        insert_curvature -= std::abs(ic[i]) * factor;
    }
    const double speed = gp.circular() && ip.circular() && !rack()
                             ? 2 * (guide_radius() + (outside ? wheel_radius() : -wheel_radius()))
                             : 2.5 * (wheel_radius() + guide_radius() * guide_curvature / insert_curvature);
    const double count = std::ceil(std::abs(delta) * speed / .5);
    if (!std::isfinite(count) || count > 2000000) {
        throw std::invalid_argument("Spirograph motion too large");
    }
    const int steps = std::max(1, static_cast<int>(count));
    if (loaded() && std::abs(delta) > 1e-12) {
        for (int i = 1; i <= steps; ++i) {
            const double a = angle + delta * (i - 1) / steps, b = angle + delta * i / steps;
            const Point ac = wheel_center(a), bc = wheel_center(b);
            const double ar = wheel_rotation(a), br = wheel_rotation(b), r = wheel_radius();
            for (int h = 0; h < hole_count(); ++h) {
                const SpiroPeg& peg = pegs[h];
                if (peg.seated && peg.loaded && peg.ink.a) {
                    const Point local = spiro_inserts()[insert].holes[h];
                    const Point pa = rotated(local, ar), pb = rotated(local, br);
                    result.push_back({{ac.x + r * pa.x, ac.y + r * pa.y},
                                      {bc.x + r * pb.x, bc.y + r * pb.y},
                                      peg.ink,
                                      peg.width,
                                      h});
                }
            }
        }
    }
    angle = target;
    return result;
}
void SpirographStroke::clear() {
    scratch_ = {};
    for (int i = 0; i < 8; ++i) {
        std::unordered_map<int, Color> released;
        layers_[i].swap(released);
        material_[i].clear();
        dynamic_[i].clear();
    }
}
Rect SpirographStroke::render(Image& image, const Image& base, const Spirograph& apparatus,
                              const std::vector<SpiroTrace>& traces) {
    if (traces.empty()) {
        return {};
    }
    if (base.width != image.width || base.height != image.height ||
        base.pixels.size() != image.pixels.size()) {
        throw std::invalid_argument("Spirograph stroke base does not match the image");
    }
    if (scratch_.width != image.width || scratch_.height != image.height) {
        clear();
        scratch_.reset(image.width, image.height, {0, 0, 0, 0});
    }
    std::unordered_set<int> affected;
    int left = image.width, top = image.height, right = 0, bottom = 0;
    for (const SpiroTrace& trace : traces) {
        const double margin = trace.width + 2;
        const int x0 = std::clamp(static_cast<int>(std::floor(std::min(trace.start.x, trace.end.x) - margin)),
                                  0, image.width);
        const int y0 = std::clamp(static_cast<int>(std::floor(std::min(trace.start.y, trace.end.y) - margin)),
                                  0, image.height);
        const int x1 = std::clamp(static_cast<int>(std::ceil(std::max(trace.start.x, trace.end.x) + margin)),
                                  0, image.width);
        const int y1 = std::clamp(static_cast<int>(std::ceil(std::max(trace.start.y, trace.end.y) + margin)),
                                  0, image.height);
        if (x1 <= x0 || y1 <= y0) {
            continue;
        }
        std::unordered_map<int, Color>& layer = layers_.at(trace.peg);
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const int index = y * image.width + x;
                const std::unordered_map<int, Color>::const_iterator found = layer.find(index);
                scratch_.pixels[index] = found == layer.end() ? Color{0, 0, 0, 0} : (*found).second;
            }
        }
        const Ink ink = apparatus.peg_ink(trace.peg);
        if (ink.brush == Brush::Round) {
            material_[trace.peg].segment(scratch_, trace.start, trace.end, ink);
        } else {
            dynamic_[trace.peg].segment(scratch_, trace.start, trace.end, ink,
                                        apparatus.pegs[trace.peg].glitter);
        }
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const int index = y * image.width + x;
                const Color color = scratch_.pixels[index];
                const std::unordered_map<int, Color>::const_iterator previous = layer.find(index);
                if (color.a && (previous == layer.end() || !equal((*previous).second, color))) {
                    layer[index] = color;
                    affected.insert(index);
                }
            }
        }
        left = std::min(left, x0);
        top = std::min(top, y0);
        right = std::max(right, x1);
        bottom = std::max(bottom, y1);
    }
    for (int index : affected) {
        const int x = index % image.width, y = index / image.width;
        image.pixels[index] = base.pixels[index];
        for (const std::unordered_map<int, Color>& layer : layers_) {
            const std::unordered_map<int, Color>::const_iterator found = layer.find(index);
            if (found != layer.end()) {
                image.blend(x, y, (*found).second);
            }
        }
    }
    return {left, top, std::max(0, right - left), std::max(0, bottom - top)};
}
} // namespace paint
