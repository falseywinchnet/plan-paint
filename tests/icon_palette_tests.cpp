#include "codecs.hpp"
#include "icon_palette.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
void check_roundtrip(const paint::IconFrame& frame, unsigned depth, paint::ContainerKind kind) {
    const paint::ImageContainer container{kind, {frame}};
    const std::vector<std::uint8_t> bytes = paint::encode_icon_container(container);
    const paint::ImageContainer decoded = paint::decode_icon_container(bytes.data(), bytes.size());
    const paint::IconFrame& copy = decoded.frames[0];
    require(paint::icon_output_format(copy, kind).bits_per_pixel == depth, "indexed depth survives save");
    require(copy.image.width == frame.image.width && copy.image.height == frame.image.height,
            "indexed dimensions survive save");
    require(std::equal(copy.image.pixels.begin(), copy.image.pixels.end(), frame.image.pixels.begin(),
                       frame.image.pixels.end(), paint::equal),
            "indexed pixels and mask roundtrip");
    require(copy.xor_pixels.empty(), "transparent pixels never become legacy XOR");
    require(paint::encode_icon_container(decoded) == bytes, "encoded indexed snapshot preserved");
    if (kind == paint::ContainerKind::Cursor) {
        require(copy.hotspot_x == frame.hotspot_x && copy.hotspot_y == frame.hotspot_y,
                "cursor hotspot survives indexed conversion");
    }
}
void conversion_contracts() {
    paint::IconFrame source;
    source.image.reset(2, 2, {0, 0, 0, 255});
    source.image.set(1, 0, {255, 255, 255, 255});
    source.image.set(0, 1, {255, 255, 255, 255});
    source.hotspot_x = 1;
    paint::IconPaletteOptions options;
    options.bits_per_pixel = 1;
    const paint::IconPaletteResult monochrome = paint::convert_icon_palette(source, options);
    const std::vector<std::uint8_t>& dib = (*monochrome.frame.source).payload;
    require(dib.size() == 64 && dib[14] == 1 && dib[32] == 2, "1-bit DIB header and size");
    require(dib[48] == 0x80 && dib[52] == 0x40 && dib[56] == 0 && dib[60] == 0,
            "bottom-up MSB-first packed pixels and DWORD-aligned masks");
    check_roundtrip(monochrome.frame, 1, paint::ContainerKind::Cursor);
    source.image.set(0, 0, {0, 0, 0, 0});
    const paint::IconPaletteResult masked_mono = paint::convert_icon_palette(source, options);
    require(paint::equal(masked_mono.frame.image.get(0, 0), {0, 0, 0, 0}) &&
                paint::equal(masked_mono.frame.image.get(1, 0), {255, 255, 255, 255}) &&
                paint::equal(masked_mono.frame.image.get(1, 1), {0, 0, 0, 255}),
            "one-bit black, white and transparency share the black entry with separate AND bits");
    check_roundtrip(masked_mono.frame, 1, paint::ContainerKind::Cursor);
    source.image.reset(5, 3, {20, 80, 120, 255});
    source.image.set(0, 0, {255, 17, 90, 0});
    source.image.set(1, 0, {0, 0, 0, 127});
    source.image.set(2, 0, {0, 0, 0, 128});
    source.image.set(4, 2, {250, 60, 90, 255});
    source.hotspot_x = 4;
    source.hotspot_y = 2;
    options.bits_per_pixel = 4;
    const paint::IconPaletteResult indexed = paint::convert_icon_palette(source, options);
    require(indexed.transparent_pixels == 2 && indexed.partial_pixels == 2, "transparency loss counted");
    require(paint::equal(indexed.frame.image.get(0, 0), {0, 0, 0, 0}) &&
                indexed.frame.image.get(1, 0).a == 0 &&
                paint::equal(indexed.frame.image.get(2, 0), {127, 127, 127, 255}),
            "127 transparent, 128 matte-composited and opaque");
    for (unsigned depth : {1u, 8u}) {
        paint::IconPaletteOptions alternative = options;
        alternative.bits_per_pixel = depth;
        check_roundtrip(paint::convert_icon_palette(source, alternative).frame, depth,
                        paint::ContainerKind::Cursor);
    }
    require(source.image.get(0, 0).r == 255 && source.image.get(2, 0).a == 128, "source untouched");
    check_roundtrip(indexed.frame, 4, paint::ContainerKind::Cursor);
    require((*indexed.frame.source).payload ==
                (*paint::convert_icon_palette(source, options).frame.source).payload,
            "palette construction deterministic");
    paint::IconFrame edited = indexed.frame;
    edited.image.set(0, 0, {17, 18, 19, 255});
    require(paint::icon_output_format(edited, paint::ContainerKind::Icon).bits_per_pixel == 32,
            "further editing invalidates indexed snapshot");
    source.image.reset(256, 256);
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            source.image.set(x, y, {static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), 110, 255});
        }
    }
    options.bits_per_pixel = 8;
    const paint::IconPaletteResult plain = paint::convert_icon_palette(source, options);
    options.dither = paint::IconPaletteDither::FloydSteinberg;
    const paint::IconPaletteResult dithered = paint::convert_icon_palette(source, options);
    require(dithered.palette.size() == 256, "8-bit palette uses full capacity");
    require(!std::equal(plain.frame.image.pixels.begin(), plain.frame.image.pixels.end(),
                        dithered.frame.image.pixels.begin(), paint::equal),
            "dither changes gradient output");
    require((*dithered.frame.source).payload ==
                (*paint::convert_icon_palette(source, options).frame.source).payload,
            "error diffusion deterministic");
    check_roundtrip(dithered.frame, 8, paint::ContainerKind::Icon);
    source.image.reset(7, 2, {200, 60, 30, 0});
    source.hotspot_x = 0;
    source.hotspot_y = 0;
    for (unsigned depth : {1u, 4u, 8u}) {
        options.bits_per_pixel = depth;
        const paint::IconPaletteResult empty = paint::convert_icon_palette(source, options);
        require(empty.transparent_pixels == 14 && empty.palette.size() == 1, "empty icon palette");
        check_roundtrip(empty.frame, depth, paint::ContainerKind::Icon);
    }
    source.xor_pixels.resize(14, {255, 255, 255, 255});
    source.image.reset(7, 2, {255, 255, 255, 0});
    bool rejected = false;
    try {
        static_cast<void>(paint::convert_icon_palette(source, options));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "legacy XOR is rejected before conversion");
    source.xor_pixels.clear();
    source.image.pixels.pop_back();
    rejected = false;
    try {
        static_cast<void>(paint::convert_icon_palette(source, options));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "malformed image geometry is rejected");
}
} // namespace
int main() {
    try {
        conversion_contracts();
        std::cout << "Icon palette contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
