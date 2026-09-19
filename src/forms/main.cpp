#include "forms/editor.hpp"
#include "paths.hpp"
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
int main(int argc, char** argv) {
    try {
        const std::shared_ptr<paint::forms::Editor> editor =
            gui_forms::make_control<paint::forms::Editor>(gui_forms::StableId("paint.editor"));
        try {
            std::filesystem::path preferences = paint::path_from_utf8(paint::preference_directory());
            (*editor).custom_colors.storage_path = paint::path_to_utf8(preferences / "custom-colors.bin");
            (*editor).custom_colors.load();
            (*editor).settings.storage_path = paint::path_to_utf8(preferences / "settings.txt");
            (*editor).settings.load();
            (*editor).recent.storage_path = paint::path_to_utf8(preferences / "recent.bin");
            (*editor).recent.load();
        } catch (const std::exception&) {
            (*editor).custom_colors.storage_path.clear();
        }
        const std::string initial_path = argc > 1 ? argv[1] : "";
        std::unique_ptr<gui_forms::Window> window =
            std::make_unique<gui_forms::Window>(editor, gui_forms::Size{1280, 820});
        gui_forms::ApplicationWindowOptions options;
        options.title = "Rainstar Paint";
        options.initial_size = {1280, 820};
        options.minimum_size = {800, 520};
        options.print_metrics_on_close = false;
        options.ready = std::bind(&paint::forms::Editor::ready, editor, std::placeholders::_1,
                                  std::placeholders::_2, initial_path);
        options.closing = std::bind(&paint::forms::Editor::closing, editor, std::placeholders::_1);
        gui_forms::ApplicationResult result =
            gui_forms::Application::run(std::move(window), std::move(options));
        if (result.callback_exception) {
            std::rethrow_exception(result.callback_exception);
        }
        return result.accepted() ? 0 : 1;
    } catch (const std::exception& exception) {
        std::cerr << "Rainstar Paint: " << exception.what() << '\n';
        return 1;
    }
}
