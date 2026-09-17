#pragma once
#include "image.hpp"
#include <SDL3/SDL.h>
namespace paint {
enum class WallpaperLayout { Fill, Tile, Center };
struct RecentFiles {
    std::string storage_path;
    std::vector<std::string> paths;
    void load();
    void remember(const std::string& path);
};
std::string preference_directory();
std::string desktop_export(const Image& image, const char* purpose);
Image wallpaper_image(const Image& image, int width, int height, WallpaperLayout layout);
void compose_email(SDL_Window* window, const std::string& path);
void set_wallpaper(const std::string& path);
// Windows returns an acquired file; other desktops open their acquisition app.
bool acquire_picture(std::string& acquired_path);
} // namespace paint
