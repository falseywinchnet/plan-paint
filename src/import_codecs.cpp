#include "import_codecs.hpp"
#include "codecs.hpp"
#include "conv.hpp"
#include "raster.hpp"
#include <algorithm>
#include <avif/avif.h>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <resvg.h>
#include <stdexcept>
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
Image rasterize_svg(const std::string& path, int width, int height) {
    std::vector<std::uint8_t> bytes = read_image_bytes(path);
    std::unique_ptr<resvg_options, void (*)(resvg_options*)> options(resvg_options_create(),
                                                                     resvg_options_destroy);
    if (!options) {
        throw std::runtime_error("Could not allocate SVG rasterizer.");
    }
    resvg_options_load_system_fonts(options.get());
#ifdef __linux__
    resvg_options_set_font_family(options.get(), "DejaVu Sans");
#else
    resvg_options_set_font_family(options.get(), "Arial");
#endif
    std::filesystem::path directory =
        std::filesystem::path(std::u8string(path.begin(), path.end())).parent_path();
    std::u8string directory_bytes = directory.u8string();
    std::string resources(directory_bytes.begin(), directory_bytes.end());
    resvg_options_set_resources_dir(options.get(), resources.c_str());
    resvg_render_tree* raw_tree = nullptr;
    int error = resvg_parse_tree_from_data(reinterpret_cast<const char*>(bytes.data()), bytes.size(),
                                           options.get(), &raw_tree);
    if (error || !raw_tree) {
        throw std::runtime_error("SVG could not be parsed (error " + std::to_string(error) + ").");
    }
    std::unique_ptr<resvg_render_tree, void (*)(resvg_render_tree*)> tree(raw_tree, resvg_tree_destroy);
    resvg_size size = resvg_get_image_size(tree.get());
    if (!std::isfinite(size.width) || !std::isfinite(size.height) || size.width <= 0 || size.height <= 0 ||
        size.width > 16384 || size.height > 16384) {
        throw std::runtime_error("SVG dimensions are invalid or exceed the canvas limit.");
    }
    if (width <= 0) {
        width = static_cast<int>(std::ceil(size.width));
    }
    if (height <= 0) {
        height = static_cast<int>(std::ceil(size.height));
    }
    Image image;
    image.reset(width, height, {0, 0, 0, 0});
    resvg_transform transform = resvg_transform_identity();
    transform.a = width / size.width;
    transform.d = height / size.height;
    resvg_render(tree.get(), transform, width, height, reinterpret_cast<char*>(image.pixels.data()));
    unpremultiply(image);
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
