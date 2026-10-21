#include "codecs.hpp"
#include "icon_batch.hpp"
#include "paths.hpp"
#include "safe_file.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
struct Directory {
    std::filesystem::path path;
    Directory()
        : path(std::filesystem::temp_directory_path() /
               ("paint-icon-batch-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        require(std::filesystem::create_directory(path), "isolated directory");
    }
    ~Directory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};
void cancel_after_one(const paint::IconBatchPlan& plan, void* context) {
    if (plan.entries.front().state == paint::IconExportState::Completed) {
        (*static_cast<std::atomic<bool>*>(context)).store(true);
    }
}
void batch_contracts() {
    Directory root;
    const std::filesystem::path output = root.path / "out";
    std::filesystem::create_directory(output);
    const std::string folder = paint::path_to_utf8(output);
    const std::string picture = paint::path_to_utf8(root.path / "wide.png");
    paint::Image image;
    image.reset(40, 20, {30, 90, 170, 255});
    paint::save_image(image, picture);
    std::atomic<bool> cancel{false};
    paint::IconBatchSettings settings;
    settings.sizes = {16, 32};
    paint::IconBatchPlan job = paint::plan_icon_batch({picture}, folder, settings, cancel);
    require(paint::icon_batch_ready(job) && std::filesystem::is_empty(output), "review is write-free");
    require(job.entries[0].summary.find("padding") != std::string::npos, "aspect fit disclosed");
    paint::run_icon_batch(job, cancel);
    require(job.entries[0].state == paint::IconExportState::Completed, "conversion completes");
    const std::string icon = paint::path_to_utf8(output / job.entries[0].filename);
    paint::ImageContainer converted = paint::load_container(icon);
    require(converted.frames.size() == 2 && converted.frames[0].image.width == 16 &&
                converted.frames[0].image.get(8, 0).a == 0 && converted.frames[0].image.get(8, 8).a == 255,
            "conversion fits the rectangle without stretching");
    settings.operation = paint::IconBatchOperation::Normalize;
    settings.sizes = {16, 48};
    job = paint::plan_icon_batch({icon}, folder, settings, cancel);
    require(job.entries[0].summary.find("2 kept, 1 added") != std::string::npos, "add mode preserves sizes");
    paint::run_icon_batch(job, cancel);
    require(paint::load_container(paint::path_to_utf8(output / job.entries[0].filename)).frames.size() == 3,
            "normalization adds missing size");
    settings.sizes_policy = paint::IconSizePolicy::ExactProfile;
    settings.collision = paint::IconExportCollision::Rename;
    job = paint::plan_icon_batch({icon}, folder, settings, cancel);
    require(job.entries[0].summary.find("1 removed") != std::string::npos, "exact removals disclosed");
    paint::run_icon_batch(job, cancel);
    require(paint::load_container(paint::path_to_utf8(output / job.entries[0].filename)).frames.size() == 2,
            "exact profile removes unrequested size");
    settings.operation = paint::IconBatchOperation::States;
    job = paint::plan_icon_batch({icon}, folder, settings, cancel);
    paint::run_icon_batch(job, cancel);
    const paint::ImageContainer disabled =
        paint::load_container(paint::path_to_utf8(output / job.entries[0].filename));
    const paint::Color state = disabled.frames[0].image.get(8, 8);
    require(state.r == state.g && state.g == state.b && state.a == 140,
            "state batch applies existing recipe");
    settings.operation = paint::IconBatchOperation::Representations;
    job = paint::plan_icon_batch({icon, icon}, folder, settings, cancel);
    require(job.entries.size() == 4 && job.entries[0].filename != job.entries[2].filename,
            "duplicate inputs and representation ordinals retain distinct names");
    paint::run_icon_batch(job, cancel, cancel_after_one, &cancel);
    require(job.entries[0].state == paint::IconExportState::Completed &&
                job.entries[1].state == paint::IconExportState::Cancelled &&
                !std::filesystem::exists(output / job.entries[1].filename),
            "cancellation preserves completed output only");
    cancel.store(false);
    job = paint::plan_icon_batch({icon}, folder, settings, cancel);
    const std::filesystem::path occupied = output / job.entries[0].filename;
    paint::write_file_atomic({'s', 'a', 'f', 'e'}, paint::path_to_utf8(occupied), "sentinel");
    paint::run_icon_batch(job, cancel);
    require(job.entries[0].state == paint::IconExportState::Failed &&
                paint::read_image_bytes(paint::path_to_utf8(occupied)) ==
                    std::vector<std::uint8_t>({'s', 'a', 'f', 'e'}),
            "late collision cannot replace reviewed renamed destination");
    settings.operation = paint::IconBatchOperation::Convert;
    job = paint::plan_icon_batch({picture}, folder, settings, cancel);
    image.reset(42, 22, {40, 80, 120, 255});
    paint::save_image(image, picture);
    paint::run_icon_batch(job, cancel);
    require(job.entries[0].state == paint::IconExportState::Failed &&
                job.entries[0].diagnostic.find("Source changed") != std::string::npos,
            "stale source fails");
    job = paint::plan_icon_batch({picture, paint::path_to_utf8(root.path / "missing.ico")}, folder, settings,
                                 cancel);
    require(!paint::icon_batch_ready(job) && job.entries.size() == 2 &&
                job.entries[1].state == paint::IconExportState::Failed,
            "missing input stays visible and blocks Run");
    settings.sizes.clear();
    job = paint::plan_icon_batch({picture}, folder, settings, cancel);
    require(!paint::icon_batch_ready(job), "empty target profile rejected");
    settings.sizes = {16};
    settings.collision = paint::IconExportCollision::Replace;
    const std::filesystem::path alias_folder = root.path / "aliases";
    std::filesystem::create_directory(alias_folder);
    job = paint::plan_icon_batch({picture}, paint::path_to_utf8(alias_folder), settings, cancel);
    const std::filesystem::path alias = alias_folder / job.entries[0].filename;
    std::error_code error;
    std::filesystem::create_hard_link(paint::path_from_utf8(picture), alias, error);
    require(!error, "hardlink test setup");
    job = paint::plan_icon_batch({picture}, paint::path_to_utf8(alias_folder), settings, cancel);
    require(!paint::icon_batch_ready(job) && job.entries[0].diagnostic.find("source") != std::string::npos,
            "output alias cannot overwrite input");
    require(paint::icon_batch_report(job).find("\"Failed\"") != std::string::npos, "report retains failure");
}
void cursor_and_report_contracts() {
    Directory root;
    const std::filesystem::path output = root.path / "out";
    std::filesystem::create_directory(output);
    paint::IconFrame frame;
    frame.image.reset(20, 10, {80, 120, 200, 255});
    frame.hotspot_x = 19;
    frame.hotspot_y = 9;
    const std::string source = paint::path_to_utf8(root.path / "cursor.cur");
    paint::save_container({paint::ContainerKind::Cursor, {frame, frame}}, source);
    const paint::ImageContainer original = paint::load_container(source);
    std::atomic<bool> cancel{false};
    paint::IconBatchSettings settings;
    settings.operation = paint::IconBatchOperation::Normalize;
    settings.sizes = {16};
    paint::IconBatchPlan job =
        paint::plan_icon_batch({source}, paint::path_to_utf8(output), settings, cancel);
    paint::run_icon_batch(job, cancel);
    require(job.entries[0].state == paint::IconExportState::Completed, "cursor normalization completes");
    const paint::ImageContainer result =
        paint::load_container(paint::path_to_utf8(output / job.entries[0].filename));
    require(result.kind == paint::ContainerKind::Cursor && result.frames.size() == 3,
            "normalization retains duplicate cursor images");
    require((*result.frames[0].source).payload == (*original.frames[0].source).payload &&
                (*result.frames[1].source).payload == (*original.frames[1].source).payload,
            "untouched duplicate images keep encoded payloads");
    require(result.frames[2].hotspot_x == 15 && result.frames[2].hotspot_y == 11 &&
                result.frames[2].image.get(0, 0).a == 0,
            "cursor hotspot follows aspect fitting and centered padding");
    const std::string report = paint::icon_batch_report(job);
    require(report.find("\"operation\":\"normalize\"") != std::string::npos &&
                report.find("\"sizes\":[16]") != std::string::npos &&
                report.find("\"sizePolicy\":\"add-missing\"") != std::string::npos &&
                report.find("\"opacityPercent\":55") != std::string::npos &&
                report.find("\"representationOrdinal\":null") != std::string::npos,
            "report records the operation, exact profile, recipe and image identity");
}
} // namespace
int main() {
    try {
        batch_contracts();
        cursor_and_report_contracts();
        std::cout << "Icon batch: conversion, normalization, state, PNG, cancellation, stale input and "
                     "collisions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
