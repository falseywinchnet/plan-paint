#include "icon_batch.hpp"
#include "codecs.hpp"
#include "conv.hpp"
#include "paths.hpp"
#include "safe_file.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace paint {
namespace {
bool occupied(const std::filesystem::path& path) {
    return std::filesystem::symlink_status(path).type() != std::filesystem::file_type::not_found;
}
void protect_sources(const IconBatchPlan& plan, const std::filesystem::path& target) {
    for (const IconBatchSource& source : plan.sources) {
        const std::filesystem::path input = path_from_utf8(source.path);
        if (target == input || (std::filesystem::exists(target) && std::filesystem::exists(input) &&
                                std::filesystem::equivalent(target, input))) {
            throw std::runtime_error("An output would replace a batch source. Choose another destination.");
        }
    }
}
void require_rgba(const IconFrame& frame) {
    for (std::size_t pixel = 0; pixel < frame.image.pixels.size(); ++pixel) {
        if (legacy_xor_pixel(frame, pixel)) {
            throw std::runtime_error("Background-dependent XOR cannot be resampled or exported to PNG.");
        }
    }
}
IconFrame fit_image(const IconFrame& source, int size, bool cursor) {
    require_rgba(source);
    const double scale = std::min(double(size) / source.image.width, double(size) / source.image.height);
    const int width = std::clamp(static_cast<int>(std::lround(source.image.width * scale)), 1, size);
    const int height = std::clamp(static_cast<int>(std::lround(source.image.height * scale)), 1, size);
    Image resized;
    if (width == source.image.width && height == source.image.height) {
        resized = source.image;
    } else {
        conv_resize(source.image, width, height, resized);
    }
    IconFrame result;
    result.image.reset(size, size, {0, 0, 0, 0});
    const int left = (size - width) / 2, top = (size - height) / 2;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            result.image.set(left + x, top + y, resized.get(x, y));
        }
    }
    if (cursor) {
        result.hotspot_x = left + std::clamp(static_cast<int>(std::lround(
                                                 (source.hotspot_x + .5) * width / source.image.width - .5)),
                                             0, width - 1);
        result.hotspot_y = top + std::clamp(static_cast<int>(std::lround(
                                                (source.hotspot_y + .5) * height / source.image.height - .5)),
                                            0, height - 1);
    }
    return result;
}
ImageContainer transform(const AssetPreview& source, const IconBatchSettings& settings, std::string& summary,
                         const std::atomic<bool>& cancel) {
    if (settings.operation == IconBatchOperation::Representations) {
        return {};
    }
    if (settings.operation == IconBatchOperation::States) {
        std::vector<int> selected;
        for (std::size_t index = 0; index < source.images.frames.size(); ++index) {
            selected.push_back(static_cast<int>(index));
        }
        IconStateResult states = generate_icon_state(source.images, selected, settings.recipe, &cancel);
        if (!icon_state_ready(states)) {
            throw std::runtime_error("State family is incomplete; no partial icon will be written.");
        }
        summary =
            icon_state_name(settings.recipe.preset) + "; " + std::to_string(selected.size()) + " images";
        return std::move(states.output);
    }
    const bool normalize = settings.operation == IconBatchOperation::Normalize;
    if (normalize && source.images.kind == ContainerKind::Image) {
        throw std::runtime_error("Normalize requires ICO or CUR. Use Convert for ordinary images.");
    }
    const std::vector<IconFrame> empty;
    const IconSizePlan sizes =
        plan_icon_sizes(normalize ? source.images.frames : empty, settings.sizes, settings.sizes_policy);
    const IconFrame& original = source.images.frames[source.preferred];
    ImageContainer output;
    output.kind = normalize ? source.images.kind : ContainerKind::Icon;
    bool upscale = false;
    for (const IconSizeStep& step : sizes.steps) {
        if (cancel.load()) {
            throw std::runtime_error("Cancelled during size generation.");
        }
        if (step.generate) {
            upscale = upscale || step.size > std::max(original.image.width, original.image.height);
            output.frames.push_back(fit_image(original, step.size, output.kind == ContainerKind::Cursor));
        } else {
            output.frames.push_back(source.images.frames[step.existing_index]);
        }
    }
    summary = "Source image #" + std::to_string(source.preferred + 1) + " (" +
              std::to_string(original.image.width) + "x" + std::to_string(original.image.height) + "); " +
              std::to_string(sizes.kept) + " kept, " + std::to_string(sizes.added) + " added, " +
              std::to_string(sizes.replaced) + " replaced, " + std::to_string(sizes.removed) + " removed";
    if (upscale) {
        summary += "; UPSCALING";
    }
    if (original.image.width != original.image.height && sizes.added + sizes.replaced > 0) {
        summary += "; centered transparent padding";
    }
    return output;
}
void choose_destination(IconBatchPlan& plan, IconBatchEntry& entry) {
    const std::filesystem::path path = path_from_utf8(entry.filename);
    const std::string stem = path_to_utf8(path.stem()), extension = path_to_utf8(path.extension());
    std::filesystem::path target = plan.directory / path;
    protect_sources(plan, target);
    if (!occupied(target)) {
        return;
    }
    if (plan.settings.collision == IconExportCollision::Skip) {
        entry.state = IconExportState::Skipped;
        entry.diagnostic = "Destination already exists.";
    } else if (plan.settings.collision == IconExportCollision::Rename) {
        int suffix = 2;
        do {
            if (suffix > 10000) {
                throw std::runtime_error("Too many collisions. Choose another output folder.");
            }
            entry.filename = stem + "-copy" + std::to_string(suffix++) + extension;
            target = plan.directory / path_from_utf8(entry.filename);
        } while (occupied(target));
        protect_sources(plan, target);
    } else {
        if (std::filesystem::symlink_status(target).type() != std::filesystem::file_type::regular) {
            throw std::runtime_error("Replacement requires a regular file, not a folder or link.");
        }
        entry.replaces_existing = true;
    }
}
std::string quote(const std::string& value) {
    std::ostringstream text;
    text << '"';
    for (unsigned char character : value) {
        if (character == '"' || character == '\\') {
            text << '\\' << static_cast<char>(character);
        } else if (character < 32) {
            text << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << unsigned(character);
        } else {
            text << static_cast<char>(character);
        }
    }
    return text.str() + '"';
}
} // namespace
IconBatchPlan plan_icon_batch(const std::vector<std::string>& sources, const std::string& directory,
                              const IconBatchSettings& settings, const std::atomic<bool>& cancel) {
    if (sources.empty() || sources.size() > 256) {
        throw std::runtime_error("Choose 1 to 256 source files.");
    }
    if (directory.empty() || !std::filesystem::is_directory(path_from_utf8(directory))) {
        throw std::runtime_error("Choose an existing output folder.");
    }
    IconBatchPlan plan;
    plan.settings = settings;
    plan.directory = std::filesystem::canonical(path_from_utf8(directory));
    for (const std::string& path : sources) {
        IconBatchSource source;
        source.path = path_to_utf8(std::filesystem::weakly_canonical(path_from_utf8(path)));
        plan.sources.push_back(std::move(source));
    }
    for (std::size_t index = 0; index < plan.sources.size(); ++index) {
        IconBatchEntry entry;
        entry.source = index;
        if (cancel.load()) {
            entry.state = IconExportState::Cancelled;
            entry.diagnostic = "Cancelled before source review.";
            plan.entries.push_back(entry);
            continue;
        }
        try {
            const AssetPreview source = load_asset_preview(plan.sources[index].path);
            plan.sources[index].fingerprint = source.fingerprint;
            plan.sources[index].preferred = source.preferred;
            const std::string prefix = icon_export_stem(source.path) + "-" + std::to_string(index + 1);
            if (settings.operation == IconBatchOperation::Representations) {
                for (std::size_t image = 0; image < source.images.frames.size(); ++image) {
                    IconBatchEntry frame_entry = entry;
                    frame_entry.representation = static_cast<int>(image);
                    frame_entry.filename = prefix + "-image-" + std::to_string(image + 1) + ".png";
                    const IconFrame& frame = source.images.frames[image];
                    try {
                        require_rgba(frame);
                        frame_entry.summary = std::to_string(frame.image.width) + "x" +
                                              std::to_string(frame.image.height) + "; RGBA PNG";
                        choose_destination(plan, frame_entry);
                    } catch (const std::exception& error) {
                        frame_entry.state = IconExportState::Failed;
                        frame_entry.diagnostic = error.what();
                    }
                    plan.entries.push_back(std::move(frame_entry));
                }
            } else {
                const ImageContainer output = transform(source, settings, entry.summary, cancel);
                static_cast<void>(encode_icon_container(output));
                const std::string kind = settings.operation == IconBatchOperation::Convert ? "-icon"
                                         : settings.operation == IconBatchOperation::Normalize
                                             ? "-normalized"
                                             : "-" + icon_state_name(settings.recipe.preset);
                entry.filename = prefix + kind + (output.kind == ContainerKind::Cursor ? ".cur" : ".ico");
                choose_destination(plan, entry);
                plan.entries.push_back(std::move(entry));
            }
            if (plan.entries.size() > 65536) {
                throw std::runtime_error("Batch output limit exceeded.");
            }
        } catch (const std::exception& error) {
            entry.state = cancel.load() ? IconExportState::Cancelled : IconExportState::Failed;
            entry.diagnostic = error.what();
            plan.entries.push_back(std::move(entry));
        }
    }
    return plan;
}
bool icon_batch_ready(const IconBatchPlan& plan) {
    bool pending = false;
    for (const IconBatchEntry& entry : plan.entries) {
        if (entry.state == IconExportState::Failed || entry.state == IconExportState::Cancelled) {
            return false;
        }
        pending = pending || entry.state == IconExportState::Planned;
    }
    return pending;
}
void run_icon_batch(IconBatchPlan& plan, const std::atomic<bool>& cancel, IconBatchProgress progress,
                    void* context) {
    if (!icon_batch_ready(plan)) {
        throw std::runtime_error("Review a complete valid batch before running.");
    }
    AssetPreview loaded;
    std::size_t loaded_index = plan.sources.size();
    for (IconBatchEntry& entry : plan.entries) {
        if (entry.state != IconExportState::Planned) {
            continue;
        }
        entry.state = IconExportState::Running;
        if (progress) {
            progress(plan, context);
        }
        try {
            if (cancel.load()) {
                throw std::runtime_error("Cancelled before this output was published.");
            }
            const IconBatchSource& source = plan.sources.at(entry.source);
            const AssetFingerprint current = asset_fingerprint(source.path);
            if (current.bytes != source.fingerprint.bytes ||
                current.modified != source.fingerprint.modified) {
                throw std::runtime_error("Source changed since review. Review the batch again.");
            }
            if (loaded_index != entry.source) {
                loaded = {};
                loaded = load_asset_preview(source.path);
                loaded_index = entry.source;
                if (loaded.fingerprint.bytes != source.fingerprint.bytes ||
                    loaded.fingerprint.modified != source.fingerprint.modified) {
                    throw std::runtime_error("Source changed while loading. Review the batch again.");
                }
            }
            const std::filesystem::path target = plan.directory / path_from_utf8(entry.filename);
            if (std::filesystem::canonical(plan.directory) != plan.directory) {
                throw std::runtime_error("Output folder changed since review.");
            }
            protect_sources(plan, target);
            if (occupied(target) && plan.settings.collision == IconExportCollision::Skip) {
                entry.state = IconExportState::Skipped;
                entry.diagnostic = "Destination exists at execution; left unchanged.";
            } else {
                if (occupied(target) &&
                    (!entry.replaces_existing ||
                     std::filesystem::symlink_status(target).type() != std::filesystem::file_type::regular)) {
                    throw std::runtime_error("Reviewed output is now occupied or is not a regular file.");
                }
                std::vector<std::uint8_t> bytes;
                if (entry.representation >= 0) {
                    const IconFrame& frame =
                        loaded.images.frames.at(static_cast<std::size_t>(entry.representation));
                    require_rgba(frame);
                    bytes = encode_png(frame.image);
                } else {
                    std::string summary;
                    bytes = encode_icon_container(transform(loaded, plan.settings, summary, cancel));
                }
                if (cancel.load()) {
                    throw std::runtime_error("Cancelled before this output was published.");
                }
                // Recheck input identity after work and before publishing.
                if (!asset_unchanged(loaded)) {
                    throw std::runtime_error("Source changed during processing; output was not written.");
                }
                write_file_atomic(bytes, path_to_utf8(target), "Could not publish batch output.",
                                  entry.replaces_existing);
                entry.state = IconExportState::Completed;
            }
        } catch (const std::exception& error) {
            entry.state = cancel.load() ? IconExportState::Cancelled : IconExportState::Failed;
            entry.diagnostic = error.what();
        }
        if (progress) {
            progress(plan, context);
        }
    }
}
std::string icon_batch_report(const IconBatchPlan& plan) {
    std::ostringstream json;
    json.imbue(std::locale::classic());
    json << std::setprecision(17);
    std::string operation;
    switch (plan.settings.operation) {
    case IconBatchOperation::Convert:
        operation = "convert-to-ico";
        break;
    case IconBatchOperation::Normalize:
        operation = "normalize";
        break;
    case IconBatchOperation::States:
        operation = "state-family";
        break;
    case IconBatchOperation::Representations:
        operation = "representations-to-png";
        break;
    }
    const IconBatchSettings& settings = plan.settings;
    json << "{\n\"schemaVersion\":1,\"operation\":" << quote(operation)
         << ",\"directory\":" << quote(path_to_utf8(plan.directory)) << ",\"collision\":"
         << quote(settings.collision == IconExportCollision::Skip     ? "skip"
                  : settings.collision == IconExportCollision::Rename ? "rename"
                                                                      : "replace")
         << ",\"sizePolicy\":"
         << quote(settings.sizes_policy == IconSizePolicy::AddMissing     ? "add-missing"
                  : settings.sizes_policy == IconSizePolicy::ExactProfile ? "exact-profile"
                                                                          : "replace-selected")
         << ",\"sizes\":[";
    for (std::size_t index = 0; index < settings.sizes.size(); ++index) {
        if (index != 0) {
            json << ',';
        }
        json << settings.sizes[index];
    }
    json << "],\"recipe\":{\"preset\":" << quote(icon_state_name(settings.recipe.preset))
         << ",\"saturationPercent\":" << settings.recipe.saturation
         << ",\"brightnessPercent\":" << settings.recipe.brightness
         << ",\"opacityPercent\":" << settings.recipe.opacity << ",\"palette\":"
         << quote(settings.recipe.palette == IconStatePalette::Rgba ? "rgba" : "indexed-dib")
         << "},\"outputs\":[\n";
    for (std::size_t index = 0; index < plan.entries.size(); ++index) {
        const IconBatchEntry& entry = plan.entries[index];
        json << "{\"source\":" << quote(plan.sources[entry.source].path)
             << ",\"sourceOrdinal\":" << entry.source + 1
             << ",\"reviewedSourceBytes\":" << plan.sources[entry.source].fingerprint.bytes
             << ",\"preferredSourceImageOrdinal\":" << plan.sources[entry.source].preferred + 1
             << ",\"representationOrdinal\":"
             << (entry.representation < 0 ? "null" : std::to_string(entry.representation + 1))
             << ",\"replacesExisting\":" << (entry.replaces_existing ? "true" : "false")
             << ",\"filename\":" << quote(entry.filename) << ",\"summary\":" << quote(entry.summary)
             << ",\"state\":" << quote(icon_export_state_name(entry.state))
             << ",\"diagnostic\":" << quote(entry.diagnostic) << '}'
             << (index + 1 < plan.entries.size() ? ",\n" : "\n");
    }
    return json.str() + "]}\n";
}
} // namespace paint
