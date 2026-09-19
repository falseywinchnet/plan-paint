#include "import_codecs.hpp"
#include "codecs.hpp"
#include "conv.hpp"
#include "raster.hpp"
#include "text.hpp"
#include <algorithm>
#include <avif/avif.h>
#include <cmath>
#include <cstring>
#include <lunasvg.h>
#include <memory>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <tinyxml2.h>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif
namespace paint {
namespace {
const char* animation_error =
    "Animation containers are not supported. Open a sprite sheet in Atlas to edit and step through frames.";
std::uint32_t big32(const std::uint8_t* data) {
    return (std::uint32_t(data[0]) << 24) | (std::uint32_t(data[1]) << 16) | (std::uint32_t(data[2]) << 8) |
           data[3];
}
bool heif_brand(const std::uint8_t* brand) {
    static const char accepted[][5] = {"heic", "heix", "hevc", "hevx",
                                        "heim", "heis", "hevm", "hevs"};
    for (std::size_t index = 0; index < std::size(accepted); ++index) {
        if (std::memcmp(brand, accepted[index], 4) == 0) {
            return true;
        }
    }
    return false;
}
bool heif_signature(const void* data, std::size_t size) {
    if (size < 16) {
        return false;
    }
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data);
    const std::uint32_t box_size = big32(bytes);
    if (box_size < 16 || box_size > size || std::memcmp(bytes + 4, "ftyp", 4) != 0) {
        return false;
    }
    if (heif_brand(bytes + 8)) {
        return true;
    }
    for (std::size_t offset = 16; offset + 4 <= box_size; offset += 4) {
        if (heif_brand(bytes + offset)) {
            return true;
        }
    }
    return false;
}
void unpremultiply(Image& image) {
    for (std::size_t i = 0; i < image.pixels.size(); ++i) {
        Color& c = image.pixels[i];
        if (!c.a) {
            c = {0, 0, 0, 0};
        } else if (c.a < 255) {
            c.r = static_cast<std::uint8_t>(std::min(255, (int(c.r) * 255 + c.a / 2) / c.a));
            c.g = static_cast<std::uint8_t>(std::min(255, (int(c.g) * 255 + c.a / 2) / c.a));
            c.b = static_cast<std::uint8_t>(std::min(255, (int(c.b) * 255 + c.a / 2) / c.a));
        }
    }
}
} // namespace
void reject_animation(const std::uint8_t* data, std::size_t size) {
    const unsigned char png[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size >= 8 && std::memcmp(data, png, 8) == 0) {
        std::size_t offset = 8;
        while (offset + 12 <= size) {
            std::size_t length = big32(data + offset);
            if (length > size - offset - 12) {
                throw std::runtime_error("PNG chunk is truncated.");
            }
            if (std::memcmp(data + offset + 4, "acTL", 4) == 0) {
                throw std::runtime_error(animation_error);
            }
            offset += length + 12;
        }
    }
}
bool is_avif(const void* data, std::size_t size) {
    avifROData raw{static_cast<const std::uint8_t*>(data), size};
    return avifPeekCompatibleFileType(&raw) == AVIF_TRUE;
}
Image decode_avif(const void* data, std::size_t size) {
    std::unique_ptr<avifDecoder, void (*)(avifDecoder*)> decoder(avifDecoderCreate(), avifDecoderDestroy);
    if (!decoder) {
        throw std::runtime_error("Could not allocate AVIF decoder.");
    }
    (*decoder).maxThreads = 4;
    (*decoder).imageSizeLimit = 64000000;
    (*decoder).imageDimensionLimit = 16384;
    (*decoder).imageCountLimit = 1;
    (*decoder).allowProgressive = AVIF_FALSE;
    (*decoder).ignoreExif = AVIF_TRUE;
    (*decoder).ignoreXMP = AVIF_TRUE;
    avifResult result = avifDecoderSetIOMemory(decoder.get(), static_cast<const std::uint8_t*>(data), size);
    if (result == AVIF_RESULT_OK) {
        result = avifDecoderParse(decoder.get());
    }
    if (result != AVIF_RESULT_OK) {
        throw std::runtime_error(std::string("AVIF: ") + avifResultToString(result));
    }
    if ((*decoder).imageCount != 1) {
        throw std::runtime_error(animation_error);
    }
    result = avifDecoderNextImage(decoder.get());
    if (result != AVIF_RESULT_OK) {
        throw std::runtime_error(std::string("AVIF: ") + avifResultToString(result));
    }
    const avifImage& source = *(*decoder).image;
    if (source.width > 16384 || source.height > 16384 || source.width == 0 || source.height == 0 ||
        static_cast<std::uint64_t>(source.width) * source.height > 64000000) {
        throw std::runtime_error("AVIF dimensions are invalid or exceed the canvas limit.");
    }
    Image image;
    image.reset(static_cast<int>(source.width), static_cast<int>(source.height), {0, 0, 0, 0});
    avifRGBImage rgb{};
    avifRGBImageSetDefaults(&rgb, &source);
    rgb.depth = 8;
    rgb.format = AVIF_RGB_FORMAT_RGBA;
    rgb.alphaPremultiplied = AVIF_FALSE;
    rgb.pixels = reinterpret_cast<std::uint8_t*>(image.pixels.data());
    rgb.rowBytes = image.width * 4;
    result = avifImageYUVToRGB(&source, &rgb);
    if (result != AVIF_RESULT_OK) {
        throw std::runtime_error(std::string("AVIF color conversion: ") + avifResultToString(result));
    }
    if (source.transformFlags & AVIF_TRANSFORM_CLAP) {
        avifCropRect crop{};
        avifDiagnostics diagnostics{};
        if (!avifCropRectFromCleanApertureBox(&crop, &source.clap, source.width, source.height,
                                              &diagnostics)) {
            throw std::runtime_error("AVIF has an invalid clean aperture.");
        }
        image = cropped(image, {static_cast<int>(crop.x), static_cast<int>(crop.y),
                                static_cast<int>(crop.width), static_cast<int>(crop.height)});
    }
    if (source.transformFlags & AVIF_TRANSFORM_IROT) {
        image = rotate_quarter(image, (4 - source.irot.angle) % 4);
    }
    if (source.transformFlags & AVIF_TRANSFORM_IMIR) {
        image = flipped(image, source.imir.axis == 1);
    }
    return image;
}
namespace {
void register_svg_fallback_fonts() {
    // SVG imports must retain text even on a clean machine without system fonts.
    // These immutable bytes are the same bundled faces used by Paint's text tool.
    struct Face {
        const unsigned char* bytes;
        unsigned int length;
        bool bold;
        bool italic;
    };
    const Face faces[] = {{embedded_font, embedded_font_size, false, false},
                          {embedded_font_bold, embedded_font_bold_size, true, false},
                          {embedded_font_italic, embedded_font_italic_size, false, true},
                          {embedded_font_bolditalic, embedded_font_bolditalic_size, true, true}};
    for (std::size_t index = 0; index < std::size(faces); ++index) {
        const Face& face = faces[index];
        if (!lunasvg_add_font_face_from_data("", face.bold, face.italic, face.bytes, face.length, nullptr,
                                             nullptr)) {
            throw std::runtime_error("Could not load the bundled SVG fallback font.");
        }
    }
}
std::string svg_local_name(const char* name) {
    const char* colon = std::strchr(name, ':');
    return colon ? colon + 1 : name;
}
void prepare_svg_element(tinyxml2::XMLElement& element, int depth, std::size_t& element_count,
                         std::size_t& attribute_count) {
    if (depth > 128 || ++element_count > 100000) {
        throw std::runtime_error("SVG structure exceeds the safe element or nesting limit.");
    }
    const char* filter_error = "This SVG uses filters, which LunaSVG does not render. Export it as PNG in "
                               "its source application to preserve those effects.";
    std::string name = svg_local_name(element.Name());
    if (name == "image") {
        throw std::runtime_error(
            "SVG raster image elements are disabled because LunaSVG's embedded image decoder is not safe "
            "for untrusted files. Replace the image element with vector artwork.");
    }
    if (name == "filter") {
        throw std::runtime_error(filter_error);
    }
    static const std::regex css_filter(R"((^|[;{\s])filter\s*:)", std::regex::icase);
    if (name == "style" && element.GetText() && std::regex_search(element.GetText(), css_filter)) {
        throw std::runtime_error(filter_error);
    }
    for (const tinyxml2::XMLAttribute* attribute = element.FirstAttribute(); attribute;
         attribute = (*attribute).Next()) {
        if (++attribute_count > 200000) {
            throw std::runtime_error("SVG structure exceeds the safe attribute limit.");
        }
        std::string key = svg_local_name((*attribute).Name()), value = (*attribute).Value();
        if ((key == "filter" && value != "none" && !value.empty()) ||
            (key == "style" && std::regex_search(value, css_filter))) {
            throw std::runtime_error(filter_error);
        }
    }
    for (tinyxml2::XMLElement* child = element.FirstChildElement(); child;
         child = (*child).NextSiblingElement()) {
        prepare_svg_element(*child, depth + 1, element_count, attribute_count);
    }
}
} // namespace
Image rasterize_svg(const std::string& path, int width, int height) {
    static std::once_flag fonts_ready;
    std::call_once(fonts_ready, register_svg_fallback_fonts);
    std::vector<std::uint8_t> bytes = read_image_bytes(path, 16000000);
    tinyxml2::XMLDocument xml;
    if (xml.Parse(reinterpret_cast<const char*>(bytes.data()), bytes.size()) != tinyxml2::XML_SUCCESS ||
        !xml.RootElement()) {
        throw std::runtime_error("SVG could not be parsed.");
    }
    std::size_t element_count = 0, attribute_count = 0;
    prepare_svg_element(*xml.RootElement(), 1, element_count, attribute_count);
    tinyxml2::XMLPrinter prepared;
    xml.Print(&prepared);
    std::unique_ptr<lunasvg::Document> tree = lunasvg::Document::loadFromData(prepared.CStr());
    if (!tree) {
        throw std::runtime_error("SVG could not be parsed by LunaSVG.");
    }
    double intrinsic_width = (*tree).width(), intrinsic_height = (*tree).height();
    if (!std::isfinite(intrinsic_width) || !std::isfinite(intrinsic_height) || intrinsic_width <= 0 ||
        intrinsic_height <= 0 || intrinsic_width > 16384 || intrinsic_height > 16384) {
        throw std::runtime_error("SVG dimensions are invalid or exceed the canvas limit.");
    }
    if (width <= 0) {
        width = static_cast<int>(std::ceil(intrinsic_width));
    }
    if (height <= 0) {
        height = static_cast<int>(std::ceil(intrinsic_height));
    }
    Image image;
    image.reset(width, height, {0, 0, 0, 0});
    lunasvg::Bitmap bitmap(reinterpret_cast<std::uint8_t*>(image.pixels.data()), width, height, width * 4);
    if (bitmap.isNull()) {
        throw std::runtime_error("Could not allocate SVG rasterizer.");
    }
    (*tree).render(bitmap, lunasvg::Matrix(width / intrinsic_width, 0, 0, height / intrinsic_height, 0, 0));
    bitmap.convertToRGBA();
    return image;
}
Image decode_native_heif(const void* data, std::size_t size) {
#ifdef __APPLE__
    if (!heif_signature(data, size)) {
        throw std::runtime_error("The selected HEIC/HEIF file does not contain a recognized HEVC image.");
    }
    CFDataRef encoded =
        CFDataCreate(kCFAllocatorDefault, static_cast<const UInt8*>(data), static_cast<CFIndex>(size));
    if (!encoded) {
        throw std::runtime_error("Could not allocate HEIF import data.");
    }
    CGImageSourceRef source = CGImageSourceCreateWithData(encoded, nullptr);
    CFRelease(encoded);
    if (!source) {
        throw std::runtime_error("This macOS installation could not decode the HEIC/HEIF image.");
    }
    std::size_t index = CGImageSourceGetPrimaryImageIndex(source);
    CFDictionaryRef properties = CGImageSourceCopyPropertiesAtIndex(source, index, nullptr);
    int width = 0, height = 0;
    if (properties) {
        CFNumberRef w =
            static_cast<CFNumberRef>(CFDictionaryGetValue(properties, kCGImagePropertyPixelWidth));
        CFNumberRef h =
            static_cast<CFNumberRef>(CFDictionaryGetValue(properties, kCGImagePropertyPixelHeight));
        if (w) {
            CFNumberGetValue(w, kCFNumberIntType, &width);
        }
        if (h) {
            CFNumberGetValue(h, kCFNumberIntType, &height);
        }
        CFRelease(properties);
    }
    if (width < 1 || height < 1 || width > 16384 || height > 16384 ||
        std::int64_t(width) * height > 64000000) {
        CFRelease(source);
        throw std::runtime_error("HEIF dimensions are invalid or exceed the canvas limit.");
    }
    int maximum = std::max(width, height);
    CFNumberRef max_size = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &maximum);
    const void* keys[] = {kCGImageSourceCreateThumbnailFromImageAlways,
                          kCGImageSourceCreateThumbnailWithTransform, kCGImageSourceThumbnailMaxPixelSize};
    const void* values[] = {kCFBooleanTrue, kCFBooleanTrue, max_size};
    CFDictionaryRef options =
        CFDictionaryCreate(kCFAllocatorDefault, keys, values, 3, &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks);
    CGImageRef decoded = CGImageSourceCreateThumbnailAtIndex(source, index, options);
    CFRelease(options);
    CFRelease(max_size);
    CFRelease(source);
    if (!decoded) {
        throw std::runtime_error("macOS ImageIO could not rasterize this HEIC/HEIF image.");
    }
    try {
        Image image;
        image.reset(static_cast<int>(CGImageGetWidth(decoded)), static_cast<int>(CGImageGetHeight(decoded)),
                    {0, 0, 0, 0});
        CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGContextRef context = CGBitmapContextCreate(
            image.pixels.data(), image.width, image.height, 8, image.width * 4, space,
            static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(space);
        if (!context) {
            throw std::runtime_error("Could not allocate the HEIF raster surface.");
        }
        CGContextDrawImage(context, CGRectMake(0, 0, image.width, image.height), decoded);
        CGContextRelease(context);
        CGImageRelease(decoded);
        unpremultiply(image);
        return image;
    } catch (...) {
        CGImageRelease(decoded);
        throw;
    }
#else
    static_cast<void>(data);
    static_cast<void>(size);
    throw std::runtime_error(
        "HEIC/HEIF import uses macOS ImageIO. On this platform, convert the image to PNG first.");
#endif
}
} // namespace paint
