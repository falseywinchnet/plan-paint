#include "canvas_backing.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace paint {
namespace {
constexpr int texture_side = 512;
std::uint32_t next_noise(std::uint32_t& state) {
    state = state * 1664525U + 1013904223U;
    return state;
}
double unit_noise(std::uint32_t& state) {
    return static_cast<double>(next_noise(state) >> 8) / 16777216.0;
}
std::size_t wrapped_index(int x, int y) {
    return static_cast<std::size_t>((y + texture_side) % texture_side) * texture_side +
           (x + texture_side) % texture_side;
}
double lattice(int x, int y, int cells, std::uint32_t seed) {
    std::uint32_t hash = static_cast<std::uint32_t>((x + cells) % cells) * 0x9e3779b9U ^
                         static_cast<std::uint32_t>((y + cells) % cells) * 0x85ebca6bU ^ seed;
    hash ^= hash >> 16;
    hash *= 0x7feb352dU;
    hash ^= hash >> 15;
    return static_cast<double>(hash & 65535U) / 32767.5 - 1;
}
double pile_noise(int x, int y, int spacing, std::uint32_t seed) {
    const int cell_x = x / spacing, cell_y = y / spacing, cells = texture_side / spacing;
    double tx = static_cast<double>(x % spacing) / spacing;
    double ty = static_cast<double>(y % spacing) / spacing;
    tx = tx * tx * (3 - 2 * tx);
    ty = ty * ty * (3 - 2 * ty);
    const double top =
        std::lerp(lattice(cell_x, cell_y, cells, seed), lattice(cell_x + 1, cell_y, cells, seed), tx);
    const double bottom =
        std::lerp(lattice(cell_x, cell_y + 1, cells, seed), lattice(cell_x + 1, cell_y + 1, cells, seed), tx);
    return std::lerp(top, bottom, ty);
}
std::uint8_t channel(double value) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}
} // namespace
const char* canvas_backing_name(CanvasBacking backing) {
    const char* names[] = {"Original grey-blue felt", "Moss green felt",    "Warm brown felt",    "Tan felt",
                           "Matte painted slate",     "Matte painted clay", "Matte painted ivory"};
    const int index = static_cast<int>(backing);
    return index >= 0 && index < canvas_backing_count ? names[index] : names[0];
}
double canvas_backing_tile_size(CanvasBacking backing) {
    return backing == CanvasBacking::PaleFelt ? 128 : 256;
}
Image canvas_backing_texture(CanvasBacking backing) {
    const int index = static_cast<int>(backing);
    if (index < 0 || index >= canvas_backing_count) {
        throw std::invalid_argument("Unknown canvas backing.");
    }
    Image image;
    std::uint32_t random = 0x5241494eU;
    if (backing == CanvasBacking::PaleFelt) {
        // Preserve the original screen-space tile byte for byte.
        image.reset(128, 128, {});
        for (int y = 0; y < 128; ++y) {
            for (int x = 0; x < 128; ++x) {
                const int fiber =
                    static_cast<int>((next_noise(random) >> 24) % 7) - 3 + ((x + 3 * y) % 17 == 0 ? 1 : 0);
                image.set(x, y, {channel(208 + fiber), channel(218 + fiber), channel(226 + fiber), 255});
            }
        }
        return image;
    }
    const bool felt = index <= static_cast<int>(CanvasBacking::TanFelt);
    const Color bases[] = {{208, 218, 226, 255}, {83, 111, 86, 255},   {119, 89, 67, 255},
                           {185, 159, 121, 255}, {106, 121, 137, 255}, {161, 137, 119, 255},
                           {213, 207, 192, 255}};
    const Color base = bases[index];
    std::vector<double> height(texture_side * texture_side), dye(height.size());
    for (int y = 0; y < texture_side; ++y) {
        for (int x = 0; x < texture_side; ++x) {
            const std::size_t pixel = static_cast<std::size_t>(y) * texture_side + x;
            // Periodic fields avoid a seam. Felt has uneven pile; matte paint
            // has a much shallower fine stipple, without fiber directionality.
            height[pixel] = (felt ? 0.85 : 0.16) * pile_noise(x, y, 32, 0xa429U) +
                            (felt ? 0.32 : 0.22) * pile_noise(x, y, 4, 0x815fU) +
                            (unit_noise(random) - 0.5) * (felt ? 0.2 : 0.08);
            dye[pixel] = pile_noise(x, y, 16, 0x39caU) * (felt ? 3.0 : 0.65);
        }
    }
    if (felt) {
        // Short, curved fibers with toroidal coverage, rather than a woven grid
        // or independent white-noise pixels. Two texture samples per UI pixel.
        for (int fiber = 0; fiber < 12500; ++fiber) {
            const double x = unit_noise(random) * texture_side, y = unit_noise(random) * texture_side;
            const double angle = unit_noise(random) * 6.283185307179586;
            const double length = 3 + unit_noise(random) * 12;
            const double bend = (unit_noise(random) - 0.5) * 2.5;
            const double amplitude = 0.28 + unit_noise(random) * 0.55;
            const double tone = (unit_noise(random) - 0.5) * 5;
            const int samples = static_cast<int>(length * 2);
            for (int step = 0; step <= samples; ++step) {
                const double t = static_cast<double>(step) / samples;
                const double across = std::sin(t * 3.141592653589793) * bend;
                const double fx = x + std::cos(angle) * length * t - std::sin(angle) * across;
                const double fy = y + std::sin(angle) * length * t + std::cos(angle) * across;
                const int ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy));
                for (int row = -1; row <= 1; ++row) {
                    for (int col = -1; col <= 1; ++col) {
                        const double dx = ix + col + 0.5 - fx, dy = iy + row + 0.5 - fy;
                        const double coverage =
                            std::max(0.0, 1 - (dx * dx + dy * dy) / 1.4) * std::sin(t * 3.141592653589793);
                        const std::size_t pixel = wrapped_index(ix + col, iy + row);
                        height[pixel] += coverage * amplitude * 0.5;
                        dye[pixel] += coverage * tone * 0.25;
                    }
                }
            }
        }
    }
    image.reset(texture_side, texture_side, {});
    for (int y = 0; y < texture_side; ++y) {
        for (int x = 0; x < texture_side; ++x) {
            const std::size_t pixel = wrapped_index(x, y);
            // The same upper-left light as the controls: a pile slope facing
            // the source is lighter, while valleys carry a little occlusion.
            const double slope = height[wrapped_index(x + 1, y)] - height[wrapped_index(x - 1, y)] +
                                 height[wrapped_index(x, y + 1)] - height[wrapped_index(x, y - 1)];
            const double shade = slope * (felt ? 12 : 8) + height[pixel] * (felt ? 6 : 2) + dye[pixel];
            image.set(x, y, {channel(base.r + shade), channel(base.g + shade), channel(base.b + shade), 255});
        }
    }
    return image;
}
} // namespace paint
