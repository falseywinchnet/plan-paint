#include "codecs.hpp"
#include "forms/editor.hpp"
#include <algorithm>
#include <filesystem>
#include <functional>
#include <gui_forms/timer.hpp>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <gui_forms/platform/windows_host.hpp>
#endif
namespace {
namespace gf = gui_forms;
struct NativeExercise {
    std::shared_ptr<paint::forms::Editor> editor;
    bool keep_open = false, resize_preview = false, features = false, compact = false, bugs = false,
         materials = false, interface_review = false, view_review = false;
    int backing = 0, hue = 220;
    gf::Window* resize_window = nullptr;
    std::unique_ptr<gf::Timer> resize_timer;
    gf::SubscriptionToken resize_tick;
    void show_resize_preview() {
        (*resize_timer).stop();
        gf::RasterCanvas& canvas = (*editor).canvas();
        gui_drawing::PointF origin = canvas.view_origin();
        gf::Point start =
            canvas.point_to_window({(256 - origin.x) * canvas.zoom(), (120 - origin.y) * canvas.zoom()});
        gf::Point end =
            canvas.point_to_window({(480 - origin.x) * canvas.zoom(), (120 - origin.y) * canvas.zoom()});
        static_cast<void>(
            (*resize_window).dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, start}));
        static_cast<void>(
            (*resize_window).dispatch_pointer({gf::PointerAction::move, gf::PointerButton::primary, end}));
        if (!canvas.has_pointer_capture() || (*editor).document.selection.image.width != 192) {
            throw std::runtime_error(
                "Live resize must hold capture without replacing the source before release");
        }
        std::cout << "Resize preview held before mouse-up: original 192 x 112, displayed 416 x 112\n"
                  << std::flush;
    }

    void feature_gallery(gf::Window& window) {
        paint::Document& document = (*editor).document;
        document.new_image(960, 600);
        document.image.reset(960, 600, {248, 246, 241, 255});
        paint::TextStyle label;
        label.size = 15;
        const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path();
        const char* fonts[] = {"DynaPuff[wdth,wght].ttf", "Bubble Sans 1.01.otf", "Anton-Regular.ttf",
                               "TitanOne-Regular.ttf"};
        const char* names[] = {"DynaPuff", "Bubble Sans", "Anton", "Titan One"};
        for (int index = 0; index < 4; ++index) {
            paint::TextSession sample;
            sample.begin({0, 0});
            sample.resize({0, 0, 220, 110});
            sample.style.face_path = (root / "assets/fonts/poster" / fonts[index]).string();
            sample.style.size = 64;
            sample.style.contour = true;
            sample.style.outline_width = 1;
            sample.replace("BOOM");
            sample.refresh({74, 49, 117, 255}, {244, 183, 63, 255});
            paint::composite(document.image, sample.preview, 10 + index * 235, 25);
            paint::draw_text(document.image, {10.0 + index * 235, 7}, names[index], label, {45, 45, 55, 255},
                             {}, "");
        }
        for (int index = 0; index < paint::brush_count; ++index) {
            const int col = index % 6, row = index / 6;
            paint::Ink ink;
            ink.size = 37;
            ink.primary = {38, 119, 164, 255};
            ink.brush = static_cast<paint::Brush>(index);
            paint::DynamicBrushStroke brush;
            brush.segment(document.image, {20.0 + col * 156, 153.0 + row * 80},
                          {136.0 + col * 156, 159.0 + row * 80}, ink, false);
            paint::draw_text(document.image, {14.0 + col * 156, 177.0 + row * 80}, paint::brush_names[index],
                             label, {45, 45, 55, 255}, {}, "");
        }
        paint::Ink glitter;
        glitter.size = 58;
        glitter.brush = paint::Brush::Airbrush;
        glitter.primary = {192, 131, 34, 255};
        paint::DynamicBrushStroke sparkles;
        sparkles.segment(document.image, {45, 320}, {425, 325}, glitter, true);
        paint::draw_text(document.image, {20, 350}, "Fine glitter spray", label, {45, 45, 55, 255}, {}, "");
        paint::TextSession art;
        art.begin({0, 0});
        art.resize({0, 0, 440, 108});
        art.style.size = 78;
        art.style.face_path = (root / "assets/fonts/poster/TitanOne-Regular.ttf").string();
        art.style.word_art = paint::WordArt::Extruded;
        art.style.skew = -0.18;
        art.style.perspective = 0.28;
        art.style.warp = -0.12;
        art.replace("Rainstar");
        art.refresh({142, 68, 168, 255}, {});
        paint::composite(document.image, art.preview, 465, 285);
        document.ink.primary = {49, 91, 151, 255};
        document.ink.secondary = {244, 183, 63, 255};
        (*editor).choose_tool(paint::Tool::Text);
        (*editor).text.begin({75, 425});
        (*editor).text.resize({75, 425, 780, 135});
        (*editor).text.style.face_path = (root / "assets/fonts/poster/Anton-Regular.ttf").string();
        (*editor).text.style.size = 110;
        (*editor).text.style.contour = true;
        (*editor).text.style.outline_width = 2;
        (*editor).text.style.skew = 0.2;
        (*editor).text.style.perspective = -0.35;
        (*editor).text.style.warp = 0.15;
        (*editor).text.replace("EDITABLE LETTERING");
        (*editor).refresh();
        window.perform_layout();
        entered = true;
    }
    void bug_gallery(gf::Window& window) {
        paint::Document& document = (*editor).document;
        document.new_image(960, 540);
        document.image.reset(960, 540, {248, 246, 241, 255});
        paint::TextStyle label;
        label.size = 18;
        const paint::Brush brushes[] = {paint::Brush::Pencil, paint::Brush::Crayon, paint::Brush::Pastel,
                                        paint::Brush::Charcoal};
        for (int index = 0; index < 4; ++index) {
            paint::Ink ink;
            ink.size = 48;
            ink.primary = {38, 119, 164, 255};
            ink.brush = brushes[index];
            paint::DynamicBrushStroke brush;
            brush.segment(document.image, {35.0 + index * 230, 60}, {200.0 + index * 230, 70}, ink, false);
            paint::draw_text(document.image, {20.0 + index * 230, 102},
                             paint::brush_names[static_cast<int>(brushes[index])], label, {45, 45, 55, 255},
                             {}, "");
        }
        document.ink.brush = paint::Brush::Marker;
        document.ink.pattern = paint::Pattern::Diagonal;
        (*editor).choose_tool(paint::Tool::Pencil);
        (*editor).refresh();
        window.perform_layout();
        stroke(window, {30, 170}, {910, 184});
        paint::draw_text(document.image, {20, 195}, "Home Pencil after Marker + Diagonal material", label,
                         {45, 45, 55, 255}, {}, "");
        paint::draw_text(document.image, {20, 245}, "Selection: two islands and a subtracted hole", label,
                         {45, 45, 55, 255}, {}, "");
        for (int y = 290; y < 490; ++y) {
            for (int x = 745; x < 925; ++x) {
                document.image.set(x, y, {255, 0, 255, 0});
            }
        }
        paint::draw_text(document.image, {740, 500}, "Transparent RGBA", label, {45, 45, 55, 255}, {}, "");
        (*editor).choose_tool(paint::Tool::Lasso);
        document.select({25, 290, 330, 180}, {{25, 310}, {150, 290}, {355, 360}, {295, 470}, {40, 445}});
        document.edit_selection({{460, 305}, {640, 305}, {660, 450}, {470, 465}}, false);
        document.edit_selection({{130, 350}, {240, 345}, {270, 415}, {130, 420}}, true);
        (*editor).refresh();
        window.perform_layout();
        const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path();
        paint::save_image(document.visible_image(), (root / "astra/bugfix-review.png").string());
        entered = true;
    }
    void material_gallery(gf::Window& window) {
        paint::Document& document = (*editor).document;
        document.new_image(960, 600);
        document.image.reset(960, 600, {248, 246, 241, 255});
        paint::TextStyle label;
        label.size = 18;
        for (int row = 0; row < 2; ++row) {
            const paint::Brush medium = row == 0 ? paint::Brush::Crayon : paint::Brush::Charcoal;
            paint::draw_text(document.image, {25, 20.0 + row * 160}, row == 0 ? "Crayon" : "Charcoal", label,
                             {40, 45, 55, 255}, {}, "");
            for (int column = 0; column < 3; ++column) {
                paint::Ink ink;
                ink.brush = medium;
                ink.primary = row == 0 ? paint::Color{174, 54, 30, 255} : paint::Color{34, 39, 45, 255};
                ink.size = 16 + column * 24;
                paint::DynamicBrushStroke brush;
                brush.segment(document.image, {35.0 + column * 310, 90.0 + row * 160},
                              {275.0 + column * 310, 100.0 + row * 160}, ink, false);
                paint::draw_text(document.image, {25.0 + column * 310, 135.0 + row * 160},
                                 std::to_string(ink.size) + " px", label, {40, 45, 55, 255}, {}, "");
            }
        }
        const char* captions[] = {"Pattern + Alt charcoal", "Alt carries body", "Solid"};
        for (int column = 0; column < 3; ++column) {
            document.ink.primary = {24, 91, 131, 255};
            document.ink.secondary = {207, 91, 40, 255};
            document.ink.size = 12;
            paint::select_pattern(document.ink, paint::Pattern::Checker);
            paint::select_brush(document.alt_ink, paint::Brush::Charcoal);
            document.alt_carries_body = column == 1;
            if (column == 1) {
                paint::select_brush(document.ink, paint::Brush::Crayon);
                paint::select_pattern(document.alt_ink, paint::Pattern::Solid);
            } else if (column == 2) {
                paint::select_pattern(document.ink, paint::Pattern::Solid);
            }
            const paint::Ink edge = document.primary_ink(), body = document.body_ink();
            paint::draw_shape(document.image, paint::Shape::RoundedRectangle, {35.0 + column * 310, 365},
                              {275.0 + column * 310, 525}, edge, true, true, body.brush, &body);
            paint::draw_text(document.image, {25.0 + column * 310, 548}, captions[column], label,
                             {40, 45, 55, 255}, {}, "");
        }
        paint::select_brush(document.ink, paint::Brush::Crayon);
        paint::select_pattern(document.alt_ink, paint::Pattern::None);
        document.ink.size = 40;
        document.alt_carries_body = false;
        (*editor).choose_tool(paint::Tool::Brush);
        (*editor).refresh();
        window.perform_layout();
        const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path();
        paint::save_image(document.visible_image(), (root / "astra/material-review.png").string());
        entered = true;
    }
    void interface_gallery(gf::Window& window) {
        (*editor).settings.storage_path.clear();
        (*editor).settings.canvas_backing = static_cast<paint::CanvasBacking>(backing);
        paint::Document& document = (*editor).document;
        document.new_image(640, 360);
        document.image.reset(640, 360, {252, 250, 245, 255});
        paint::TextStyle label;
        label.size = 32;
        paint::draw_text(document.image, {35, 25}, "Rainstar Paint", label, {35, 70, 104, 255}, {}, "");
        const paint::Brush brushes[] = {paint::Brush::Round, paint::Brush::Pastel, paint::Brush::Crayon};
        for (int index = 0; index < 3; ++index) {
            paint::Ink ink;
            ink.size = 35;
            ink.primary = index == 0   ? paint::Color{45, 116, 149, 255}
                          : index == 1 ? paint::Color{166, 100, 63, 255}
                                       : paint::Color{88, 128, 95, 255};
            ink.brush = brushes[index];
            paint::DynamicBrushStroke stroke;
            stroke.segment(document.image, {50, 125.0 + index * 75}, {585, 105.0 + index * 75}, ink, false);
        }
        (*editor).choose_tool(paint::Tool::Pencil);
        (*editor).refresh();
        window.perform_layout();
        const gf::Rect area = (*editor).canvas().client_rectangle();
        (*editor).canvas().set_view(1, {-(area.width - 640) / 2, -(area.height - 360) / 2});
        entered = true;
    }
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
        (*editor).settings.drag_shapes = true; // This fixture exercises held native capture.
        (*editor).document.new_image(960, 540);
        (*editor).refresh();
        window.perform_layout();
        paint::Document& document = (*editor).document;
        if (view_review) {
            document.new_image(480, 300);
            document.ink.primary = {30, 90, 150, 255};
            document.ink.secondary = {221, 160, 50, 255};
            paint::draw_shape(document.image, paint::Shape::Star5, {30, 25}, {160, 150}, document.ink, true,
                              true);
            paint::draw_shape(document.image, paint::Shape::RoundedRectangle, {200, 40}, {410, 160},
                              document.ink, true, false);
            paint::TextStyle style;
            style.size = 28;
            paint::draw_text(document.image, {40, 210}, "Original pixel coordinates", style,
                             {40, 60, 85, 255}, {}, "");
            (*editor).settings.rotate_view = true;
            (*editor).rotate_view(0.42);
            (*editor).execute("fit");
            (*editor).choose_tool(paint::Tool::Pencil);
            window.perform_layout();
            entered = true;
            return;
        }
        if (interface_review) {
            (*editor).settings.interface_hue = hue;
            interface_gallery(window);
            return;
        }
        if (materials) {
            material_gallery(window);
            return;
        }
        if (bugs) {
            bug_gallery(window);
            return;
        }
        if (features) {
            feature_gallery(window);
            return;
        }
        if (resize_preview) {
            paint::Image sample;
            sample.reset(192, 112, {230, 70, 40, 255});
            for (int y = 0; y < sample.height; ++y) {
                for (int x = 0; x < sample.width; ++x) {
                    if ((x / 24) % 2) {
                        sample.set(x, y, {30, 110, 190, 255});
                    }
                }
            }
            document.paste(sample, 64, 64);
            (*editor).refresh();
            resize_window = &window;
            resize_timer = std::make_unique<gf::Timer>(window, std::chrono::milliseconds(1000));
            resize_tick = (*resize_timer)
                              .tick()
                              .subscribe(std::bind(&NativeExercise::show_resize_preview, std::ref(*this)));
            (*resize_timer).start();
            entered = true;
            return;
        }
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
#ifdef _WIN32
// The interactive fixture exposes the existing host automation transport so
// platform checks can capture the exact CPU surface and route real host input.
struct WindowsFixtureReady {
    NativeExercise* exercise;
    gf::Window* window;
    void operator()(std::function<void()>, std::function<void()>,
                    std::function<gf::HostDialogResult(const gf::HostDialogRequest&)>,
                    std::function<gf::HostServiceStatus(const gf::HostTooltipRequest&)>,
                    std::function<void()>, std::function<gf::HostClipboardTextResult()>,
                    std::function<gf::HostServiceStatus(std::string_view)>) const {
        (*exercise).ready(*window, {});
    }
};
#endif
} // namespace
int main(int argc, char** argv) {
    try {
        NativeExercise exercise;
        exercise.keep_open = argc > 1;
        exercise.features = argc > 1 && (std::string(argv[1]) == "--features" ||
                                         std::string(argv[1]) == "--features-compact");
        exercise.interface_review = argc > 1 && (std::string(argv[1]) == "--interface" ||
                                                 std::string(argv[1]) == "--interface-compact");
        exercise.compact = argc > 1 && (std::string(argv[1]) == "--features-compact" ||
                                        std::string(argv[1]) == "--interface-compact");
        if (exercise.interface_review && argc > 2) {
            exercise.backing = std::clamp(std::stoi(argv[2]), 0, paint::canvas_backing_count - 1);
        }
        if (exercise.interface_review && argc > 3) { exercise.hue = std::clamp(std::stoi(argv[3]), 0, 359); }
        exercise.materials = argc > 1 && std::string(argv[1]) == "--materials";
        exercise.view_review = argc > 1 && std::string(argv[1]) == "--view";
        exercise.bugs = argc > 1 && std::string(argv[1]) == "--bugs";
        exercise.resize_preview = argc > 1 && std::string(argv[1]) == "--resize-preview";
        exercise.editor = gf::make_control<paint::forms::Editor>(gf::StableId("native.editor"));
        gf::ApplicationWindowOptions options;
        options.title = "Rainstar Paint — GUI.Forms native interaction check";
        options.initial_size = exercise.compact ? gf::Size{800, 600} : gf::Size{1280, 820};
        options.minimum_size = {800, 520};
        options.print_metrics_on_close = false;
        options.ready = std::bind(&NativeExercise::ready, std::ref(exercise), std::placeholders::_1,
                                  std::placeholders::_2);
#ifdef _WIN32
        if (exercise.keep_open) {
            std::unique_ptr<gf::Window> window =
                std::make_unique<gf::Window>(exercise.editor, options.initial_size);
            gf::host::WindowsHostOptions host_options;
            host_options.title = options.title;
            host_options.initial_size = options.initial_size;
            host_options.minimum_size = options.minimum_size;
            host_options.automation_enabled = true;
            host_options.host_ready = WindowsFixtureReady{&exercise, window.get()};
            return gf::host::run_windows(std::move(window), std::move(host_options));
        }
#endif
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
