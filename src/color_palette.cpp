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
        colors[slot] = {bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]};
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
