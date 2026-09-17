#pragma once
#include <filesystem>
#include <string_view>
namespace paint {
inline std::filesystem::path path_from_utf8(std::string_view path) {
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
}
} // namespace paint
