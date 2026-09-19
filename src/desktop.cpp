#include "desktop.hpp"
#include "codecs.hpp"
#include "conv.hpp"
#include "paths.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace paint {
std::string preference_directory() {
    std::filesystem::path directory;
#if defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home) {
        directory = std::filesystem::path(home) / "Library/Application Support/rainstar/RainstarPaint";
    }
#elif defined(_WIN32)
    const wchar_t* appdata = _wgetenv(L"APPDATA");
    if (appdata) {
        directory = std::filesystem::path(appdata) / L"rainstar" / L"RainstarPaint";
    }
#else
    const char* data = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    if (data && *data) {
        directory = std::filesystem::path(data) / "rainstar/RainstarPaint";
    } else if (home) {
        directory = std::filesystem::path(home) / ".local/share/rainstar/RainstarPaint";
    }
#endif
    if (directory.empty()) {
        throw std::runtime_error("Paint could not find its preferences folder.");
    }
    std::filesystem::create_directories(directory);
    return path_to_utf8(directory) + std::string(1, std::filesystem::path::preferred_separator);
}
void EditorSettings::load() {
    if (storage_path.empty()) {
        return;
    }
    std::ifstream input(path_from_utf8(storage_path));
    std::string magic;
    double distance = 0;
    if (input >> magic >> distance && (magic == "RSPS1" || magic == "RSPS2") && std::isfinite(distance) &&
        distance >= 0.1 && distance <= 100) {
        scroll_distance = distance;
        int background = 0;
        if (magic == "RSPS2" && input >> background) {
            green_felt = background == 1;
        }
    }
}
void EditorSettings::save() const {
    if (storage_path.empty()) {
        return;
    }
    std::ofstream output(path_from_utf8(storage_path), std::ios::trunc);
    output << "RSPS2\n" << scroll_distance << '\n' << (green_felt ? 1 : 0) << '\n';
    output.close();
    if (!output) {
        throw std::runtime_error("Paint could not save its settings file.");
    }
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
