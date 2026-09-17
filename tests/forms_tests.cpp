#include "codecs.hpp"
#include "forms/display.hpp"
#include "forms/editor.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace {
namespace gf = gui_forms;
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
bool white(paint::Color color) {
    return paint::equal(color, {255, 255, 255, 255});
}
struct Fixture {
    std::shared_ptr<paint::forms::Editor> editor;
    std::unique_ptr<gf::Window> window;
    Fixture() {
        editor = gf::make_control<paint::forms::Editor>(gf::StableId("test.editor"));
        (*editor).document.new_image(128, 96);
        (*editor).refresh();
        window = std::make_unique<gf::Window>(editor, gf::Size{1180, 820});
        (*window).perform_layout();
    }
    gf::Point position(double x, double y) {
        gf::RasterCanvas& canvas = (*editor).canvas();
        gui_drawing::PointF origin = canvas.view_origin();
        return canvas.point_to_window({(x - origin.x) * canvas.zoom(), (y - origin.y) * canvas.zoom()});
    }
    bool pointer(gf::PointerAction action, double x, double y,
                 gf::PointerButton button = gf::PointerButton::primary,
                 gf::Modifier modifiers = gf::Modifier::none) {
        gf::PointerEvent event{action, button, position(x, y)};
        event.modifiers = modifiers;
        return (*window).dispatch_pointer(event);
    }
    void drag(double x, double y, double end_x, double end_y) {
        require(pointer(gf::PointerAction::down, x, y), "canvas consumes pointer down");
        pointer(gf::PointerAction::move, end_x, end_y);
        pointer(gf::PointerAction::up, end_x, end_y);
    }
    void click(double x, double y) {
        drag(x, y, x, y);
    }
};
gf::HostCapabilities service_capabilities() {
    gf::HostCapabilities capabilities;
    capabilities.available = gf::HostCapability::lifecycle | gf::HostCapability::pointer_capture |
                             gf::HostCapability::dialogs | gf::HostCapability::clipboard |
                             gf::HostCapability::clipboard_images;
    return capabilities;
}
class TestServices final : public gf::HostServices {
  public:
    TestServices() : HostServices(service_capabilities()) {}
    std::string path;
    gf::HostDialogChoice choice = gf::HostDialogChoice::cancel;
    bool fail_dialogs = false;
    std::vector<std::uint64_t> dialog_ids;
    gf::HostImage clipboard;

