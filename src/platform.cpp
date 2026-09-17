#include "platform.hpp"
#include "codecs.hpp"
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
namespace paint {
static const SDL_DialogFileFilter image_filters[] = {
    {"All supported images", "png;jpg;jpeg;bmp;gif;tif;tiff;tga;webp;psd;pnm;ppm;pgm;hdr;pic"}};
static const SDL_DialogFileFilter save_filters[] = {
    {"PNG image", "png"},           {"JPEG picture", "jpg;jpeg"}, {"Bitmap picture", "bmp"},
    {"GIF picture", "gif"},         {"TIFF picture", "tiff;tif"}, {"Targa image", "tga"},
    {"WebP lossless image", "webp"}};
static const char* save_extensions[] = {".png", ".jpg", ".bmp", ".gif", ".tiff", ".tga", ".webp"};
static void file_dialog_result(void* userdata, const char* const* files, int filter) {
    FileDialog& dialog = *static_cast<FileDialog*>(userdata);
    std::lock_guard<std::mutex> lock(dialog.mutex);
    try {
        if (!files) {
            dialog.result = {FileAction::None, "", SDL_GetError()};
        } else if (files[0]) {
            std::string path = files[0];
            if (dialog.requested == FileAction::Save && std::filesystem::path(path).extension().empty()) {
                path += save_extensions[filter >= 0 && filter < 7 ? filter : 0];
            }
            dialog.result = {dialog.requested, path, ""};
        }
    } catch (...) {
        dialog.result.action = FileAction::None;
    }
    dialog.result.completed = true;
    dialog.pending = false;
}
void FileDialog::show(SDL_Window* window, FileAction action, const std::string& initial) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (pending) {
            return;
        }
        requested = action;
        pending = true;
    }
    if (action == FileAction::Save) {
        SDL_ShowSaveFileDialog(file_dialog_result, this, window, save_filters, 7, initial.c_str());
    } else {
        SDL_ShowOpenFileDialog(file_dialog_result, this, window, image_filters, 1,
                               initial.empty() ? nullptr : initial.c_str(), false);
    }
}
FileResult FileDialog::take() {
    std::lock_guard<std::mutex> lock(mutex);
    FileResult taken = std::move(result);
    result = {};
    return taken;
}
#ifndef __APPLE__
static const void* clipboard_data(void* userdata, const char* type, std::size_t* size) {
    const std::vector<std::uint8_t>& bytes = *static_cast<std::vector<std::uint8_t>*>(userdata);
    if (!type || std::strcmp(type, "image/png") != 0) {
        *size = 0;
        return nullptr;
    }
    *size = bytes.size();
    return bytes.data();
}
static void clipboard_cleanup(void* userdata) {
    delete static_cast<std::vector<std::uint8_t>*>(userdata);
}
void copy_to_clipboard(const Image& image) {
    std::unique_ptr<std::vector<std::uint8_t>> bytes =
        std::make_unique<std::vector<std::uint8_t>>(encode_png(image));
    const char* types[] = {"image/png"};
    if (!SDL_SetClipboardData(clipboard_data, clipboard_cleanup, bytes.get(), types, 1)) {
        throw std::runtime_error(SDL_GetError());
    }
    bytes.release();
}
bool paste_from_clipboard(Image& image) {
    std::size_t size = 0;
    void* bytes = SDL_GetClipboardData("image/png", &size);
    if (!bytes) {
        return false;
    }
    try {
        image = decode_image(bytes, size);
        SDL_free(bytes);
        return true;
    } catch (...) {
        SDL_free(bytes);
        throw;
    }
}
std::string default_font_path(bool mono, bool bold, bool italic) {
#ifdef _WIN32
    std::string face = mono ? "consola" : "arial";
    if (bold && italic) {
        face += mono ? "z" : "bi";
    } else if (bold) {
        face += mono ? "b" : "bd";
    } else if (italic) {
        face += mono ? "i" : "i";
    }
    return "C:/Windows/Fonts/" + face + ".ttf";
#else
    std::string face = mono ? "DejaVuSansMono" : "DejaVuSans";
    if (bold && italic) {
        face += "-BoldOblique";
    } else if (bold) {
        face += "-Bold";
    } else if (italic) {
        face += "-Oblique";
    }
    return "/usr/share/fonts/truetype/dejavu/" + face + ".ttf";
#endif
}
#endif
} // namespace paint
