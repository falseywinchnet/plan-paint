#include "icon_effects.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
bool same(const paint::Image& left, const paint::Image& right) {
    return left.width == right.width && left.height == right.height &&
           std::equal(left.pixels.begin(), left.pixels.end(), right.pixels.begin(), right.pixels.end(),
                      paint::equal);
}
void contracts() {
    paint::Image source;
    source.reset(16, 16, {80, 90, 100, 0});
    source.set(8, 8, {255, 0, 0, 128});
    paint::IconEffectSettings settings;
    require(same(paint::render_icon_effect(source, settings).image, source),
            "neutral brightness/contrast is exact");
    settings.brightness = 20;
    const paint::Color lifted = paint::render_icon_effect(source, settings).image.get(8, 8);
    require(paint::equal(lifted, {255, 51, 51, 128}), "brightness adds encoded RGB and preserves alpha");
    settings.brightness = 0;
    settings.contrast = -25;
    require(paint::equal(paint::render_icon_effect(source, settings).image.get(8, 8), {191, 64, 64, 128}),
            "contrast pivots around 127.5");
    settings.effect = paint::IconEffect::HueSaturation;
    settings.hue = 120;
    require(paint::equal(paint::render_icon_effect(source, settings).image.get(8, 8), {0, 255, 0, 128}),
            "hue rotates red to green");
    settings.hue = 0;
    settings.saturation = 0;
    require(paint::equal(paint::render_icon_effect(source, settings).image.get(8, 8), {255, 255, 255, 128}),
            "HSV saturation zero retains value while removing chroma");
    settings.effect = paint::IconEffect::Grayscale;
    require(paint::equal(paint::render_icon_effect(source, settings).image.get(8, 8), {54, 54, 54, 128}),
            "grayscale uses encoded Rec.709 weights");
    settings.effect = paint::IconEffect::Opacity;
    settings.opacity = 50;
    const paint::Image faded = paint::render_icon_effect(source, settings).image;
    require(paint::equal(faded.get(8, 8), {255, 0, 0, 64}) && paint::equal(faded.get(0, 0), source.get(0, 0)),
            "opacity preserves RGB including hidden RGB");
    settings.effect = paint::IconEffect::Blur;
    settings.radius = 3;
    const paint::Image blurred = paint::render_icon_effect(source, settings).image;
    require(blurred.get(8, 8).a < 128 && blurred.get(7, 8).a > 0 && blurred.get(7, 8).g == 0 &&
                blurred.get(7, 8).b == 0,
            "blur spreads premultiplied red without hidden RGB fringe");
    require(blurred.get(7, 8).a == blurred.get(9, 8).a && blurred.get(8, 7).a == blurred.get(8, 9).a,
            "Gaussian symmetry");
    settings.radius = 0;
    require(same(paint::render_icon_effect(source, settings).image, source), "zero blur exact identity");
    settings.effect = paint::IconEffect::Sharpen;
    settings.radius = 3;
    const paint::Image sharp = paint::render_icon_effect(blurred, settings).image;
    for (std::size_t i = 0; i < sharp.pixels.size(); ++i) {
        require(sharp.pixels[i].a == blurred.pixels[i].a, "sharpen preserves alpha");
    }
    settings.strength = 0;
    require(same(paint::render_icon_effect(source, settings).image, source),
            "zero sharpening exact identity");
    paint::Image detail;
    detail.reset(16, 16, {100, 100, 100, 255});
    detail.set(8, 8, {150, 150, 150, 255});
    settings.strength = 100;
    const paint::Image enhanced = paint::render_icon_effect(detail, settings).image;
    require(enhanced.get(8, 8).r > 150 && enhanced.get(7, 8).r < 100,
            "sharpen increases local contrast on both sides of an isolated detail");
    settings.effect = paint::IconEffect::Shadow;
    settings.radius = 0;
    settings.opacity = 100;
    settings.offset_x = 1;
    settings.offset_y = 0;
    source.reset(8, 8, {0, 0, 0, 0});
    source.set(7, 4, {240, 180, 80, 255});
    const paint::IconEffectResult shadow = paint::render_icon_effect(source, settings);
    require(shadow.image.width == 8 && shadow.image.height == 8 && shadow.clipped_pixels == 1,
            "shadow keeps dimensions and counts exact overflow");
    require(paint::equal(shadow.image.get(7, 4), source.get(7, 4)) &&
                shadow.overflow.get(shadow.bounds.x + 8, shadow.bounds.y + 4).a == 255,
            "overflow proof contains the cropped shadow without replacing foreground");
    settings.offset_x = -1;
    const paint::IconEffectResult inside = paint::render_icon_effect(source, settings);
    require(inside.clipped_pixels == 0 && paint::equal(inside.image.get(6, 4), {0, 0, 0, 255}),
            "negative offset places shadow inside the canvas");
    settings.effect = paint::IconEffect::Fit;
    settings.padding = 2;
    source.reset(16, 16, {255, 0, 0, 255});
    const paint::Image fitted = paint::render_icon_effect(source, settings).image;
    require(fitted.width == 16 && fitted.get(1, 8).a == 0 && fitted.get(2, 8).a == 255 &&
                fitted.get(13, 8).a == 255 && fitted.get(14, 8).a == 0,
            "explicit Fit creates requested transparent margins without changing dimensions");
    require(source.get(0, 0).a == 255, "effects never mutate their source");
    paint::Image narrow;
    narrow.reset(16, 16, {0, 0, 0, 0});
    for (int y = 4; y < 12; ++y) {
        for (int x = 6; x < 10; ++x) {
            narrow.set(x, y, {30, 80, 200, 255});
        }
    }
    const paint::Image centered = paint::render_icon_effect(narrow, settings).image;
    require(centered.get(5, 2).a == 255 && centered.get(4, 2).a == 0 && centered.get(10, 13).a == 255 &&
                centered.get(11, 13).a == 0,
            "Fit crops transparent margins, preserves aspect ratio and centers the expanded artwork");
    settings.padding = 8;
    bool rejected = false;
    try {
        static_cast<void>(paint::render_icon_effect(source, settings));
    } catch (const std::exception&) {
        rejected = true;
    }
    require(rejected, "padding cannot remove the entire drawable area");
    settings.padding = 2;
    settings.brightness = std::numeric_limits<double>::quiet_NaN();
    rejected = false;
    try {
        static_cast<void>(paint::render_icon_effect(source, settings));
    } catch (const std::exception&) {
        rejected = true;
    }
    require(rejected, "nonfinite numerical parameters rejected");
}
} // namespace
int main() {
    try {
        contracts();
        std::cout << "Icon effects contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
