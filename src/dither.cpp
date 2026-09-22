#include "dither.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
namespace paint {
namespace {
struct V {
    double x = 0, y = 0, z = 0;
};
V add(V a, V b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
V sub(V a, V b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
V mul(V a, double s) {
    return {a.x * s, a.y * s, a.z * s};
}
double dot(V a, V b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
double decode(std::uint8_t c) {
    double s = c / 255.0;
    return s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
}
std::uint8_t encode(double c) {
    c = std::clamp(c, 0.0, 1.0);
    return static_cast<std::uint8_t>(
        std::lround(255 * (c <= .0031308 ? 12.92 * c : 1.055 * std::pow(c, 1 / 2.4) - .055)));
}
V linear(Color c) {
    return {decode(c.r), decode(c.g), decode(c.b)};
}
Color encoded(V v, std::uint8_t alpha = 255) {
    return {encode(v.x), encode(v.y), encode(v.z), alpha};
}
V lab(Color c) {
    Lab l = to_oklab(c);
    return {l.l, l.a, l.b};
}
std::uint32_t hash(std::uint32_t v) {
    v ^= v >> 16;
    v *= 0x7feb352dU;
    v ^= v >> 15;
    v *= 0x846ca68bU;
    return v ^ (v >> 16);
}
double random(std::uint32_t v) {
    return (hash(v) + .5) / 4294967296.0;
}
bool allowed(const Image& image, std::span<const std::uint8_t> mask, int x, int y) {
    return image.contains(x, y) && (mask.empty() || mask[static_cast<std::size_t>(y) * image.width + x]);
}
using Bin = std::tuple<int, int, int>;
Bin bin(V v, double step) {
    return {static_cast<int>(std::floor(v.x / step)), static_cast<int>(std::floor(v.y / step)),
            static_cast<int>(std::floor(v.z / step))};
}
struct Sample {
    Color color;
    V perceptual;
    double entropy = 0, weight = 1;
    Bin family;
};
struct Family {
    double count = 0, entropy = 0;
};
// A bounded, deterministic sample; selection holes and hidden RGB never enter it.
std::vector<Sample> samples(const Image& image, std::span<const std::uint8_t> mask,
                            const std::atomic<bool>* cancel) {
    std::size_t active = 0;
    for (std::size_t i = 0; i < image.pixels.size(); ++i) {
        if ((i & 4095) == 0 && cancel && (*cancel).load()) {
            return {};
        }
        if ((mask.empty() || mask[i]) && image.pixels[i].a) {
            ++active;
        }
    }
    const std::size_t stride = std::max<std::size_t>(1, (active + 4095) / 4096);
    std::vector<Sample> out;
    std::size_t ordinal = 0;
    for (std::size_t i = 0; i < image.pixels.size(); ++i) {
        if ((i & 4095) == 0 && cancel && (*cancel).load()) {
            return {};
        }
        Color c = image.pixels[i];
        if ((!mask.empty() && !mask[i]) || !c.a) {
            continue;
        }
        if (ordinal++ % stride) {
            continue;
        }
        const int x = static_cast<int>(i % image.width), y = static_cast<int>(i / image.width);
        Sample sample;
        sample.color = c;
        sample.perceptual = lab(c);
        sample.family = bin(sample.perceptual, .1);
        std::map<Bin, int> symbols;
        V mean{}, squared{};
        int n = 0;
        for (int yy = std::max(0, y - 2); yy <= std::min(image.height - 1, y + 2); ++yy) {
            for (int xx = std::max(0, x - 2); xx <= std::min(image.width - 1, x + 2); ++xx) {
                if (!allowed(image, mask, xx, yy) || !image.get(xx, yy).a) {
                    continue;
                }
                V p = lab(image.get(xx, yy));
                ++symbols[bin(p, .025)];
                mean = add(mean, p);
                squared = add(squared, {p.x * p.x, p.y * p.y, p.z * p.z});
                ++n;
            }
        }
        if (n > 1) {
            double entropy = 0;
            for (const std::pair<const Bin, int>& symbol : symbols) {
                double p = static_cast<double>(symbol.second) / n;
                entropy -= p * std::log2(p);
            }
            mean = mul(mean, 1.0 / n);
            double variance = std::max(0.0, (squared.x + squared.y + squared.z) / n - dot(mean, mean)) / 3;
            sample.entropy = entropy / std::log2(n) * variance / (variance + .015 * .015);
        }
        out.push_back(sample);
    }
    std::map<Bin, Family> families;
    for (const Sample& s : out) {
        Family& f = families[s.family];
        f.count += 1;
        f.entropy += s.entropy;
    }
    double rarity_sum = 0, importance_sum = 0;
    for (const Sample& s : out) {
        const Family& f = families[s.family];
        rarity_sum += std::pow(f.count, -.35);
        importance_sum += std::pow(f.entropy / (f.count + 4), 2) * std::pow(f.count, -.75);
    }
    for (Sample& s : out) {
        const Family& f = families[s.family];
        double rare = std::pow(f.count, -.35) * out.size() / std::max(rarity_sum, 1e-20);
        double imp = std::pow(f.entropy / (f.count + 4), 2) * std::pow(f.count, -.75) * out.size() /
                     std::max(importance_sum, 1e-20);
        s.weight = (.4 + .3 * rare + .3 * imp) * s.color.a / 255.0;
    }
    return out;
}
bool palette_less(Color a, Color b);
std::vector<Color> make_palette(const std::vector<Sample>& source, int count,
                                const std::atomic<bool>* cancel) {
    if (source.empty()) {
        return {};
    }
    // Actual source colors seed a bounded candidate set; weighted farthest
    // sampling keeps uncommon families available to the subsequent t2 score.
    std::vector<std::size_t> candidates;
    std::vector<double> nearest(source.size(), std::numeric_limits<double>::infinity());
    std::size_t chosen = 0;
    for (std::size_t i = 1; i < source.size(); ++i) {
        if (source[i].weight > source[chosen].weight) {
            chosen = i;
        }
    }
    for (int step = 0; step < 192; ++step) {
        if (cancel && (*cancel).load()) {
            return {};
        }
        candidates.push_back(chosen);
        double best = -1;
        std::size_t next = 0;
        for (std::size_t i = 0; i < source.size(); ++i) {
            V delta = sub(source[i].perceptual, source[chosen].perceptual);
            nearest[i] = std::min(nearest[i], dot(delta, delta));
            double score = nearest[i] * std::sqrt(source[i].weight);
            if (score > best) {
                best = score;
                next = i;
            }
        }
        if (best < 1e-12) {
            break;
        }
        chosen = next;
    }
    std::vector<double> costs(source.size() * candidates.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        for (std::size_t j = 0; j < candidates.size(); ++j) {
            V delta = sub(source[i].perceptual, source[candidates[j]].perceptual);
            costs[i * candidates.size() + j] =
                1 - std::pow(1 + (std::pow(2.0, 2.0 / 3) - 1) * dot(delta, delta) / (.08 * .08), -1.5);
        }
    }
    std::fill(nearest.begin(), nearest.end(), std::numeric_limits<double>::infinity());
    std::vector<bool> used(candidates.size(), false);
    std::vector<Color> palette;
    for (int step = 0; step < std::min(count, static_cast<int>(candidates.size())); ++step) {
        if (cancel && (*cancel).load()) {
            return {};
        }
        double best = std::numeric_limits<double>::infinity();
        std::size_t next = 0;
        for (std::size_t j = 0; j < candidates.size(); ++j) {
            if (used[j]) {
                continue;
            }
            double score = 0;
            for (std::size_t i = 0; i < source.size(); ++i) {
                score += source[i].weight * std::min(nearest[i], costs[i * candidates.size() + j]);
            }
            if (score < best) {
                best = score;
                next = j;
            }
        }
        used[next] = true;
        Color c = source[candidates[next]].color;
        c.a = 255;
        palette.push_back(c);
        for (std::size_t i = 0; i < source.size(); ++i) {
            nearest[i] = std::min(nearest[i], costs[i * candidates.size() + next]);
        }
    }
    // Stable perceptual order couples nearby source colors to the same intervals.
    std::sort(palette.begin(), palette.end(), palette_less);
    return palette;
}
struct Mix {
    std::array<int, 4> ids{};
    std::array<double, 4> weights{};
    int size = 0;
    V point;
};
bool solve(std::array<std::array<double, 4>, 3>& m, int n, double* answer) {
    for (int col = 0; col < n; ++col) {
        int pivot = col;
        for (int row = col + 1; row < n; ++row) {
            if (std::abs(m[row][col]) > std::abs(m[pivot][col])) {
                pivot = row;
            }
        }
        if (std::abs(m[pivot][col]) < 1e-14) {
            return false;
        }
        for (int j = col; j <= n; ++j) {
            std::swap(m[col][j], m[pivot][j]);
        }
        double scale = m[col][col];
        for (int j = col; j <= n; ++j) {
            m[col][j] /= scale;
        }
        for (int row = 0; row < n; ++row) {
            if (row != col) {
                scale = m[row][col];
                for (int j = col; j <= n; ++j) {
                    m[row][j] -= scale * m[col][j];
                }
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        answer[i] = m[i][n];
    }
    return true;
}
Mix project(V source, const std::vector<V>& palette) {
    Mix mix;
    mix.size = 1;
    mix.weights[0] = 1;
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < palette.size(); ++i) {
        V d = sub(palette[i], source);
        if (dot(d, d) < best) {
            best = dot(d, d);
            mix.ids[0] = static_cast<int>(i);
        }
    }
    mix.point = palette[mix.ids[0]];
    for (int iteration = 0; iteration < 32; ++iteration) {
        V residual = sub(mix.point, source);
        int entering = -1;
        double gain = -1e-12;
        for (std::size_t i = 0; i < palette.size(); ++i) {
            double value = dot(residual, sub(palette[i], mix.point));
            if (value < gain) {
                gain = value;
                entering = static_cast<int>(i);
            }
        }
        if (entering < 0) {
            break;
        }
        std::array<int, 5> ids{};
        for (int i = 0; i < mix.size; ++i) {
            ids[i] = mix.ids[i];
        }
        ids[mix.size] = entering;
        const int count = mix.size + 1;
        Mix next = mix;
        double error = dot(residual, residual);
        // Closest point on the small active hull. Nonnegative affine weights
        // select its face/edge/interior without an external numerical library.
        for (int bits = 1; bits < (1 << count); ++bits) {
            int n = std::popcount(static_cast<unsigned int>(bits));
            if (n > 4) {
                continue;
            }
            Mix trial;
            trial.size = n;
            int index = 0;
            for (int j = 0; j < count; ++j) {
                if (bits & (1 << j)) {
                    trial.ids[index++] = ids[j];
                }
            }
            V origin = palette[trial.ids[0]];
            std::array<V, 3> edges{};
            std::array<std::array<double, 4>, 3> matrix{};
            std::array<double, 3> solution{};
            for (int j = 0; j < n - 1; ++j) {
                edges[j] = sub(palette[trial.ids[j + 1]], origin);
            }
            for (int j = 0; j < n - 1; ++j) {
                for (int k = 0; k < n - 1; ++k) {
                    matrix[j][k] = dot(edges[j], edges[k]);
                }
                matrix[j][n - 1] = dot(edges[j], sub(source, origin));
            }
            if (n > 1 && !solve(matrix, n - 1, solution.data())) {
                continue;
            }
            trial.weights[0] = 1;
            bool valid = true;
            for (int j = 1; j < n; ++j) {
                trial.weights[j] = solution[j - 1];
                trial.weights[0] -= solution[j - 1];
            }
            for (int j = 0; j < n; ++j) {
                if (trial.weights[j] < -1e-10) {
                    valid = false;
                }
            }
            if (!valid) {
                continue;
            }
            trial.point = {};
            double sum = 0;
            for (int j = 0; j < n; ++j) {
                trial.weights[j] = std::max(0.0, trial.weights[j]);
                sum += trial.weights[j];
            }
            for (int j = 0; j < n; ++j) {
                trial.weights[j] /= sum;
                trial.point = add(trial.point, mul(palette[trial.ids[j]], trial.weights[j]));
            }
            V d = sub(source, trial.point);
            double candidate = dot(d, d);
            if (candidate < error - 1e-15) {
                error = candidate;
                next = trial;
            }
        }
        if (dot(sub(next.point, mix.point), sub(next.point, mix.point)) < 1e-20) {
            break;
        }
        mix = next;
    }
    for (int i = 0; i < mix.size; ++i) {
        for (int j = i + 1; j < mix.size; ++j) {
            if (mix.ids[j] < mix.ids[i]) {
                std::swap(mix.ids[i], mix.ids[j]);
                std::swap(mix.weights[i], mix.weights[j]);
            }
        }
    }
    return mix;
}
bool palette_less(Color a, Color b) {
    V x = lab(a), y = lab(b);
    return std::tie(x.x, x.y, x.z) < std::tie(y.x, y.y, y.z);
}
} // namespace
std::uint16_t dither_rank(int x, int y, DitherPattern pattern) {
    unsigned int xx = static_cast<unsigned int>(x), yy = static_cast<unsigned int>(y), rank = 0;
    for (unsigned int j = 0; j < 6; ++j) {
        unsigned int xb = (xx >> j) & 1U, yb = (yy >> j) & 1U;
        if (pattern == DitherPattern::Scrambled) {
            unsigned int cell = hash((xx >> (j + 1)) ^ std::rotl(yy >> (j + 1), 16) ^ (j * 0x9e3779b9U));
            rank = (rank << 1) | (xb ^ yb ^ (cell & 1U));
            rank = (rank << 1) | (yb ^ ((cell >> 1) & 1U));
        } else {
            unsigned int lower = (1U << j) - 1U;
            unsigned int twist = std::popcount((xx & lower) ^ (yy & lower)) & 1U;
            rank = (rank << 1) | (xb ^ yb);
            rank = (rank << 1) | (yb ^ twist);
        }
    }
    if (pattern == DitherPattern::Drift) {
        unsigned int word = (xx >> 6) ^ std::rotl(yy >> 6, 16);
        for (unsigned int j = 0; j < 8; ++j) {
            rank ^= (std::popcount(word & std::rotl(0x9e3779b9U, static_cast<int>(j * 3))) & 1U) << j;
        }
    }
    return static_cast<std::uint16_t>(rank);
}
Image dithered(const Image& source, std::span<const std::uint8_t> mask, DitherOptions options,
               std::vector<Color>* offered, const std::atomic<bool>* cancel) {
    if (!mask.empty() && mask.size() != source.pixels.size()) {
        throw std::invalid_argument("Dither mask dimensions do not match the image.");
    }
    if (options.colors < 2 || options.colors > 32) {
        throw std::invalid_argument("Choose between 2 and 32 colors.");
    }
    std::vector<Color> palette = make_palette(samples(source, mask, cancel), options.colors, cancel);
    if (cancel && (*cancel).load()) {
        return {};
    }
    if (offered) {
        *offered = palette;
    }
    Image output = source;
    if (palette.empty()) {
        return output;
    }
    std::vector<V> colors, perceptual;
    for (Color c : palette) {
        colors.push_back(linear(c));
        perceptual.push_back(lab(c));
    }
    std::unordered_map<std::uint32_t, Mix> cache;
    std::vector<V> current(static_cast<std::size_t>(source.width) + 2), next(current.size());
    for (int y = 0; y < source.height; ++y) {
        if (cancel && (*cancel).load()) {
            return {};
        }
        const bool reverse = (y & 1) != 0;
        const int direction = reverse ? -1 : 1;
        for (int column = 0; column < source.width; ++column) {
            int x = reverse ? source.width - 1 - column : column;
            std::size_t i = static_cast<std::size_t>(y) * source.width + x;
            Color original = source.pixels[i];
            if ((!mask.empty() && !mask[i]) || !original.a) {
                continue;
            }
            int chosen = 0;
            V source_linear = linear(original);
            if (options.pattern == DitherPattern::Crosswind || options.pattern == DitherPattern::Posterize) {
                V target = options.pattern == DitherPattern::Crosswind ? add(source_linear, current[x + 1])
                                                                       : lab(original);
                double distance = std::numeric_limits<double>::infinity();
                for (std::size_t j = 0; j < palette.size(); ++j) {
                    V delta =
                        sub(target, options.pattern == DitherPattern::Crosswind ? colors[j] : perceptual[j]);
                    if (dot(delta, delta) < distance) {
                        distance = dot(delta, delta);
                        chosen = static_cast<int>(j);
                    }
                }
                if (options.pattern == DitherPattern::Crosswind) {
                    const std::array<int, 4> dx{direction, -direction, 0, direction}, dy{0, 1, 1, 1};
                    const std::array<double, 4> base{7, 3, 5, 1};
                    std::array<double, 4> weights{};
                    double sum = 0;
                    for (int j = 0; j < 4; ++j) {
                        int xx = x + dx[j], yy = y + dy[j];
                        if (!allowed(source, mask, xx, yy) || !source.get(xx, yy).a) {
                            continue;
                        }
                        V delta = sub(source_linear, linear(source.get(xx, yy)));
                        weights[j] = base[j] *
                                     (.2 + 1.6 * random(static_cast<std::uint32_t>(i) * 31 + j * 911)) *
                                     (.1 + .9 / (1 + dot(delta, delta) / .012));
                        sum += weights[j];
                    }
                    V error = sub(target, colors[chosen]);
                    if (sum > 0) {
                        for (int j = 0; j < 4; ++j) {
                            if (weights[j] > 0) {
                                std::vector<V>& row = dy[j] ? next : current;
                                row[x + dx[j] + 1] = add(row[x + dx[j] + 1], mul(error, weights[j] / sum));
                            }
                        }
                    }
                }
            } else {
                std::uint32_t key = static_cast<std::uint32_t>(original.r) |
                                    (static_cast<std::uint32_t>(original.g) << 8) |
                                    (static_cast<std::uint32_t>(original.b) << 16);
                std::unordered_map<std::uint32_t, Mix>::const_iterator found = cache.find(key);
                Mix mix;
                if (found != cache.end()) {
                    mix = (*found).second;
                } else {
                    mix = project(source_linear, colors);
                    // Bound working memory on photographic / very large selections.
                    if (cache.size() < 32768) {
                        cache.emplace(key, mix);
                    }
                }
                double threshold = (dither_rank(x, y, options.pattern) + .5) / 4096.0, cumulative = 0;
                chosen = mix.ids[mix.size - 1];
                for (int j = 0; j < mix.size; ++j) {
                    cumulative += mix.weights[j];
                    if (threshold < cumulative) {
                        chosen = mix.ids[j];
                        break;
                    }
                }
            }
            Color c = palette[chosen];
            c.a = original.a;
            output.pixels[i] = c;
        }
        current.swap(next);
        std::fill(next.begin(), next.end(), V{});
    }
    return output;
}
void DitherBrushStroke::clear() {
    pending_ = 0;
    serial_ = 0;
    started_ = false;
}
void DitherBrushStroke::dab(Image& image, Point center, double diameter, DitherBrushMode mode,
                            std::uint32_t seed, bool wrap, std::span<const std::uint8_t> mask) {
    const double radius = std::max(.5, diameter * .5);
    const int left = static_cast<int>(std::floor(center.x - radius)),
              right = static_cast<int>(std::ceil(center.x + radius));
    const int top = static_cast<int>(std::floor(center.y - radius)),
              bottom = static_cast<int>(std::ceil(center.y + radius));
    std::vector<std::pair<std::size_t, Color>> changes;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            double dx = x - center.x, dy = y - center.y, r2 = (dx * dx + dy * dy) / (radius * radius);
            if (r2 >= 1) {
                continue;
            }
            int xx = wrap ? (x % image.width + image.width) % image.width : x,
                yy = wrap ? (y % image.height + image.height) % image.height : y;
            if (!allowed(image, mask, xx, yy)) {
                continue;
            }
            std::uint32_t noise = hash(static_cast<std::uint32_t>(x) * 374761393U ^
                                       static_cast<std::uint32_t>(y) * 668265263U ^ seed ^ serial_ * 104729U);
            if (random(noise) > .28 * (1 - r2) * (1 - r2)) {
                continue;
            }
            Color original = image.get(xx, yy);
            if (!original.a) {
                continue;
            }
            Color color;
            if (mode == DitherBrushMode::Neighborhood) {
                V mean{};
                double mass = 0;
                for (int j = -2; j <= 2; ++j) {
                    for (int k = -2; k <= 2; ++k) {
                        int sx = xx + k, sy = yy + j;
                        if (wrap) {
                            sx = (sx % image.width + image.width) % image.width;
                            sy = (sy % image.height + image.height) % image.height;
                        }
                        if (!allowed(image, mask, sx, sy)) {
                            continue;
                        }
                        Color neighbor = image.get(sx, sy);
                        double a = neighbor.a / 255.0;
                        mean = add(mean, mul(linear(neighbor), a));
                        mass += a;
                    }
                }
                if (mass <= 0) {
                    continue;
                }
                color = encoded(mul(add(linear(original), mul(mean, 1 / mass)), .5), original.a);
            } else {
                Lab value = to_oklab(original);
                value.l += .025 * (2 * random(noise ^ 0xa511e9b3U) - 1);
                double saturation = 1 + .1 * (2 * random(noise ^ 0x63d83595U) - 1);
                value.a *= saturation;
                value.b *= saturation;
                color = from_oklab(value);
                color.a = original.a;
            }
            changes.emplace_back(static_cast<std::size_t>(yy) * image.width + xx, color);
        }
    }
    // All neighborhood reads precede writes, so one dab has no scan-direction feedback.
    for (const std::pair<std::size_t, Color>& change : changes) {
        image.pixels[change.first] = change.second;
    }
    ++serial_;
}
void DitherBrushStroke::segment(Image& image, Point start, Point end, double diameter, DitherBrushMode mode,
                                std::uint32_t seed, bool wrap, std::span<const std::uint8_t> mask) {
    if (image.width <= 0 || image.height <= 0) {
        return;
    }
    if (!mask.empty() && mask.size() != image.pixels.size()) {
        throw std::invalid_argument("Brush mask dimensions do not match the image.");
    }
    diameter = std::clamp(diameter, 1.0, 1024.0);
    double spacing = std::max(1.0, diameter * .12), distance = std::hypot(end.x - start.x, end.y - start.y);
    if (!started_) {
        dab(image, start, diameter, mode, seed, wrap, mask);
        started_ = true;
    }
    if (distance <= 0) {
        return;
    }
    double position = spacing - pending_;
    for (; position <= distance; position += spacing) {
        double t = position / distance;
        dab(image, {start.x + t * (end.x - start.x), start.y + t * (end.y - start.y)}, diameter, mode, seed,
            wrap, mask);
    }
    pending_ = std::fmod(pending_ + distance, spacing);
}
} // namespace paint
