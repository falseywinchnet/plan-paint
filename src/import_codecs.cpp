#include "import_codecs.hpp"
#include "codecs.hpp"
#include "conv.hpp"
#include "raster.hpp"
#include "text.hpp"
#include <algorithm>
#include <avif/avif.h>
#include <cmath>
#include <cstring>
#include <filesystem>
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
    } else if (size >= 13 && (std::memcmp(data, "GIF87a", 6) == 0 || std::memcmp(data, "GIF89a", 6) == 0)) {
        std::size_t offset = 13 + ((data[10] & 128) ? 3u * (2u << (data[10] & 7)) : 0);
        int frames = 0;
        while (offset < size) {
            unsigned marker = data[offset++];
            if (marker == 0x3b) {
                break;
            }
            if (marker == 0x2c) {
                if (++frames > 1) {
                    throw std::runtime_error(animation_error);
                }
                if (size - offset < 9) {
                    throw std::runtime_error("GIF image descriptor is truncated.");
                }
                unsigned flags = data[offset + 8];
                offset += 9;
                if (flags & 128) {
                    offset += 3u * (2u << (flags & 7));
                }
                if (offset >= size) {
                    throw std::runtime_error("GIF image data is truncated.");
                }
                ++offset;
            } else if (marker == 0x21) {
                if (offset >= size) {
                    throw std::runtime_error("GIF extension is truncated.");
                }
                ++offset;
            } else {
                throw std::runtime_error("GIF contains an invalid block.");
            }
            bool terminated = false;
            while (offset < size) {
                std::size_t length = data[offset++];
                if (length > size - offset) {
                    throw std::runtime_error("GIF data block is truncated.");
                }
                offset += length;
                if (!length) {
                    terminated = true;
                    break;
                }
            }
            if (!terminated) {
                throw std::runtime_error("GIF data block is incomplete.");
            }
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
    (*decoder).allowProgressive = AVIF_FALSE;
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
std::string svg_base64(const std::vector<std::uint8_t>& bytes) {
    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result = "data:application/octet-stream;base64,";
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        std::uint32_t value = std::uint32_t(bytes[i]) << 16;
        if (i + 1 < bytes.size()) {
            value |= std::uint32_t(bytes[i + 1]) << 8;
        }
        if (i + 2 < bytes.size()) {
            value |= bytes[i + 2];
        }
        result += alphabet[(value >> 18) & 63];
        result += alphabet[(value >> 12) & 63];
        result += i + 1 < bytes.size() ? alphabet[(value >> 6) & 63] : '=';
        result += i + 2 < bytes.size() ? alphabet[value & 63] : '=';
    }
    return result;
}
void prepare_svg_element(tinyxml2::XMLElement& element, const std::filesystem::path& directory) {
    const char* filter_error = "This SVG uses filters, which LunaSVG does not render. Export it as PNG in "
                               "its source application to preserve those effects.";
    std::string name = svg_local_name(element.Name());
    if (name == "filter") {
        throw std::runtime_error(filter_error);
    }
    static const std::regex css_filter(R"((^|[;{\s])filter\s*:)", std::regex::icase);
    if (name == "style" && element.GetText() && std::regex_search(element.GetText(), css_filter)) {
        throw std::runtime_error(filter_error);
    }
    std::string image_attribute, image_source;
    for (const tinyxml2::XMLAttribute* attribute = element.FirstAttribute(); attribute;
         attribute = (*attribute).Next()) {
        std::string key = svg_local_name((*attribute).Name()), value = (*attribute).Value();
        if ((key == "filter" && value != "none" && !value.empty()) ||
            (key == "style" && std::regex_search(value, css_filter))) {
            throw std::runtime_error(filter_error);
        }
        if (name == "image" && key == "href" && value.compare(0, 5, "data:") != 0) {
            image_attribute = (*attribute).Name();
            image_source = value;
        }
    }
    if (!image_source.empty()) {
        if (image_source.find("://") != std::string::npos || image_source.compare(0, 2, "//") == 0) {
            throw std::runtime_error("SVG images must be embedded or stored locally beside the SVG.");
        }
        std::filesystem::path file =
            directory / std::filesystem::path(std::u8string(image_source.begin(), image_source.end()));
        std::u8string encoded = file.u8string();
        std::string resource(encoded.begin(), encoded.end());
        std::string embedded = svg_base64(read_image_bytes(resource));
        element.SetAttribute(image_attribute.c_str(), embedded.c_str());
    }
    for (tinyxml2::XMLElement* child = element.FirstChildElement(); child;
         child = (*child).NextSiblingElement()) {
        prepare_svg_element(*child, directory);
    }
}
} // namespace
Image rasterize_svg(const std::string& path, int width, int height) {
    static std::once_flag fonts_ready;
    std::call_once(fonts_ready, register_svg_fallback_fonts);
    std::vector<std::uint8_t> bytes = read_image_bytes(path);
    tinyxml2::XMLDocument xml;
    if (xml.Parse(reinterpret_cast<const char*>(bytes.data()), bytes.size()) != tinyxml2::XML_SUCCESS ||
        !xml.RootElement()) {
        throw std::runtime_error("SVG could not be parsed.");
    }
    std::filesystem::path directory =
        std::filesystem::path(std::u8string(path.begin(), path.end())).parent_path();
    prepare_svg_element(*xml.RootElement(), directory);
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
