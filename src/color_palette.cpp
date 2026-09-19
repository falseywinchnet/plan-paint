#include "desktop.hpp"
#include "paths.hpp"
#include <algorithm>
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
const char* theme_name(int column) {
    static const std::array<const char*, 10> names = {"Glue",      "Phosphor", "Camo",   "Royal",
                                                      "Afterglow", "Arcade",   "Lagoon", "Porcelain",
                                                      "Bordeaux",  "Ochre"};
    return names.at(static_cast<std::size_t>(column));
}
Color themed_color(int index) {
    // Each column is a three-color family; rows are primary, secondary, accent.
    static const std::array<unsigned, 30> colors = {
        0x9261b3, 0x000000, 0xeee7cb, 0x3056a1, 0x39205d, 0x191835, 0x153f50, 0xf4eee1, 0x672c40, 0xb87832,
        0xffffff, 0x82b361, 0x356b62, 0xffd27a, 0xff83c8, 0x52e4e0, 0x6bdbc1, 0x355779, 0xe7d6be, 0x292f3d,
        0xb3c76c, 0xb36182, 0xb96955, 0xc76073, 0x71e7e0, 0xf0ec89, 0xf6b886, 0xba644e, 0x9ca77b, 0xe6dec4};
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
    std::array<char, 5> magic_bytes{};
    input.read(magic_bytes.data(), 5);
    const std::string magic(magic_bytes.data(), 5);
    const bool legacy = magic == "RSPC1";
    if (!input || (!legacy && magic != "RSPC2")) {
        return;
    }
    const int count = legacy ? 16 : 30;
    const int mask_size = legacy ? 2 : 4;
    std::vector<unsigned char> bytes(static_cast<std::size_t>(mask_size + count * 4));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input) {
        return;
    }
    std::uint32_t mask = 0;
    for (int index = 0; index < mask_size; ++index) {
        mask |= static_cast<std::uint32_t>(bytes[index]) << (index * 8);
    }
    occupied = mask & 0x3fffffffU;
    for (int slot = 0; slot < count; ++slot) {
        const std::size_t offset = static_cast<std::size_t>(mask_size + slot * 4);
        colors[slot] = {bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]};
    }
}
void CustomColors::store(int slot, Color color) {
    if (slot < 0 || slot >= static_cast<int>(colors.size())) {
        throw std::out_of_range("Choose a custom-color slot first.");
    }
    colors[static_cast<std::size_t>(slot)] = color;
    occupied |= (1u << slot);
    if (storage_path.empty()) {
        return;
    }
    std::array<unsigned char, 129> bytes{};
    const std::string magic = "RSPC2";
    std::copy(magic.begin(), magic.end(), bytes.begin());
    bytes[5] = static_cast<unsigned char>(occupied);
    bytes[6] = static_cast<unsigned char>(occupied >> 8);
    bytes[7] = static_cast<unsigned char>(occupied >> 16);
    bytes[8] = static_cast<unsigned char>(occupied >> 24);
    for (std::size_t index = 0; index < colors.size(); ++index) {
        const std::size_t offset = 9 + index * 4;
        bytes[offset] = colors[index].r;
        bytes[offset + 1] = colors[index].g;
        bytes[offset + 2] = colors[index].b;
        bytes[offset + 3] = colors[index].a;
    }
    std::ofstream output(path_from_utf8(storage_path), std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    output.close();
    if (!output) {
        throw std::runtime_error(
            "The custom color is available now, but its preferences file could not be saved.");
    }
}
} // namespace paint
