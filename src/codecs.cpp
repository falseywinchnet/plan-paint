#include "codecs.hpp"
#define STBI_WINDOWS_UTF8
#define STBIW_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <cstdio>
#include <filesystem>
#ifdef _WIN32
static FILE* utf8_fopen(const char* path, const char* mode) {
    std::string utf8_path = path;
    std::filesystem::path native = std::filesystem::path(std::u8string(utf8_path.begin(), utf8_path.end()));
    std::wstring wide_mode;
    for (const char* letter = mode; *letter; ++letter) {
        wide_mode.push_back(static_cast<wchar_t>(*letter));
    }
    FILE* file = nullptr;
    _wfopen_s(&file, native.c_str(), wide_mode.c_str());
    return file;
}
#define fopen utf8_fopen
#endif
#include "gif.h"
#ifdef _WIN32
#undef fopen
#endif
#include "stb_image_write.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <tiffio.h>
#include <webp/decode.h>
#include <webp/encode.h>

namespace paint {
static std::string extension(const std::string& path) {
    std::string result = std::filesystem::path(std::u8string(path.begin(), path.end())).extension().string();
    for (char& value : result) {
        value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    }
    return result;
}
Image decode_image(const void* data, std::size_t size) {
    if (size > 256000000) {
        throw std::runtime_error("Encoded file exceeds 256 MB.");
    }
    int width = 0, height = 0, channels = 0;
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    if (!stbi_info_from_memory(bytes, static_cast<int>(size), &width, &height, &channels)) {
        if (!WebPGetInfo(bytes, size, &width, &height)) {
            throw std::runtime_error("This image format could not be decoded.");
        }
        Image result;
        result.reset(width, height);
        if (!WebPDecodeRGBAInto(bytes, size, reinterpret_cast<std::uint8_t*>(result.pixels.data()),
                                result.pixels.size() * 4, width * 4)) {
            throw std::runtime_error("WebP decoding failed.");
        }
        return result;
    }
    Image result;
    result.reset(width, height);
    unsigned char* pixels =
        stbi_load_from_memory(bytes, static_cast<int>(size), &width, &height, &channels, 4);
    if (!pixels) {
        throw std::runtime_error(stbi_failure_reason());
    }
    std::memcpy(result.pixels.data(), pixels, result.pixels.size() * 4);
    stbi_image_free(pixels);
    return result;
}
static TIFF* open_tiff(const std::string& path, const char* mode) {
#ifdef _WIN32
    std::filesystem::path native = std::filesystem::path(std::u8string(path.begin(), path.end()));
    return TIFFOpenW(native.c_str(), mode);
#else
    return TIFFOpen(path.c_str(), mode);
#endif
}
Image load_image(const std::string& path) {
    std::string ext = extension(path);
    if (ext == ".tif" || ext == ".tiff") {
        TIFF* file = open_tiff(path, "r");
        if (!file) {
            throw std::runtime_error("Could not open TIFF.");
        }
        try {
            std::uint32_t width = 0, height = 0;
            TIFFGetField(file, TIFFTAG_IMAGEWIDTH, &width);
            TIFFGetField(file, TIFFTAG_IMAGELENGTH, &height);
            Image result;
            result.reset(static_cast<int>(width), static_cast<int>(height));
            std::vector<std::uint32_t> raster(result.pixels.size());
            if (!TIFFReadRGBAImageOriented(file, width, height, raster.data(), ORIENTATION_TOPLEFT, 0)) {
                throw std::runtime_error("TIFF decoding failed.");
            }
            for (std::size_t i = 0; i < raster.size(); ++i) {
                std::uint32_t pixel = raster[i];
                unsigned alpha = TIFFGetA(pixel);
                unsigned r = TIFFGetR(pixel), g = TIFFGetG(pixel), b = TIFFGetB(pixel);
                if (alpha > 0 && alpha < 255) {
                    r = std::min(255u, (r * 255 + alpha / 2) / alpha);
                    g = std::min(255u, (g * 255 + alpha / 2) / alpha);
                    b = std::min(255u, (b * 255 + alpha / 2) / alpha);
                }
                result.pixels[i] = {static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                                    static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(alpha)};
            }
            TIFFClose(file);
            return result;
        } catch (...) {
            TIFFClose(file);
            throw;
        }
    }
    std::ifstream file(std::filesystem::path(std::u8string(path.begin(), path.end())),
                       std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Could not open image file.");
    }
    std::streamoff size = file.tellg();
    if (size <= 0 || size > 256000000) {
        throw std::runtime_error("File is empty or exceeds 256 MB.");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        throw std::runtime_error("Could not read complete image.");
    }
    Image result = decode_image(bytes.data(), bytes.size());
    return result;
}
static void png_write(void* context, void* data, int size) {
    std::vector<std::uint8_t>& bytes = *static_cast<std::vector<std::uint8_t>*>(context);
    std::uint8_t* first = static_cast<std::uint8_t*>(data);
    bytes.insert(bytes.end(), first, first + size);
}
std::vector<std::uint8_t> encode_bmp(const Image& source) {
    Image image;
    image.reset(source.width, source.height);
    composite(image, source, 0, 0);
    std::vector<std::uint8_t> bytes;
    int ok = stbi_write_bmp_to_func(png_write, &bytes, image.width, image.height, 4, image.pixels.data());
    if (!ok) {
        throw std::runtime_error("Bitmap encoding failed.");
    }
    return bytes;
}
std::vector<std::uint8_t> encode_png(const Image& image) {
    std::vector<std::uint8_t> bytes;
    int ok = stbi_write_png_to_func(png_write, &bytes, image.width, image.height, 4, image.pixels.data(),
                                    image.width * 4);
    if (!ok) {
        throw std::runtime_error("PNG encoding failed.");
    }
    return bytes;
}
static void write_encoded(const Image& source, const std::string& path, const std::string& ext, int quality) {
    Image flattened;
    const Image* input = &source;
    if (ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif") {
        flattened.reset(source.width, source.height);
        composite(flattened, source, 0, 0);
        input = &flattened;
    }
    const Image& image = *input;
    int width = image.width, height = image.height;
    const void* pixels = image.pixels.data();
    int ok = 0;
    if (ext == ".png") {
        ok = stbi_write_png(path.c_str(), width, height, 4, pixels, width * 4);
    } else if (ext == ".jpg" || ext == ".jpeg") {
        ok = stbi_write_jpg(path.c_str(), width, height, 4, pixels, quality);
    } else if (ext == ".bmp") {
        ok = stbi_write_bmp(path.c_str(), width, height, 4, pixels);
    } else if (ext == ".tga") {
        ok = stbi_write_tga(path.c_str(), width, height, 4, pixels);
    } else if (ext == ".gif") {
        GifWriter writer{};
        if (!GifBegin(&writer, path.c_str(), width, height, 0)) {
            throw std::runtime_error("Could not create GIF.");
        }
        ok = GifWriteFrame(&writer, reinterpret_cast<const std::uint8_t*>(pixels), width, height, 0, 8, true);
        bool closed = GifEnd(&writer);
        if (!closed) {
            ok = 0;
        }
    } else if (ext == ".webp") {
        std::uint8_t* encoded = nullptr;
        std::size_t size = WebPEncodeLosslessRGBA(reinterpret_cast<const std::uint8_t*>(pixels), width,
                                                  height, width * 4, &encoded);
        if (size == 0) {
            throw std::runtime_error("WebP encoding failed.");
        }
        std::ofstream file(std::filesystem::path(std::u8string(path.begin(), path.end())), std::ios::binary);
        file.write(reinterpret_cast<const char*>(encoded), static_cast<std::streamsize>(size));
        file.close();
        ok = file ? 1 : 0;
        WebPFree(encoded);
    } else if (ext == ".tif" || ext == ".tiff") {
        TIFF* file = open_tiff(path, "w");
        if (!file) {
            throw std::runtime_error("Could not create TIFF.");
        }
        TIFFSetField(file, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField(file, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField(file, TIFFTAG_SAMPLESPERPIXEL, 4);
        TIFFSetField(file, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        TIFFSetField(file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(file, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        std::uint16_t extra = EXTRASAMPLE_UNASSALPHA;
        TIFFSetField(file, TIFFTAG_EXTRASAMPLES, 1, &extra);
        ok = 1;
        for (int row = 0; row < height; ++row) {
            void* scanline = const_cast<Color*>(image.pixels.data() + static_cast<std::size_t>(row) * width);
            if (TIFFWriteScanline(file, scanline, row, 0) < 0) {
                ok = 0;
                break;
            }
        }
        TIFFClose(file);
    } else {
        throw std::runtime_error("Choose PNG, JPEG, BMP, GIF, TIFF, TGA or WebP.");
    }
    if (!ok) {
        throw std::runtime_error("Image could not be written. Check the destination and free disk space.");
    }
}
void save_image(const Image& image, const std::string& path, int quality) {
    // Write alongside the destination, then rename. Failed encoding preserves old artwork.
    static std::atomic<unsigned> sequence{0};
    std::chrono::steady_clock::duration tick = std::chrono::steady_clock::now().time_since_epoch();
    std::string temporary = path + ".rainstar-writing-" + std::to_string(tick.count()) + "-" +
                            std::to_string(sequence.fetch_add(1));
    try {
        write_encoded(image, temporary, extension(path), std::clamp(quality, 1, 100));
        std::filesystem::rename(std::filesystem::path(std::u8string(temporary.begin(), temporary.end())),
                                std::filesystem::path(std::u8string(path.begin(), path.end())));
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(std::filesystem::path(std::u8string(temporary.begin(), temporary.end())),
                                ignored);
        throw;
    }
}
} // namespace paint
