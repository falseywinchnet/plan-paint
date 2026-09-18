#include "platform.hpp"
#include <algorithm>
#include <filesystem>
namespace paint {
std::vector<std::string> installed_fonts() {
    std::vector<std::string> result;
#ifdef __APPLE__
    const char* directories[] = {"/System/Library/Fonts/Supplemental", "/Library/Fonts"};
#elif defined(_WIN32)
    const char* directories[] = {"C:/Windows/Fonts"};
#else
    const char* directories[] = {"/usr/share/fonts", "/usr/local/share/fonts"};
#endif
    for (const char* directory : directories) {
        std::error_code error;
        std::filesystem::recursive_directory_iterator cursor(
            directory, std::filesystem::directory_options::skip_permission_denied, error);
        std::filesystem::recursive_directory_iterator end;
        while (!error && cursor != end && result.size() < 1024) {
            std::filesystem::path path = (*cursor).path();
            std::string extension = path.extension().string();
            if ((*cursor).is_regular_file(error) &&
                (extension == ".ttf" || extension == ".TTF" || extension == ".otf")) {
                std::u8string encoded = path.u8string();
                result.emplace_back(encoded.begin(), encoded.end());
            }
            cursor.increment(error);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}
} // namespace paint