  protected:
    gf::HostMonitorResult query_monitors_impl() override {
        return {};
    }
    gf::HostServiceStatus set_cursor_impl(gf::CursorKind) override {
        return {};
    }
    gf::HostServiceStatus set_pointer_capture_impl(bool, std::uint64_t) override {
        return {};
    }
    gf::HostClipboardTextResult read_clipboard_text_impl() override {
        return {};
    }
    gf::HostServiceStatus write_clipboard_text_impl(std::string_view) override {
        return {};
    }
    gf::HostClipboardImageResult read_clipboard_image_impl() override {
        return {{}, clipboard, 1, !clipboard.pixels.empty()};
    }
    gf::HostServiceStatus write_clipboard_image_impl(gf::HostImageView image) override {
        clipboard = {image.width, image.height, image.row_bytes,
                     std::vector<std::byte>(image.pixels.begin(), image.pixels.end())};
        return {};
    }
    gf::HostServiceStatus play_sound_cue_impl(const gf::HostSoundCueRequest&) override {
        return {};
    }
    gf::HostDialogResult show_dialog_impl(const gf::HostDialogRequest& request) override {
        dialog_ids.push_back(request.request_id);
        if (fail_dialogs) {
            return {
                {gf::HostServiceError::backend_failure}, request.request_id, gf::HostMessageDialogResult{}};
        }
        if (std::holds_alternative<gf::HostMessageDialogRequest>(request.payload)) {
            return {{},
                    request.request_id,
                    gf::HostMessageDialogResult{choice == gf::HostDialogChoice::cancel
                                                    ? gf::HostDialogOutcome::cancelled
                                                    : gf::HostDialogOutcome::accepted,
                                                choice}};
        }
        if (std::holds_alternative<gf::HostColorDialogRequest>(request.payload)) {
            return {{},
                    request.request_id,
                    gf::HostColorDialogResult{gf::HostDialogOutcome::accepted, 0x0f284180}};
        }
        return {{}, request.request_id, gf::HostPathDialogResult{gf::HostDialogOutcome::accepted, {path}}};
    }
};
void dialog_clipboard_and_close_contracts() {
    Fixture fixture;
    TestServices services;
    gf::HostSession session(*fixture.window, service_capabilities(), &services);
    require(session.dispatch({1, 0, gf::HostAttachEvent{{1180, 820}, 1}}).accepted(), "test host attaches");
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.image.set(1, 1, {240, 60, 80, 0});
    editor.execute("copy");
    editor.execute("paste");
    require(editor.document.selection.active &&
                paint::equal(editor.document.selection.image.get(1, 1), {240, 60, 80, 0}),
            "clipboard transfers straight-alpha hidden RGB");
    editor.execute("release");
    editor.execute("primary");
    require(paint::equal(editor.document.ink.primary, {15, 40, 65, 128}),
            "typed native color result reaches document");
    services.path = (std::filesystem::temp_directory_path() / "rainstar-forms-dialog-test.png").string();
    require(editor.save(true) && std::filesystem::exists(services.path),
            "save-as dialog supplies a writable path");
    editor.choose_tool(paint::Tool::Pencil);
    fixture.drag(12, 20, 35, 20);
    gf::HostCloseRequest close;
    editor.closing(close);
    require(close.cancel && editor.document.dirty(), "cancel retains unsaved document");
    services.fail_dialogs = true;
    editor.closing(close);
    require(close.cancel && editor.document.dirty(),
            "failed native dialog cannot discard changes or recurse");
    services.fail_dialogs = false;
    services.choice = gf::HostDialogChoice::yes;
    editor.closing(close);
    require(!close.cancel && !editor.document.dirty(), "save choice saves before approving close");
    for (std::size_t index = 0; index < services.dialog_ids.size(); ++index) {
        require(services.dialog_ids[index] != 0 &&
                    (index == 0 || services.dialog_ids[index] > services.dialog_ids[index - 1]),
                "native dialogs receive distinct nonzero request identities");
    }
    std::filesystem::remove(services.path);
}
void display_preserves_document_and_hidden_rgb() {
    Fixture fixture;
    paint::Document& document = (*fixture.editor).document;
    document.image.set(2, 2, {231, 17, 63, 0});
    document.image.set(3, 2, {201, 41, 99, 128});
    (*fixture.editor).refresh();
    gf::RasterCanvas& canvas = (*fixture.editor).canvas();
    gui_drawing::BitmapLockView view = (*canvas.bitmap()).lock(gui_drawing::BitmapLockMode::read);
    const std::byte* pixel = view.data + 2 * view.row_bytes + 3 * 4;
    require(pixel[0] == std::byte{50} && pixel[1] == std::byte{21} && pixel[2] == std::byte{101} &&
                pixel[3] == std::byte{128},
            "rounded premultiplied BGRA presentation");
    (*canvas.bitmap()).unlock(view.token);
    require(paint::equal(document.image.get(2, 2), {231, 17, 63, 0}),
            "hidden straight RGBA survives presentation");
    std::uint64_t generation = (*canvas.bitmap()).generation();
    (*fixture.editor).refresh();
    require((*canvas.bitmap()).generation() == generation,
            "unchanged presentation does not mutate bitmap generation");
}
void captured_stroke_undo_and_right_color() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    require(fixture.pointer(gf::PointerAction::down, 10, 10), "stroke down handled");
    require(editor.canvas().has_pointer_capture(), "stroke captures pointer");
    require(fixture.pointer(gf::PointerAction::move, 1500, 10), "captured move outside control");
    require(fixture.pointer(gf::PointerAction::up, 1500, 10), "captured release outside control");
    require(!editor.canvas().has_pointer_capture(), "stroke releases capture");
    require(!white(editor.document.image.get(100, 10)), "stroke reaches canvas edge");
    require(editor.document.undo_history.size() == 1, "one history transaction per stroke");
    editor.execute("undo");
    require(white(editor.document.image.get(100, 10)), "undo restores prior image");
    editor.execute("redo");
    require(!white(editor.document.image.get(100, 10)), "redo restores stroke");
    editor.document.ink.secondary = {250, 20, 50, 255};
    fixture.pointer(gf::PointerAction::down, 10, 20, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::up, 20, 20, gf::PointerButton::secondary);
    require(paint::equal(editor.document.image.get(15, 20), editor.document.ink.secondary),
            "right stroke uses color two");
}
void material_deposition_does_not_depend_on_event_count() {
    Fixture first;
    Fixture second;
    paint::forms::Editor& a = *first.editor;
    paint::forms::Editor& b = *second.editor;
    a.choose_tool(paint::Tool::Brush);
    b.choose_tool(paint::Tool::Brush);
    a.document.ink.primary = b.document.ink.primary = {200, 30, 60, 100};
    a.document.ink.size = b.document.ink.size = 20;
    first.click(30, 30);
    second.pointer(gf::PointerAction::down, 30, 30);
    for (int index = 0; index < 8; ++index) {
        second.pointer(gf::PointerAction::move, 30, 30);
    }
    second.pointer(gf::PointerAction::up, 30, 30);
    require(paint::equal(a.document.image.get(30, 30), b.document.image.get(30, 30)),
            "same coat does not accumulate from extra events");
}
void retained_curve_save_undo_and_release() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.shape = paint::Shape::Bezier;
    editor.choose_tool(paint::Tool::Shape);
    fixture.drag(10, 20, 100, 20);
    require(editor.document.curve.line_set, "Bezier line established");
    paint::Point handle = editor.document.curve.geometry.handle(0);
    fixture.drag(handle.x, handle.y, handle.x, handle.y + 30);
    require(std::abs(editor.document.curve.geometry.handle(0).y - handle.y - 30) < 1e-9,
            "Bezier handle responds to routed drag");
    editor.execute("undo");
    require(editor.document.curve.line_set &&
                std::abs(editor.document.curve.geometry.handle(0).y - handle.y) < 1e-9,
            "undo keeps live curve session");
    editor.execute("redo");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "rainstar-gui-forms-curve-test.png";
    editor.document.filename = path.string();
    std::uint64_t session = editor.document.curve.session;
    require(editor.save(false), "save live curve");
    paint::Image loaded = paint::load_image(path.string());
    require(loaded.width == 128 && editor.document.curve.session == session,
            "save keeps retained curve session");
    handle = editor.document.curve.geometry.handle(0);
    fixture.drag(handle.x, handle.y, handle.x + 4, handle.y);
    require(editor.document.dirty(), "adjustment after save dirties document");
    editor.execute("release");
    editor.execute("undo");
    require(!editor.document.curve.base, "undo after release does not resurrect handles");
    std::filesystem::remove(path);
}
void selection_move_path_and_stamp() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    fixture.drag(10, 10, 25, 10);
    editor.choose_tool(paint::Tool::Select);
    fixture.drag(8, 8, 28, 15);
    require(editor.document.selection.active && white(editor.document.image.get(15, 10)),
            "selection lifts pixels");
    fixture.drag(15, 10, 45, 30);
    require(editor.document.selection.x == 38 && editor.document.selection.y == 28,
            "selection retains drag offset");
    editor.execute("release");
    require(!white(editor.document.image.get(45, 30)), "placing selection composites at new position");
    editor.execute("undo");
    require(!white(editor.document.image.get(15, 10)), "undo selection restores original object");
    editor.choose_tool(paint::Tool::Path);
    fixture.click(10, 40);
    fixture.click(40, 50);
    fixture.click(70, 40);
    require(editor.document.path.nodes.size() == 3, "path receives three nodes");
    editor.execute("finish-path");
    require(!editor.document.path.extending && editor.document.path.nodes.size() == 3,
            "path run ends but branch nodes remain");
    fixture.click(40, 50);
    fixture.click(80, 70);
    require(editor.document.path.nodes.size() == 5, "old node branches into a new run");
    editor.choose_tool(paint::Tool::Stamp);
    require(editor.document.path.nodes.empty(), "tool change releases path handles");
    std::uint64_t revision = editor.document.revision;
    fixture.click(40, 40);
    require(editor.document.stamp.width == 80 && editor.document.revision == revision,
            "stamp click lifts without painting");
    fixture.click(80, 40);
    require(editor.document.revision != revision, "next stamp click paints");
    fixture.pointer(gf::PointerAction::down, 80, 40, gf::PointerButton::secondary);
    require(editor.document.stamp.pixels.empty(), "right click resets stamp");
}
void zoom_anchors_the_point() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Point position = fixture.position(30, 20);
    gf::Point client = editor.canvas().point_from_window(position);
    gf::PointerEvent event{gf::PointerAction::wheel, gf::PointerButton::none, position};
    event.wheel_delta = {0, 1};
    event.modifiers = gf::Modifier::control;
    static_cast<void>((*fixture.window).dispatch_pointer(event));
    gui_drawing::PointF point = editor.canvas().client_to_bitmap(client);
    require(std::abs(point.x - 30) < 1e-9 && std::abs(point.y - 20) < 1e-9,
            "zoom anchors image point under pointer");
}
} // namespace
int main() {
    try {
        dialog_clipboard_and_close_contracts();
        display_preserves_document_and_hidden_rgb();
        captured_stroke_undo_and_right_color();
        material_deposition_does_not_depend_on_event_count();
        retained_curve_save_undo_and_release();
        selection_move_path_and_stamp();
        zoom_anchors_the_point();
        std::cout << "GUI.Forms: RGBA, routed capture, undo, material strokes, curves/save, selections, "
                     "path, stamp and anchored zoom passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
