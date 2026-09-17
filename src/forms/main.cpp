#include "forms/editor.hpp"
#include <functional>
#include <iostream>
int main(int argc, char** argv) {
    try {
        const std::shared_ptr<paint::forms::Editor> editor =
            gui_forms::make_control<paint::forms::Editor>(gui_forms::StableId("paint.editor"));
        if (argc > 1) {
            (*editor).open_file(argv[1]);
        }
        std::unique_ptr<gui_forms::Window> window =
            std::make_unique<gui_forms::Window>(editor, gui_forms::Size{1180, 820});
        gui_forms::ApplicationWindowOptions options;
        options.title = "Rainstar Paint — GUI.Forms port";
        options.initial_size = {1180, 820};
        options.minimum_size = {1040, 560};
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
