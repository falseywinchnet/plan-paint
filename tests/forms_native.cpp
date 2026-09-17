#include "forms/editor.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
namespace {
namespace gf = gui_forms;
struct NativeExercise {
    std::shared_ptr<paint::forms::Editor> editor;
    bool keep_open = false;
    bool entered = false;
    void stroke(gf::Window& window, paint::Point first, paint::Point last) {
        gf::RasterCanvas& canvas = (*editor).canvas();
        gui_drawing::PointF origin = canvas.view_origin();
        gf::Point start = canvas.point_to_window(
            {(first.x - origin.x) * canvas.zoom(), (first.y - origin.y) * canvas.zoom()});
        gf::Point end = canvas.point_to_window(
            {(last.x - origin.x) * canvas.zoom(), (last.y - origin.y) * canvas.zoom()});
        if (!window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, start})) {
            throw std::runtime_error("Native canvas did not consume down");
        }
        static_cast<void>(
            window.dispatch_pointer({gf::PointerAction::move, gf::PointerButton::primary, end}));
        static_cast<void>(window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::primary, end}));
        if (canvas.has_pointer_capture()) {
            throw std::runtime_error("Native canvas retained capture after release");
        }
    }
    void ready(gf::Window& window, gf::ApplicationWindowHandle handle) {
        if (!window.host_services()) {
            throw std::runtime_error("Native host services were not attached");
        }
        (*editor).ready(window, handle);
        (*editor).document.new_image(960, 540);
        (*editor).refresh();
        window.perform_layout();
        paint::Document& document = (*editor).document;
        document.ink.primary = {40, 109, 183, 255};
        document.shape = paint::Shape::RoundedRectangle;
        document.shape_fill = true;
        (*editor).choose_tool(paint::Tool::Shape);
        stroke(window, {60, 70}, {300, 260});
        document.ink.primary = {223, 95, 66, 255};
        document.shape = paint::Shape::Star5;
        stroke(window, {365, 65}, {565, 265});
        document.ink.primary = {79, 151, 92, 255};
        document.ink.size = 34;
        (*editor).choose_tool(paint::Tool::Brush);
        stroke(window, {665, 100}, {815, 230});
        document.ink.primary = {96, 54, 149, 255};
        document.ink.size = 7;
        document.shape = paint::Shape::Bezier;
        (*editor).choose_tool(paint::Tool::Shape);
        stroke(window, {110, 395}, {795, 395});
        paint::Point first_handle = document.curve.geometry.handle(0);
        stroke(window, first_handle, {310, 265});
        paint::Point second_handle = document.curve.geometry.handle(1);
        stroke(window, second_handle, {620, 510});
        if (!document.curve.line_set || document.undo_history.size() < 5) {
            throw std::runtime_error("Native editor did not retain curve and history");
        }
        entered = true;
        if (!keep_open && !handle.request_close().accepted()) {
            throw std::runtime_error("Native close request rejected");
        }
    }
};
} // namespace
int main(int argc, char**) {
    try {
        NativeExercise exercise;
        exercise.keep_open = argc > 1;
        exercise.editor = gf::make_control<paint::forms::Editor>(gf::StableId("native.editor"));
        gf::ApplicationWindowOptions options;
        options.title = "Rainstar Paint — GUI.Forms native interaction check";
        options.initial_size = {1280, 820};
        options.minimum_size = {1280, 600};
        options.print_metrics_on_close = false;
        options.ready = std::bind(&NativeExercise::ready, std::ref(exercise), std::placeholders::_1,
                                  std::placeholders::_2);
        gf::ApplicationResult result = gf::Application::run(
            std::make_unique<gf::Window>(exercise.editor, options.initial_size), std::move(options));
        if (result.callback_exception) {
            std::rethrow_exception(result.callback_exception);
        }
        if (!result.accepted() || !exercise.entered) {
            throw std::runtime_error("Native application startup failed");
        }
        std::cout << "Native GUI.Forms Paint: host, input, capture, painting model, curve, history and close "
                     "passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
