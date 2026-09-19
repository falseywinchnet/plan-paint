#include "guide.hpp"
#include "paint_tools.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
namespace paint {
namespace {
double distance_squared(Lab a, Lab b) {
    return (a.l - b.l) * (a.l - b.l) + (a.a - b.a) * (a.a - b.a) + (a.b - b.b) * (a.b - b.b);
}
double edge_distance(Point point, Point start, Point end) {
    const double dx = end.x - start.x, dy = end.y - start.y, length = dx * dx + dy * dy;
    const double t =
        length > 0 ? std::clamp(((point.x - start.x) * dx + (point.y - start.y) * dy) / length, 0.0, 1.0) : 0;
    return std::hypot(point.x - start.x - t * dx, point.y - start.y - t * dy);
}
Lab median_lab(std::vector<Lab> samples) {
    std::vector<double> l, a, b;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        l.push_back(samples[index].l);
        a.push_back(samples[index].a);
        b.push_back(samples[index].b);
    }
    if (l.empty()) {
        return {};
    }
    std::sort(l.begin(), l.end());
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    const std::size_t middle = l.size() / 2;
    return {l[middle], a[middle], b[middle]};
}
} // namespace
std::vector<Point> mask_outline(const std::vector<std::uint8_t>& mask, int width, int height) {
    // Trace directed boundary edges with foreground on the right. Keep the
    // largest loop as the editable outer polygon; the mask retains inner holes.
    std::unordered_multimap<int, int> edges;
    const int stride = width + 1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * width + x;
            if (!mask[index]) {
                continue;
            }
            const int tl = y * stride + x, tr = tl + 1, bl = tl + stride, br = bl + 1;
            if (y == 0 || !mask[index - width]) {
                edges.emplace(tl, tr);
            }
            if (x + 1 == width || !mask[index + 1]) {
                edges.emplace(tr, br);
            }
            if (y + 1 == height || !mask[index + width]) {
                edges.emplace(br, bl);
            }
            if (x == 0 || !mask[index - 1]) {
                edges.emplace(bl, tl);
            }
        }
    }
    std::vector<Point> largest;
    while (!edges.empty()) {
        const int first = (*edges.begin()).first;
        int current = first;
        std::vector<Point> loop;
        do {
            const std::unordered_multimap<int, int>::iterator found = edges.find(current);
            if (found == edges.end()) {
                break;
            }
            loop.push_back({static_cast<double>(current % stride), static_cast<double>(current / stride)});
            current = (*found).second;
            edges.erase(found);
        } while (current != first);
        if (loop.size() > largest.size()) {
            largest = std::move(loop);
        }
    }
    // Remove collinear grid vertices without shifting the silhouette.
    std::vector<Point> simplified;
    for (std::size_t index = 0; index < largest.size(); ++index) {
        const Point a = largest[(index + largest.size() - 1) % largest.size()], b = largest[index],
                    c = largest[(index + 1) % largest.size()];
        if ((b.x - a.x) * (c.y - b.y) != (b.y - a.y) * (c.x - b.x)) {
            simplified.push_back(b);
        }
    }
    return simplified;
}
SelectionMask tighten_lasso(const Image& image, const std::vector<Point>& polygon, bool inner_void,
                            double tolerance) {
    SelectionMask result;
    if (polygon.size() < 3 || image.width < 1 || image.height < 1) {
        return result;
    }
    double left = image.width, top = image.height, right = 0, bottom = 0;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        left = std::min(left, polygon[index].x);
        top = std::min(top, polygon[index].y);
        right = std::max(right, polygon[index].x);
        bottom = std::max(bottom, polygon[index].y);
    }
    result.bounds.x = std::clamp(static_cast<int>(std::floor(left)), 0, image.width);
    result.bounds.y = std::clamp(static_cast<int>(std::floor(top)), 0, image.height);
    result.bounds.w = std::clamp(static_cast<int>(std::ceil(right)), 0, image.width) - result.bounds.x;
    result.bounds.h = std::clamp(static_cast<int>(std::ceil(bottom)), 0, image.height) - result.bounds.y;
    if (result.bounds.w <= 0 || result.bounds.h <= 0) {
        return {};
    }
    const int width = result.bounds.w, height = result.bounds.h;
    const std::size_t size = static_cast<std::size_t>(width) * height;
    std::vector<std::uint8_t> inside(size, 0), connected(size, 0);
    std::vector<Lab> labs(size), boundary;
    Point center{result.bounds.x + width * 0.5, result.bounds.y + height * 0.5};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * width + x;
            const Point point{result.bounds.x + x + 0.5, result.bounds.y + y + 0.5};
            if (!inside_polygon(polygon, point.x, point.y)) {
                continue;
            }
            inside[index] = 1;
            labs[index] = to_oklab(image.get(result.bounds.x + x, result.bounds.y + y));
            double distance = 2;
            for (std::size_t edge = 0; edge < polygon.size(); ++edge) {
                distance = std::min(
                    distance, edge_distance(point, polygon[edge], polygon[(edge + 1) % polygon.size()]));
            }
            if (distance <= 1.5) {
                boundary.push_back(labs[index]);
            }
        }
    }
    const Lab background = inner_void
                               ? to_oklab(image.get(static_cast<int>(center.x), static_cast<int>(center.y)))
                               : median_lab(boundary);
    tolerance = std::clamp(tolerance, 0.002, 0.25);
    const double threshold = tolerance * tolerance;
    // Select the relevant connected component nearest the lasso center.
    // The color threshold deliberately absorbs small JPEG background variations.
    int seed = -1;
    double nearest = std::numeric_limits<double>::max();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * width + x;
            const double delta = distance_squared(labs[index], background);
            if (!inside[index] || (inner_void ? delta > threshold : delta <= threshold)) {
                continue;
            }
            const double dx = x + 0.5 - width * 0.5, dy = y + 0.5 - height * 0.5;
            if (dx * dx + dy * dy < nearest) {
                seed = static_cast<int>(index);
                nearest = dx * dx + dy * dy;
            }
        }
    }
    if (seed < 0) {
        return {};
    }
    std::queue<int> pending;
    pending.push(seed);
    connected[seed] = 1;
    while (!pending.empty()) {
        const int index = pending.front();
        pending.pop();
        const int x = index % width, y = index / width;
        const int neighbors[] = {x > 0 ? index - 1 : -1, x + 1 < width ? index + 1 : -1,
                                 y > 0 ? index - width : -1, y + 1 < height ? index + width : -1};
        for (int direction = 0; direction < 4; ++direction) {
            const int next = neighbors[direction];
            if (next < 0 || connected[next] || !inside[next]) {
                continue;
            }
            const double delta = distance_squared(labs[next], background);
            if (inner_void ? delta > threshold * 2.25 : delta <= threshold) {
                continue;
            }
            connected[next] = 1;
            pending.push(next);
        }
    }
    result.coverage.assign(size, 0);
    for (std::size_t index = 0; index < size; ++index) {
        if (!connected[index]) {
            continue;
        }
        const double delta = std::sqrt(distance_squared(labs[index], background));
        const double alpha = inner_void
                                 ? std::clamp((tolerance * 1.5 - delta) / (tolerance * 0.5), 0.0, 1.0)
                                 : std::clamp((delta - tolerance) / std::max(0.008, tolerance), 0.0, 1.0);
        result.coverage[index] =
            static_cast<std::uint8_t>(std::lround(alpha * alpha * (3 - 2 * alpha) * 255));
    }
    result.outline = mask_outline(result.coverage, width, height);
    return result;
}
void Guide::clear() {
    nodes.clear();
    closed = false;
    selection = {};
}
bool Guide::active() const {
    return nodes.size() >= 2 || !selection.coverage.empty();
}
double Guide::blocked(int x, int y) const {
    if (!selection.coverage.empty() && fill) {
        const int sx = x - selection.bounds.x, sy = y - selection.bounds.y;
        if (sx >= 0 && sy >= 0 && sx < selection.bounds.w && sy < selection.bounds.h) {
            return selection.coverage[static_cast<std::size_t>(sy) * selection.bounds.w + sx] / 255.0;
        }
        return 0;
    }
    if (nodes.size() < 2) {
        return 0;
    }
    const Point point{x + 0.5, y + 0.5};
    if (fill && closed && inside_polygon(nodes, point.x, point.y)) {
        return 1;
    }
    double distance = std::numeric_limits<double>::max();
    const std::size_t edges = closed ? nodes.size() : nodes.size() - 1;
    for (std::size_t index = 0; index < edges; ++index) {
        distance = std::min(distance, edge_distance(point, nodes[index], nodes[(index + 1) % nodes.size()]));
    }
    return std::clamp(width * 0.5 + 0.5 - distance, 0.0, 1.0);
}
void Guide::translate(Point delta) {
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        nodes[index].x += delta.x;
        nodes[index].y += delta.y;
    }
    selection.bounds.x += static_cast<int>(std::lround(delta.x));
    selection.bounds.y += static_cast<int>(std::lround(delta.y));
}
void constrain_paint(Image& image, const Image& base, const Guide& guide, bool preserve_alpha) {
    if (image.width != base.width || image.height != base.height || (!guide.active() && !preserve_alpha)) {
        return;
    }
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * image.width + x;
            Color color = interpolate_pixel(image.pixels[index], base.pixels[index], guide.blocked(x, y));
            if (preserve_alpha) {
                color.a = base.pixels[index].a;
                if (!color.a) {
                    color = base.pixels[index];
                }
            }
            image.pixels[index] = color;
        }
    }
}
void stencil_flood(Image& image, Point point, const Ink& ink, const Guide& guide, bool wrap) {
    if (image.width < 1 || image.height < 1) {
        return;
    }
    int x = static_cast<int>(std::floor(point.x)), y = static_cast<int>(std::floor(point.y));
    if (wrap) {
        x = (x % image.width + image.width) % image.width;
        y = (y % image.height + image.height) % image.height;
    }
    if (!image.contains(x, y) || guide.blocked(x, y) >= 0.5) {
        return;
    }
    const Color original = image.get(x, y);
    std::vector<std::uint8_t> visited(image.pixels.size(), 0);
    std::queue<int> queue;
    queue.push(y * image.width + x);
    visited[y * image.width + x] = 1;
    while (!queue.empty()) {
        const int index = queue.front();
        queue.pop();
        x = index % image.width;
        y = index / image.width;
        image.blend(x, y, patterned(ink, x, y));
        const int nx[] = {x - 1, x + 1, x, x}, ny[] = {y, y, y - 1, y + 1};
        for (int direction = 0; direction < 4; ++direction) {
            int px = nx[direction], py = ny[direction];
            if (wrap) {
                px = (px + image.width) % image.width;
                py = (py + image.height) % image.height;
            }
            if (!image.contains(px, py)) {
                continue;
            }
            const int next = py * image.width + px;
            if (visited[next] || guide.blocked(px, py) >= 0.5 || !equal(image.pixels[next], original)) {
                continue;
            }
            visited[next] = 1;
            queue.push(next);
        }
    }
}
} // namespace paint
