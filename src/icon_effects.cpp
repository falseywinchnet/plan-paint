#include "icon_effects.hpp"
#include "conv.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace paint {
namespace {
std::uint8_t byte(double value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 255.0)));
}
bool in_range(double value, double low, double high) {
    return std::isfinite(value) && value >= low && value <= high;
}
Image gaussian(const Image& source, int radius) {
    if (radius == 0) {
        return source;
    }
    const double sigma = std::max(0.5, radius / 3.0);
    std::vector<double> kernel(static_cast<std::size_t>(radius * 2 + 1));
    double sum = 0;
    for (int x = -radius; x <= radius; ++x) {
        kernel[x + radius] = std::exp(-double(x * x) / (2 * sigma * sigma));
        sum += kernel[x + radius];
    }
    for (double& value : kernel) {
        value /= sum;
    }
    std::vector<std::array<double, 4>> horizontal(source.pixels.size());
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            std::array<double, 4>& pixel = horizontal[static_cast<std::size_t>(y) * source.width + x];
            for (int dx = -radius; dx <= radius; ++dx) {
                if (!source.contains(x + dx, y)) {
                    continue;
                }
                const Color color = source.get(x + dx, y);
                const double weight = kernel[dx + radius], alpha = color.a / 255.0;
                pixel[0] += color.r * alpha * weight;
                pixel[1] += color.g * alpha * weight;
                pixel[2] += color.b * alpha * weight;
                pixel[3] += color.a * weight;
            }
        }
    }
    Image result;
    result.reset(source.width, source.height, {0, 0, 0, 0});
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            std::array<double, 4> pixel{};
            for (int dy = -radius; dy <= radius; ++dy) {
                if (y + dy < 0 || y + dy >= source.height) {
                    continue;
                }
                const std::array<double, 4>& input =
                    horizontal[static_cast<std::size_t>(y + dy) * source.width + x];
                for (int channel = 0; channel < 4; ++channel) {
                    pixel[channel] += input[channel] * kernel[dy + radius];
                }
            }
            if (byte(pixel[3]) > 0) {
                result.set(x, y,
                           {byte(pixel[0] * 255 / pixel[3]), byte(pixel[1] * 255 / pixel[3]),
                            byte(pixel[2] * 255 / pixel[3]), byte(pixel[3])});
            }
        }
    }
    return result;
}
Color hue_saturation(Color color, double degrees, double saturation) {
    const double r = color.r / 255.0, g = color.g / 255.0, b = color.b / 255.0;
    const double high = std::max({r, g, b}), low = std::min({r, g, b}), delta = high - low;
    double hue = 0;
    if (delta > 0) {
        hue = high == r ? (g - b) / delta : high == g ? (b - r) / delta + 2 : (r - g) / delta + 4;
    }
    hue = std::fmod(hue + degrees / 60 + 12, 6.0);
    const double chroma = high * std::clamp((high > 0 ? delta / high : 0) * saturation / 100, 0.0, 1.0);
    const double x = chroma * (1 - std::abs(std::fmod(hue, 2.0) - 1)), m = high - chroma;
    const std::array<double, 3> sector[] = {{chroma, x, 0}, {x, chroma, 0}, {0, chroma, x},
                                            {0, x, chroma}, {x, 0, chroma}, {chroma, 0, x}};
    const std::array<double, 3>& selected = sector[static_cast<int>(hue)];
    return {byte((selected[0] + m) * 255), byte((selected[1] + m) * 255), byte((selected[2] + m) * 255),
            color.a};
}
Image fit_artwork(const Image& source, int padding) {
    if (padding * 2 >= std::min(source.width, source.height)) {
        throw std::runtime_error("Padding leaves no room for artwork.");
    }
    int left = source.width, top = source.height, right = -1, bottom = -1;
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            if (source.get(x, y).a) {
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x);
                bottom = std::max(bottom, y);
            }
        }
    }
    if (right < left) {
        return source;
    }
    const Image artwork = cropped(source, {left, top, right - left + 1, bottom - top + 1});
    const double scale = std::min(double(source.width - padding * 2) / artwork.width,
                                  double(source.height - padding * 2) / artwork.height);
    const int width = std::max(1, static_cast<int>(std::lround(artwork.width * scale)));
    const int height = std::max(1, static_cast<int>(std::lround(artwork.height * scale)));
    Image resized, result;
    conv_resize(artwork, width, height, resized);
    result.reset(source.width, source.height, {0, 0, 0, 0});
    composite(result, resized, (source.width - width) / 2, (source.height - height) / 2);
    return result;
}
} // namespace
IconEffectResult render_icon_effect(const Image& source, const IconEffectSettings& settings) {
    if (settings.effect < IconEffect::BrightnessContrast || settings.effect > IconEffect::Fit ||
        source.width < 1 || source.height < 1 || source.width > 256 || source.height > 256 ||
        source.pixels.size() != static_cast<std::size_t>(source.width) * source.height ||
        !in_range(settings.brightness, -100, 100) || !in_range(settings.contrast, -100, 100) ||
        !in_range(settings.hue, -180, 180) || !in_range(settings.saturation, 0, 200) ||
        !in_range(settings.opacity, 0, 100) || !in_range(settings.strength, 0, 300) || settings.radius < 0 ||
        settings.radius > 32 || settings.padding < 0 || settings.padding > 127 || settings.offset_x < -128 ||
        settings.offset_x > 128 || settings.offset_y < -128 || settings.offset_y > 128) {
        throw std::runtime_error("Invalid icon effect settings or image dimensions.");
    }
    IconEffectResult result;
    result.image = source;
    result.bounds = {0, 0, source.width, source.height};
    if (settings.effect == IconEffect::Fit) {
        result.image = fit_artwork(source, settings.padding);
    } else if (settings.effect == IconEffect::Blur) {
        result.image = gaussian(source, settings.radius);
    } else if (settings.effect == IconEffect::Shadow) {
        const int border =
            settings.radius + std::max(std::abs(settings.offset_x), std::abs(settings.offset_y));
        Image mask;
        mask.reset(source.width + border * 2, source.height + border * 2, {0, 0, 0, 0});
        for (int y = 0; y < source.height; ++y) {
            for (int x = 0; x < source.width; ++x) {
                Color color = settings.shadow;
                color.a = byte(source.get(x, y).a * (settings.shadow.a / 255.0) * settings.opacity / 100);
                mask.set(x + border + settings.offset_x, y + border + settings.offset_y, color);
            }
        }
        result.overflow = gaussian(mask, settings.radius);
        composite(result.overflow, source, border, border);
        result.bounds = {border, border, source.width, source.height};
        result.image = cropped(result.overflow, result.bounds);
        for (int y = 0; y < result.overflow.height; ++y) {
            for (int x = 0; x < result.overflow.width; ++x) {
                if ((x < border || y < border || x >= border + source.width || y >= border + source.height) &&
                    result.overflow.get(x, y).a) {
                    ++result.clipped_pixels;
                }
            }
        }
    } else if (settings.effect == IconEffect::Sharpen) {
        const Image blurred = gaussian(source, settings.radius);
        const double amount = settings.strength / 100;
        for (std::size_t index = 0; index < source.pixels.size(); ++index) {
            const Color color = source.pixels[index], soft = blurred.pixels[index];
            if (!color.a) {
                continue;
            }
            result.image.pixels[index] = {byte(color.r + amount * (color.r - soft.r)),
                                          byte(color.g + amount * (color.g - soft.g)),
                                          byte(color.b + amount * (color.b - soft.b)), color.a};
        }
    } else {
        for (Color& color : result.image.pixels) {
            if (!color.a) {
                continue;
            }
            if (settings.effect == IconEffect::BrightnessContrast) {
                const double factor = std::pow(2.0, settings.contrast / 25),
                             lift = settings.brightness * 255 / 100;
                color = {byte((color.r - 127.5) * factor + 127.5 + lift),
                         byte((color.g - 127.5) * factor + 127.5 + lift),
                         byte((color.b - 127.5) * factor + 127.5 + lift), color.a};
            } else if (settings.effect == IconEffect::HueSaturation) {
                color = hue_saturation(color, settings.hue, settings.saturation);
            } else if (settings.effect == IconEffect::Grayscale) {
                const std::uint8_t gray = byte(0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b);
                color = {gray, gray, gray, color.a};
            } else if (settings.effect == IconEffect::Opacity) {
                color.a = byte(color.a * settings.opacity / 100);
            } else {
                throw std::runtime_error("Unknown icon effect.");
            }
        }
    }
    return result;
}
} // namespace paint
