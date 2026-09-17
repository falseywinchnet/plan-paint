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
            std::filesystem::path preferences;
#if defined(__APPLE__)
            const char* home = std::getenv("HOME");
            if (home) {
                preferences =
                    std::filesystem::path(home) / "Library/Application Support/rainstar/RainstarPaint";
            }
#elif defined(_WIN32)
            const char* appdata = std::getenv("APPDATA");
            if (appdata) {
                preferences = paint::path_from_utf8(appdata) / "rainstar" / "RainstarPaint";
            }
#else
            const char* config = std::getenv("XDG_DATA_HOME");
            const char* home = std::getenv("HOME");
            if (config) {
                preferences = std::filesystem::path(config) / "rainstar/RainstarPaint";
            } else if (home) {
                preferences = std::filesystem::path(home) / ".local/share/rainstar/RainstarPaint";
            }
#endif
            if (!preferences.empty()) {
                std::filesystem::create_directories(preferences);
                (*editor).custom_colors.storage_path = paint::path_to_utf8(preferences / "custom-colors.bin");
                (*editor).custom_colors.load();
            }
        } catch (const std::exception&) {
            (*editor).custom_colors.storage_path.clear();
        }
        if (argc > 1) {
            (*editor).open_file(argv[1]);
        }
        std::unique_ptr<gui_forms::Window> window =
            std::make_unique<gui_forms::Window>(editor, gui_forms::Size{1280, 820});
        gui_forms::ApplicationWindowOptions options;
        options.title = "Rainstar Paint — GUI.Forms port";
        options.initial_size = {1280, 820};
        options.minimum_size = {1280, 600};
        options.print_metrics_on_close = false;
        options.ready =
            std::bind(&paint::forms::Editor::ready, editor, std::placeholders::_1, std::placeholders::_2);
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
