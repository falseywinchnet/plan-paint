#include "desktop.hpp"
#include "codecs.hpp"
#include "conv.hpp"
#include "paths.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace paint {
Color office_color(int index) {
    static const std::array<unsigned, 30> colors = {
        0x000000, 0xFFFFFF, 0x1F497D, 0xEEECE1, 0x4F81BD, 0xC0504D, 0x9BBB59, 0x8064A2, 0x4BACC6, 0xF79646,
        0x7F7F7F, 0xF2F2F2, 0xC6D9F1, 0xDDD9C3, 0xDBE5F1, 0xF2DCDB, 0xEBF1DE, 0xE4DFEC, 0xDBEEF3, 0xFDE9D9,
        0xBFBFBF, 0xD9D9D9, 0x8DB3E2, 0xC4BD97, 0xB8CCE4, 0xE5B9B7, 0xD7E3BC, 0xCCC1D9, 0xB7DEE8, 0xFBD5B5};
    const unsigned value = colors.at(static_cast<std::size_t>(index));
    return {static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(value), 255};
}
CustomColors::CustomColors() {
    colors.fill({255, 255, 255, 255});
}
void CustomColors::load() {
    if (storage_path.empty()) {
        return;
    }
    std::ifstream input(path_from_utf8(storage_path), std::ios::binary);
    std::array<unsigned char, 71> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!input || std::string(reinterpret_cast<const char*>(bytes.data()), 5) != "RSPC1") {
        return;
    }
    occupied = static_cast<std::uint16_t>(bytes[5] | (bytes[6] << 8));
    for (std::size_t slot = 0; slot < colors.size(); ++slot) {
        const std::size_t offset = 7 + slot * 4;
        colors[slot] = {bytes[offset], bytes[offset + 1], bytes[offset + 2], 255};
    }
}
void CustomColors::store(int slot, Color color) {
    if (slot < 0 || slot >= static_cast<int>(colors.size())) {
        throw std::out_of_range("Choose a custom-color slot first.");
    }
    colors[static_cast<std::size_t>(slot)] = color;
    occupied |= static_cast<std::uint16_t>(1u << slot);
    if (storage_path.empty()) {
        return;
    }
    std::array<unsigned char, 71> bytes{};
    const std::string magic = "RSPC1";
    std::copy(magic.begin(), magic.end(), bytes.begin());
    bytes[5] = static_cast<unsigned char>(occupied);
    bytes[6] = static_cast<unsigned char>(occupied >> 8);
    for (std::size_t index = 0; index < colors.size(); ++index) {
        const std::size_t offset = 7 + index * 4;
        bytes[offset] = colors[index].r;
        bytes[offset + 1] = colors[index].g;
        bytes[offset + 2] = colors[index].b;
        bytes[offset + 3] = 255;
    }
    std::ofstream output(path_from_utf8(storage_path), std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    output.close();
    if (!output) {
        throw std::runtime_error(
            "The custom color is available now, but its preferences file could not be saved.");
    }
}
std::string preference_directory() {
    char* raw = SDL_GetPrefPath("rainstar", "RainstarPaint");
    if (!raw) {
        throw std::runtime_error("Paint could not open its preferences folder.");
    }
    std::string result(raw);
    SDL_free(raw);
    return result;
}
void RecentFiles::load() {
    paths.clear();
    if (storage_path.empty()) {
        return;
    }
    std::ifstream input(path_from_utf8(storage_path), std::ios::binary);
    std::vector<std::string> loaded;
    // Length-prefixed records preserve spaces, backslashes and embedded newlines.
    for (int index = 0; index < 12 && input; ++index) {
        std::uint32_t length = 0;
        input.read(reinterpret_cast<char*>(&length), sizeof(length));
        if (!input || length == 0 || length > 65536) {
            break;
        }
        std::string path(length, '\0');
        input.read(path.data(), length);
        if (!input || path.find('\0') != std::string::npos) {
            break;
        }
        loaded.push_back(path);
    }
    paths = std::move(loaded);
}
void RecentFiles::remember(const std::string& path) {
    if (path.empty() || path.size() > 65536) {
        return;
    }
    paths.erase(std::remove(paths.begin(), paths.end(), path), paths.end());
    paths.insert(paths.begin(), path);
    if (paths.size() > 12) {
        paths.resize(12);
    }
    if (storage_path.empty()) {
        return;
    }
    std::ofstream output(path_from_utf8(storage_path), std::ios::binary | std::ios::trunc);
    for (const std::string& item : paths) {
        const std::uint32_t length = static_cast<std::uint32_t>(item.size());
        output.write(reinterpret_cast<const char*>(&length), sizeof(length));
        output.write(item.data(), length);
    }
}
std::string desktop_export(const Image& image, const char* purpose) {
    const std::int64_t tick = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
    std::string path = preference_directory() + purpose + "-" + std::to_string(tick) + ".png";
    save_image(image, path);
    return path;
}
Image wallpaper_image(const Image& image, int width, int height, WallpaperLayout layout) {
    if (image.width <= 0 || image.height <= 0) {
        throw std::runtime_error("There is no picture to use as wallpaper.");
    }
    Image result;
    result.reset(width, height);
    if (layout == WallpaperLayout::Fill) {
        int crop_width = image.width;
        int crop_height = image.height;
        if (static_cast<std::int64_t>(image.width) * height >
            static_cast<std::int64_t>(image.height) * width) {
            crop_width = static_cast<int>(std::ceil(static_cast<double>(image.height) * width / height));
        } else {
            crop_height = static_cast<int>(std::ceil(static_cast<double>(image.width) * height / width));
        }
        Image cropped;
        cropped.reset(crop_width, crop_height, {0, 0, 0, 0});
        composite(cropped, image, -(image.width - crop_width) / 2, -(image.height - crop_height) / 2);
        Image scaled;
        conv_resize(cropped, width, height, scaled);
        composite(result, scaled, 0, 0);
    } else if (layout == WallpaperLayout::Center) {
        composite(result, image, (width - image.width) / 2, (height - image.height) / 2);
    } else {
        for (int y = 0; y < height; y += image.height) {
            for (int x = 0; x < width; x += image.width) {
                composite(result, image, x, y);
            }
        }
    }
    return result;
}
} // namespace paint
