#include "codecs.hpp"
#include "forms/editor.hpp"
#include "localization.hpp"
#include "paths.hpp"
#include "safe_file.hpp"
#include <filesystem>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace paint::forms {
namespace gf = gui_forms;
namespace {
bool variant_replaces_source(const std::string& destination, const std::string& source) {
    if (source.empty()) {
        return false;
    }
    const std::filesystem::path output = std::filesystem::weakly_canonical(path_from_utf8(destination));
    const std::filesystem::path input = std::filesystem::weakly_canonical(path_from_utf8(source));
    if (output == input) {
        return true;
    }
#ifdef _WIN32
    const std::wstring output_name = output.native();
    const std::wstring input_name = input.native();
    if (CompareStringOrdinal(output_name.c_str(), -1, input_name.c_str(), -1, TRUE) == CSTR_EQUAL) {
        return true;
    }
#endif
    return std::filesystem::exists(output) && std::filesystem::exists(input) &&
           std::filesystem::equivalent(output, input);
}
} // namespace
bool Editor::export_icon_variant(const std::vector<std::uint8_t>& bytes, bool cursor,
                                 const std::string& suggested_name, const std::string& source_path) {
    if (bytes.empty()) {
        return false;
    }
    gf::HostSaveFileDialogRequest request;
    request.title = tr("Export…");
    request.suggested_name = suggested_name;
    request.default_extension = cursor ? "cur" : "ico";
    request.filters = {{cursor ? "CUR" : "ICO", {request.default_extension}}};
    request.confirm_overwrite = true;
    const gf::HostPathDialogResult result = std::get<gf::HostPathDialogResult>(dialog(request).payload);
    if (result.outcome != gf::HostDialogOutcome::accepted || result.paths.empty()) {
        return false;
    }
    std::string destination = result.paths.front();
    const std::string extension = cursor ? ".cur" : ".ico";
    if (image_extension(destination).empty()) {
        destination += extension;
    }
    if (image_extension(destination) != extension) {
        throw std::runtime_error(tr("Choose an ICO/CUR filename matching this export."));
    }
    if (variant_replaces_source(destination, source_path) ||
        variant_replaces_source(destination, document.filename)) {
        throw std::runtime_error(tr("Export to a separate file; the source cannot be replaced."));
    }
    const bool exists = std::filesystem::exists(path_from_utf8(destination));
    write_file_atomic(bytes, destination, "Could not write the icon state variant.", exists);
    return true;
}
} // namespace paint::forms
