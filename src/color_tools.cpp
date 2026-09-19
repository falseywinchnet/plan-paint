#include "color_tools.hpp"
#include "../vendor/ok_color/ok_color.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace paint {
namespace {
std::uint8_t channel(double value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255));
}
double distance_squared(Lab a, Lab b) {
    return (a.l - b.l) * (a.l - b.l) + (a.a - b.a) * (a.a - b.a) + (a.b - b.b) * (a.b - b.b);
}
std::size_t nearest_center(Lab lab, const std::array<Lab, 96>& centers) {
    std::size_t best = 0;
    double distance = std::numeric_limits<double>::max();
    for (std::size_t index = 0; index < centers.size(); ++index) {
        const double candidate = distance_squared(lab, centers[index]);
        if (candidate < distance) {
            best = index;
            distance = candidate;
        }
    }
    return best;
}
} // namespace
Color picker_color(PickerSpace space, ColorCoordinates coordinates) {
    const double hue = coordinates.hue - std::floor(coordinates.hue);
    const double saturation = std::clamp(coordinates.saturation, 0.0, 1.5);
    const double level = std::clamp(coordinates.level, 0.0, 1.0);
    if (space == PickerSpace::OKHSL) {
        const ok_color::RGB rgb = ok_color::okhsl_to_srgb({hue, saturation, level});
        return {channel(rgb.r), channel(rgb.g), channel(rgb.b), 255};
    }
    const double h = hue * 6, c = level * saturation;
    const double x = c * (1 - std::abs(std::fmod(h, 2) - 1)), m = level - c;
    const int sector = static_cast<int>(h);
    const std::array<double, 6> red{c, x, 0, 0, x, c}, green{x, c, c, x, 0, 0}, blue{0, 0, x, c, c, x};
    return {channel(red[sector] + m), channel(green[sector] + m), channel(blue[sector] + m), 255};
}
ColorCoordinates picker_coordinates(PickerSpace space, Color color) {
    const double r = color.r / 255.0, g = color.g / 255.0, b = color.b / 255.0;
    if (space == PickerSpace::OKHSL) {
        if (color.r == color.g && color.g == color.b) {
            return {0, 0, ok_color::toe(to_oklab(color).l)};
        }
        const ok_color::HSL hsl = ok_color::srgb_to_okhsl({r, g, b});
        return {hsl.h, hsl.s, std::clamp(hsl.l, 0.0, 1.0)};
    }
    const double maximum = std::max({r, g, b}), minimum = std::min({r, g, b}), delta = maximum - minimum;
    double hue = 0;
    if (delta > 0) {
        hue = maximum == r   ? std::fmod((g - b) / delta + 6, 6) / 6
              : maximum == g ? ((b - r) / delta + 2) / 6
                             : ((r - g) / delta + 4) / 6;
    }
    return {hue, maximum == 0 ? 0 : delta / maximum, maximum};
}
Color sample_color(const Image& image, Point point, SampleMode mode, int radius) {
    const int x = static_cast<int>(std::floor(point.x)), y = static_cast<int>(std::floor(point.y));
    if (mode == SampleMode::Exact) {
        return image.get(x, y);
    }
    radius = std::clamp(radius, 1, 16);
    std::vector<Lab> samples;
    std::vector<double> weights;
    double alpha = 0;
    int count = 0;
    for (int row = std::max(0, y - radius); row <= std::min(image.height - 1, y + radius); ++row) {
        for (int column = std::max(0, x - radius); column <= std::min(image.width - 1, x + radius);
             ++column) {
            const Color color = image.get(column, row);
            alpha += color.a;
            ++count;
            if (color.a) {
                samples.push_back(to_oklab(color));
                weights.push_back(color.a / 255.0);
            }
        }
    }
    if (samples.empty()) {
        return {0, 0, 0, 0};
    }
    std::vector<double> light, a, b;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        light.push_back(samples[index].l);
        a.push_back(samples[index].a);
        b.push_back(samples[index].b);
    }
    std::sort(light.begin(), light.end());
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    const std::size_t middle = samples.size() / 2;
    const Lab median{light[middle], a[middle], b[middle]};
    std::vector<double> deviations;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        deviations.push_back(std::sqrt(distance_squared(samples[index], median)));
    }
    std::sort(deviations.begin(), deviations.end());
    const double cutoff = std::max(0.015, deviations[middle] * 3);
    Lab sum;
    double total = 0;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        if (mode == SampleMode::Representative &&
            distance_squared(samples[index], median) > cutoff * cutoff) {
            continue;
        }
        const double weight = weights[index];
        sum.l += samples[index].l * weight;
        sum.a += samples[index].a * weight;
        sum.b += samples[index].b * weight;
        total += weight;
    }
    Color result = from_oklab(total > 0 ? Lab{sum.l / total, sum.a / total, sum.b / total} : median);
    result.a = static_cast<std::uint8_t>(std::lround(alpha / count));
    return result;
}
ColorMosaic::ColorMosaic(PickerSpace space) {
    for (std::size_t index = 0; index < centers_.size(); ++index) {
        const Color color =
            picker_color(space, {static_cast<double>(index % 12) / 12, (index / 12) % 2 ? 0.9 : 0.25,
                                 0.12 + static_cast<double>(index / 24) * 0.25});
        centers_[index] = to_oklab(color);
    }
    std::vector<Lab> samples;
    for (int h = 0; h < 36; ++h) {
        for (int s = 0; s < 7; ++s) {
            for (int l = 0; l < 11; ++l) {
                samples.push_back(to_oklab(picker_color(space, {h / 36.0, s / 6.0, l / 10.0})));
            }
        }
    }
    for (int iteration = 0; iteration < 8; ++iteration) {
        std::array<Lab, 96> sums{};
        std::array<int, 96> counts{};
        for (std::size_t index = 0; index < samples.size(); ++index) {
            const Lab lab = samples[index];
            const std::size_t cell = nearest_center(lab, centers_);
            sums[cell].l += lab.l;
            sums[cell].a += lab.a;
            sums[cell].b += lab.b;
            ++counts[cell];
        }
        for (std::size_t index = 0; index < centers_.size(); ++index) {
            if (counts[index]) {
                centers_[index] = {sums[index].l / counts[index], sums[index].a / counts[index],
                                   sums[index].b / counts[index]};
            }
        }
    }
    for (std::size_t index = 0; index < centers_.size(); ++index) {
        colors_[index] = from_oklab(centers_[index]);
        centers_[index] = to_oklab(colors_[index]);
    }
    // Keep the two exact endpoints useful for line art and transparent assets.
    colors_[0] = {0, 0, 0, 255};
    colors_[95] = {255, 255, 255, 255};
    centers_[0] = to_oklab(colors_[0]);
    centers_[95] = to_oklab(colors_[95]);
}
Color ColorMosaic::nearest(Color color) const {
    Color result = colors_[nearest_center(to_oklab(color), centers_)];
    result.a = color.a;
    return result;
}
const std::array<Color, 96>& ColorMosaic::colors() const {
    return colors_;
}
} // namespace paint
