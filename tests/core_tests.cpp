#include "codecs.hpp"
#include "conv.hpp"
#include "document.hpp"
#include "fixtures/conv_reference.hpp"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
void test_color() {
    for (int r = 0; r <= 255; r += 17) {
        for (int g = 0; g <= 255; g += 17) {
            for (int b = 0; b <= 255; b += 17) {
                paint::Color color{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                                   static_cast<std::uint8_t>(b), 255};
                paint::Color roundtrip = paint::from_oklab(paint::to_oklab(color));
                require(paint::equal(color, roundtrip), "OKLab roundtrip differs");
                paint::Color parsed;
                require(paint::from_hex(paint::to_hex(color), parsed) && paint::equal(parsed, color),
                        "hex roundtrip differs");
            }
        }
    }
    paint::Color invalid;
    require(!paint::from_hex("#12zz34", invalid), "invalid hex accepted");
}
void test_conv() {
    paint::Image input, output;
    input.reset(13, 9, {53, 177, 229, 149});
    const int sizes[6][2] = {{29, 17}, {7, 5}, {1, 1}, {1, 19}, {31, 1}, {13, 9}};
    for (const int (&size)[2] : sizes) {
        paint::conv_resize(input, size[0], size[1], output);
        for (paint::Color pixel : output.pixels) {
            require(paint::equal(pixel, {53, 177, 229, 149}), "CONV constant preservation failed");
        }
    }
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            input.set(x, y, {static_cast<std::uint8_t>(x * 17), static_cast<std::uint8_t>(y * 23), 0, 255});
        }
    }
    paint::conv_resize(input, 25, 17, output);
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            require(paint::equal(input.get(x, y), output.get(x * 2, y * 2)),
                    "CONV nodal interpolation failed");
        }
    }
    // An unchanged dimension must preserve its rows/columns, including when
    // source and destination alias. These fixtures exercise both one-axis paths.
    paint::conv_resize(input, 25, 9, output);
    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 13; ++x) {
            require(paint::equal(input.get(x, y), output.get(x * 2, y)), "width-only resize moved a row");
        }
    }
    paint::Image original = input;
    paint::conv_resize(input, 13, 17, input);
    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 13; ++x) {
            require(paint::equal(original.get(x, y), input.get(x, y * 2)),
                    "height-only alias resize moved a column");
        }
    }
    input.reset(1, 3, {20, 40, 60, 255});
    paint::conv_resize(input, 7, 1, output);
    for (paint::Color pixel : output.pixels) {
        require(paint::equal(pixel, {20, 40, 60, 255}), "short line failed");
    }
}
void test_conv_reference() {
    for (const fixtures::Case& fixture : fixtures::cases) {
        paint::Image input, output;
        input.reset(fixture.width, fixture.height);
        std::memcpy(input.pixels.data(), fixture.input, input.pixels.size() * 4);
        paint::conv_resize(input, fixture.out_width, fixture.out_height, output);
        const std::uint8_t* actual = reinterpret_cast<const std::uint8_t*>(output.pixels.data());
        for (std::size_t i = 0; i < output.pixels.size() * 4; ++i) {
            if (actual[i] != fixture.expected[i]) {
                std::cerr << "reference difference at byte " << i << ": " << static_cast<int>(actual[i])
                          << " vs " << static_cast<int>(fixture.expected[i]) << '\n';
                throw std::runtime_error("Native CONV differs from website JavaScript reference");
            }
        }
    }
}
void test_editing() {
    paint::Document doc;
    doc.new_image(20, 20);
    doc.checkpoint();
    doc.ink.primary = {0, 0, 0, 255};
    doc.ink.pattern = paint::Pattern::Dither50;
    paint::flood(doc.image, 0, 0, doc.ink);
    int dark = 0;
    for (paint::Color pixel : doc.image.pixels) {
        if (pixel.r == 0) {
            ++dark;
        }
    }
    require(dark == 200, "pattern bucket region/density failed");
    doc.undo();
    require(doc.image.get(0, 0).r == 255, "undo failed");
    doc.redo();
    require(doc.image.get(0, 0).r == 0, "redo failed");
    doc.select({0, 0, 10, 10});
    doc.selection.x = 10;
    doc.selection.y = 10;
    doc.commit_selection();
    doc.undo();
    require(doc.image.get(0, 0).r == 0, "selection transaction undo failed");
    paint::Image stamp = paint::make_stamp(doc.image, {0, 0, 10, 10}, paint::StampShape::Circle, false, {});
    require(stamp.get(0, 0).a == 0 && stamp.get(5, 5).a == 255, "stamp mask failed");
    paint::Image textured;
    textured.reset(20, 20);
    paint::Ink fill_ink;
    fill_ink.secondary = {0, 0, 0, 255};
    paint::draw_shape(textured, paint::Shape::Rectangle, {1, 1}, {18, 18}, fill_ink, false, true,
                      paint::Brush::Watercolor);
    require(textured.get(10, 10).r > 180 && textured.get(10, 10).r < 250,
            "watercolor shape fill ignored its medium");
}
void test_codecs() {
    paint::Image image;
    image.reset(17, 13, {62, 147, 219, 255});
    image.set(4, 7, {197, 27, 55, 255});
    const char* formats[] = {"png", "bmp", "tga", "tiff", "webp", "gif", "jpg"};
    for (const char* format : formats) {
        std::filesystem::path file =
            std::filesystem::temp_directory_path() / (std::string("rainstar-codec-test.") + format);
        paint::save_image(image, file.string());
        paint::save_image(image, file.string()); // Replacing an existing picture must work too.
        paint::Image loaded = paint::load_image(file.string());
        require(loaded.width == 17 && loaded.height == 13, "codec dimensions failed");
        if (std::string(format) != "jpg" && std::string(format) != "gif") {
            require(paint::equal(loaded.get(4, 7), image.get(4, 7)), "lossless codec failed");
        }
        std::filesystem::remove(file);
        std::filesystem::path unicode = std::filesystem::temp_directory_path() /
                                        std::filesystem::path(std::u8string(u8"Rainstar-é-绘画."));
        unicode += format;
        std::u8string encoded_path = unicode.u8string();
        std::string path(encoded_path.begin(), encoded_path.end());
        paint::save_image(image, path);
        paint::save_image(image, path);
        loaded = paint::load_image(path);
        require(loaded.width == image.width && loaded.height == image.height,
                "Unicode filename roundtrip failed");
        std::filesystem::remove(unicode);
    }
}
} // namespace
int main() {
    try {
        test_color();
        test_conv();
        test_conv_reference();
        test_editing();
        test_codecs();
        std::cout << "Color, CONV, editing and seven-format codec tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
