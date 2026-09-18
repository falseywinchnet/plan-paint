#include "codecs.hpp"
#include "forms/display.hpp"
#include "forms/editor.hpp"
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
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
        window = std::make_unique<gf::Window>(editor, gf::Size{1280, 820});
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
void await_background(Fixture& fixture) {
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while ((*fixture.editor).background_busy()) {
        static_cast<void>((*fixture.window).drain_posted_work());
        require(std::chrono::steady_clock::now() < deadline,
                "background renderer finishes through UI dispatcher");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
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
    (*fixture.window).perform_layout();
    std::shared_ptr<gf::NumericUpDown> red =
        std::dynamic_pointer_cast<gf::NumericUpDown>((*fixture.window).find("color-Red"));
    std::shared_ptr<gf::NumericUpDown> green =
        std::dynamic_pointer_cast<gf::NumericUpDown>((*fixture.window).find("color-Green"));
    std::shared_ptr<gf::NumericUpDown> blue =
        std::dynamic_pointer_cast<gf::NumericUpDown>((*fixture.window).find("color-Blue"));
    std::shared_ptr<gf::NumericUpDown> alpha =
        std::dynamic_pointer_cast<gf::NumericUpDown>((*fixture.window).find("color-Alpha"));
    require(red && green && blue && alpha, "retained color dialog exposes editable channels");
    (*red).set_value(15);
    (*green).set_value(40);
    (*blue).set_value(65);
    (*alpha).set_value(128);
    require(!paint::equal(editor.document.ink.primary, {15, 40, 65, 128}),
            "color editing is staged until acceptance");
    std::shared_ptr<gf::Button> accept =
        std::dynamic_pointer_cast<gf::Button>((*fixture.window).find("dialog-ok"));
    require(accept && (*accept).perform_click(), "color dialog accepts through retained command");
    require(paint::equal(editor.document.ink.primary, {15, 40, 65, 128}),
            "accepted RGBA reaches document without alpha loss");
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
    fixture.drag(8, 2, 38, 28);
    require(editor.document.selection.active && white(editor.document.image.get(15, 10)),
            "selection lifts pixels");
    fixture.drag(15, 15, 45, 35);
    require(editor.document.selection.x == 38 && editor.document.selection.y == 22,
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
    await_background(fixture);
    fixture.click(80, 40);
    require(editor.document.revision != revision, "next stamp click paints");
    fixture.pointer(gf::PointerAction::down, 80, 40, gf::PointerButton::secondary);
    require(editor.document.stamp.pixels.empty(), "right click resets stamp");
}
std::shared_ptr<gf::Button> require_button(gf::Window& window, const std::string& id) {
    std::shared_ptr<gf::Button> button = std::dynamic_pointer_cast<gf::Button>(window.find(id));
    require(static_cast<bool>(button), "expected retained button exists");
    return button;
}
void routed_button(gf::Window& window, const std::string& id) {
    std::shared_ptr<gf::Button> button = require_button(window, id);
    window.perform_layout();
    gf::Rect bounds = (*button).absolute_bounds();
    gf::Point point{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
    if (!window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, point})) {
        throw std::runtime_error("button consumes routed pointer down: " + id +
                                 " bounds=" + std::to_string(bounds.x) + "," + std::to_string(bounds.y) +
                                 "," + std::to_string(bounds.width) + "," + std::to_string(bounds.height));
    }
    require(window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::primary, point}),
            "button consumes routed pointer up");
}
void ribbon_galleries_and_modal_transactions() {
    Fixture fixture;
    gf::Window& window = *fixture.window;
    paint::forms::Editor& editor = *fixture.editor;
    routed_button(window, "brush-menu");
    window.perform_layout();
    require(window.find("ribbon-popup") != nullptr, "brush gallery opens as retained popup");
    require((*require_button(window, "popup-brush-4")).image_list() != nullptr,
            "brush gallery uses rendered artwork previews");
    routed_button(window, "popup-brush-4");
    require(editor.document.tool == paint::Tool::Brush && editor.document.ink.brush == paint::Brush::Oil &&
                !window.find("ribbon-popup"),
            "gallery selection changes brush and closes popup");
    routed_button(window, "fill-menu");
    window.perform_layout();
    routed_button(window, "popup-pattern-12");
    require(editor.document.ink.pattern == paint::Pattern::Checker,
            "pattern gallery changes the actual ink pattern");
    editor.choose_shape(paint::Shape::Bezier);
    fixture.drag(12, 20, 85, 70);
    require(editor.document.curve.line_set, "curve is live before shape switch");
    routed_button(window, "shape-3");
    require(!editor.document.curve.line_set && editor.document.shape == paint::Shape::Rectangle,
            "shape switch commits prior editable curve");
    routed_button(window, "size-menu");
    window.perform_layout();
    require(window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::escape}),
            "Escape is consumed by gallery");
    require(!window.find("ribbon-popup"), "Escape dismisses gallery");
    const std::filesystem::path palette_path =
        std::filesystem::temp_directory_path() / "rainstar-forms-palette-test.bin";
    paint::CustomColors palette;
    palette.storage_path = palette_path.string();
    palette.store(3, {19, 53, 107, 128});
    paint::CustomColors reloaded;
    reloaded.storage_path = palette_path.string();
    reloaded.load();
    require(paint::equal(reloaded.colors[3], {19, 53, 107, 128}),
            "custom colors preserve RGBA through preferences");
    std::filesystem::remove(palette_path);
    paint::Color before = editor.document.ink.primary;
    editor.execute("primary");
    window.perform_layout();
    gf::Control::Ptr modal = window.find("editor-dialog");
    require(modal && (*modal).absolute_bounds().width == window.client_size().width &&
                (*modal).absolute_bounds().height == window.client_size().height,
            "modal has real full-client geometry");
    require(!window.request_focus(window.find("canvas")), "modal focus scope rejects the background canvas");
    std::size_t undo_count = editor.document.undo_history.size();
    fixture.drag(5, 5, 20, 5);
    require(editor.document.undo_history.size() == undo_count, "modal blocks background drawing");
    std::shared_ptr<gf::TextBox> hex = std::dynamic_pointer_cast<gf::TextBox>(window.find("color-hex"));
    (*hex).set_text("invalid");
    routed_button(window, "dialog-ok");
    require(window.find("editor-dialog") != nullptr, "invalid hex keeps editor open");
    (*hex).set_text("#A02060");
    routed_button(window, "dialog-cancel");
    require(paint::equal(editor.document.ink.primary, before) && !window.find("editor-dialog"),
            "Cancel discards staged color");
    editor.execute("resize");
    window.perform_layout();
    std::shared_ptr<gf::NumericUpDown> width =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("resize-width"));
    std::shared_ptr<gf::NumericUpDown> height =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("resize-height"));
    (*width).set_value(64);
    require((*height).value() == 48, "resize keeps original aspect ratio");
    std::shared_ptr<gf::CheckBox> scale =
        std::dynamic_pointer_cast<gf::CheckBox>(window.find("resize-scale"));
    (*scale).set_checked(false);
    routed_button(window, "dialog-ok");
    require(editor.document.image.width == 64 && editor.document.image.height == 48,
            "Resize commits accepted dimensions");
    editor.execute("undo");
    require(editor.document.image.width == 128 && editor.document.image.height == 96, "Resize is undoable");
}
void ribbon_tabs_status_and_context() {
    Fixture fixture;
    gf::Window& window = *fixture.window;
    paint::forms::Editor& editor = *fixture.editor;
    routed_button(window, "view-tab");
    require(!(*window.find("paste")).visible() && (*window.find("show-rulers")).visible(),
            "View replaces Home controls");
    editor.execute("show-rulers");
    window.perform_layout();
    require(editor.canvas().absolute_bounds().x == 20 && editor.canvas().absolute_bounds().y == 163,
            "rulers reserve space around canvas");
    editor.execute("show-status");
    window.perform_layout();
    require(!(*window.find("zoom-slider")).visible(), "status toggle hides all footer controls");
    editor.execute("show-status");
    window.perform_layout();
    routed_button(window, "status-zoom-in");
    require(editor.canvas().zoom() == 2, "status plus doubles zoom");
    std::shared_ptr<gf::TrackBar> slider =
        std::dynamic_pointer_cast<gf::TrackBar>(window.find("zoom-slider"));
    (*slider).set_value(2);
    require(editor.canvas().zoom() == 4, "status slider controls logarithmic zoom");
    editor.canvas().set_view(4, {17, 9});
    gf::Point position = fixture.position(31, 22);
    static_cast<void>(window.dispatch_pointer({gf::PointerAction::move, gf::PointerButton::none, position}));
    std::shared_ptr<gf::Label> cursor = std::dynamic_pointer_cast<gf::Label>(window.find("cursor-status"));
    require((*cursor).text() == "X: 31   Y: 22 px", "coordinates reflect image location after pan and zoom");
    routed_button(window, "status-zoom-reset");
    require(editor.canvas().zoom() == 1, "percentage button resets actual size");
    routed_button(window, "patterns-tab");
    require((*window.find("grain-scale")).visible() && !window.find("ribbon-popup"),
            "patterns live on a ribbon page");
    routed_button(window, "r-pattern-12");
    require(editor.document.ink.pattern == paint::Pattern::Checker, "ribbon swatch selects ink pattern");
    std::shared_ptr<gf::NumericUpDown> grain =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("grain-scale"));
    (*grain).set_value(1.75);
    require(editor.document.ink.grain_scale == 1.75, "material settings update drawing ink");
    editor.choose_tool(paint::Tool::Stamp);
    routed_button(window, "tool-tab");
    require((*window.find("stamp-shape-0")).visible() && !(*window.find("grain-scale")).visible(),
            "stamp context hides unrelated material settings");
    routed_button(window, "stamp-shape-1");
    require(editor.document.stamp_shape == paint::StampShape::Pill, "context changes stamp capture shape");
    editor.choose_tool(paint::Tool::Brush);
    require(!(*window.find("stamp-shape-0")).visible() && (*window.find("grain-scale")).visible(),
            "context swaps when active tool changes");
    routed_button(window, "home-tab");
    routed_button(window, "shapes-menu");
    window.perform_layout();
    require((*require_button(window, "popup-shape-3")).accessible_name() == "Rectangle",
            "expanded shapes expose distinct names");
    gf::Rect rectangle = (*window.find("popup-shape-3")).absolute_bounds();
    static_cast<void>(window.dispatch_pointer(
        {gf::PointerAction::move, gf::PointerButton::none, {rectangle.x + 12, rectangle.y + 12}}));
    require(window.next_wake().has_value(), "shape hover arms tooltip deadline");
    static_cast<void>(window.poll_frame_schedule(*window.next_wake()));
    window.perform_layout();
    require(window.semantic_snapshot().to_json().find("\"role\":\"tool_tip\"") != std::string::npos,
            "expanded shape hover displays an accessible tooltip");
    require(window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::escape}),
            "Escape closes shape gallery");
    require(!window.find("ribbon-popup") &&
                window.semantic_snapshot().to_json().find("\"role\":\"tool_tip\"") == std::string::npos,
            "closing shapes removes tooltip overlays");
    routed_button(window, "shapes-menu");
    window.perform_layout();
    require(window.find("popup-shape-3") != nullptr, "shape gallery can reopen after tooltip cleanup");
}
void canvas_text_editing_and_commit() {
    Fixture fixture;
    gf::Window& window = *fixture.window;
    paint::forms::Editor& editor = *fixture.editor;
    routed_button(window, "text");
    require(editor.document.tool == paint::Tool::Text && (*window.find("text-font")).visible(),
            "Text opens its formatting ribbon");
    fixture.click(12, 14);
    require(editor.text.active && editor.canvas().semantic_descriptor().role == gf::SemanticRole::text_box,
            "canvas text starts as accessible editing session");
    require(window.dispatch_text({"Aé\nPaint"}), "native text input reaches canvas text");
    require(editor.text.edit.content == "Aé\nPaint", "UTF-8 and multiline text retained");
    require(!editor.document.dirty(), "text preview does not mutate document");
    require(window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::z, gf::Modifier::control}),
            "text owns undo");
    require(editor.text.edit.content.empty(), "text undo restores content independently");
    require(window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::y, gf::Modifier::control}),
            "text owns redo");
    require(editor.text.edit.content == "Aé\nPaint", "text redo restores UTF-8");
    editor.text.edit.caret = editor.text.edit.anchor = 3;
    require(window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::backspace}), "text backspace routed");
    require(editor.text.edit.content == "A\nPaint", "backspace removes whole UTF-8 character");
    editor.execute("undo");
    editor.text.style.bold = true;
    editor.text.style.opaque = true;
    editor.text.resize({12, 14, 92, 74});
    editor.refresh();
    paint::Image expected = editor.document.image;
    paint::composite(expected, editor.text.preview, 12, 14);
    routed_button(window, "text-place");
    require(!editor.text.active && std::memcmp(editor.document.image.pixels.data(), expected.pixels.data(),
                                               expected.pixels.size() * sizeof(paint::Color)) == 0,
            "placed text exactly matches raster preview");
    editor.execute("undo");
    require(white(editor.document.image.get(20, 20)), "placed text is one document undo");
    fixture.click(8, 10);
    require(window.dispatch_text({"Cancel me"}), "second text session receives input");
    editor.execute("text-cancel");
    require(!editor.text.active && !editor.document.dirty(), "Cancel discards text without changing picture");
}
void skew_transaction_and_undo() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.new_image(16, 12);
    paint::Image original = editor.document.image;
    editor.request_skew(16, 12, true, 20, 0);
    await_background(fixture);
    require(!editor.warp_active() && !editor.document.selection.active && editor.document.image.width > 16,
            "asynchronous skew expands canvas and ends transaction");
    editor.execute("undo");
    require(editor.document.image.width == 16 && editor.document.image.height == 12 &&
                std::memcmp(original.pixels.data(), editor.document.image.pixels.data(),
                            original.pixels.size() * sizeof(paint::Color)) == 0,
            "skew undo restores exact original canvas");
    editor.execute("select-all");
    editor.request_skew(16, 12, false, -20, 10);
    editor.cancel_warp();
    await_background(fixture);
    require(editor.document.selection.image.width == 16 && editor.document.selection.image.height == 12,
            "canceled skew cannot publish a late result");
    bool rejected = false;
    try {
        editor.request_skew(16, 12, true, 45, 45);
    } catch (const std::exception&) {
        rejected = true;
    }
    require(rejected && !editor.warp_active(), "singular skew fails before starting a transaction");
}
void background_rotation_mesh_and_stamp() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.new_image(24, 20);
    editor.document.ink.primary = {200, 20, 40, 255};
    editor.choose_tool(paint::Tool::Pencil);
    fixture.drag(5, 5, 12, 9);
    paint::Image original = editor.document.image;
    editor.execute("select-all");
    editor.request_rotation(27);
    await_background(fixture);
    require(!editor.warp_active() && editor.document.selection.active,
            "rotation completes as a floating selection");
    paint::ConvWarpField field;
    field.compile(original);
    double radians = 27 * std::acos(-1.0) / 180;
    double cosine = std::cos(radians), sine = std::sin(radians);
    double x = (original.width - 1) * 0.5, y = (original.height - 1) * 0.5;
    paint::AffineMap map{cosine, -sine, x - cosine * x + sine * y, sine, cosine, y - sine * x - cosine * y};
    paint::Rect bounds = paint::affine_bounds(field, map);
    map.tx -= bounds.x;
    map.ty -= bounds.y;
    paint::Image expected;
    paint::render_affine(field, map, bounds.w, bounds.h, expected);
    require(editor.document.selection.image.pixels.size() == expected.pixels.size() &&
                std::memcmp(editor.document.selection.image.pixels.data(), expected.pixels.data(),
                            expected.pixels.size() * sizeof(paint::Color)) == 0,
            "GUI.Forms free rotation uses authoritative CONV result bytes");
    editor.execute("undo");
    require(editor.document.image.pixels.size() == original.pixels.size() &&
                std::memcmp(editor.document.image.pixels.data(), original.pixels.data(),
                            original.pixels.size() * sizeof(paint::Color)) == 0,
            "rotation undo returns original document");
    editor.execute("select-all");
    editor.start_reshape();
    fixture.drag(-0.5, -0.5, -3, -2);
    require(editor.warp_active(), "mesh remains editable after node release");
    editor.cancel_warp();
    await_background(fixture);
    require(editor.document.selection.image.width == original.width &&
                std::memcmp(editor.document.selection.image.pixels.data(), original.pixels.data(),
                            original.pixels.size() * sizeof(paint::Color)) == 0,
            "mesh cancel restores original and rejects late render");
    editor.start_reshape();
    fixture.drag(-0.5, -0.5, -3, -2);
    editor.finish_warp(true);
    require(!editor.warp_active() && !editor.document.selection.active,
            "mesh commits and releases its controls");
    editor.execute("undo");
    require(std::memcmp(editor.document.image.pixels.data(), original.pixels.data(),
                        original.pixels.size() * sizeof(paint::Color)) == 0,
            "mesh undo restores original");
    editor.choose_tool(paint::Tool::Stamp);
    editor.stamp_width = 8;
    editor.stamp_height = 6;
    fixture.click(8, 8);
    editor.stamp_scale = 0.5;
    editor.stamp_angle = 32;
    editor.regenerate_stamp();
    editor.reset_stamp();
    await_background(fixture);
    require(editor.document.stamp.pixels.empty(), "stamp reset rejects pending compilation and preview");
    fixture.click(8, 8);
    await_background(fixture);
    editor.stamp_angle = 90;
    editor.stamp_scale = 0.5;
    editor.regenerate_stamp();
    await_background(fixture);
    std::uint64_t revision = editor.document.revision;
    fixture.click(16, 12);
    require(editor.document.revision != revision, "transformed stamp places after asynchronous preview");
    // Destruction must join without a callback retaining the last owner on its worker.
    {
        Fixture closing;
        (*closing.editor).document.new_image(10, 10);
        (*closing.editor).request_rotation(13);
    }
}
void atlas_grid_frames_and_cursor_save() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.execute("atlas-grid");
    window.perform_layout();
    std::shared_ptr<gf::NumericUpDown> columns =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("atlas-0"));
    std::shared_ptr<gf::NumericUpDown> rows =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("atlas-1"));
    require(columns && rows, "atlas grid has native numeric controls");
    (*columns).set_value(3);
    (*rows).set_value(2);
    routed_button(window, "dialog-cancel");
    require(editor.document.atlas.kind == paint::AtlasKind::None, "grid cancel preserves document");
    editor.execute("atlas-grid");
    window.perform_layout();
    columns = std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("atlas-0"));
    rows = std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("atlas-1"));
    (*columns).set_value(4);
    (*rows).set_value(2);
    routed_button(window, "dialog-ok");
    require(editor.document.atlas.count() == 8 && editor.document.image.width == 32 &&
                editor.document.image.height == 48,
            "grid splits document into editable frames");
    editor.choose_tool(paint::Tool::Pencil);
    fixture.drag(4, 5, 12, 5);
    paint::Image painted = editor.document.image;
    routed_button(window, "atlas-frame-1");
    require(editor.document.atlas.active == 1 && white(editor.document.image.get(4, 5)),
            "thumbnail switches to distinct untouched frame");
    editor.select_frame(0);
    require(std::equal(editor.document.image.pixels.begin(), editor.document.image.pixels.end(),
                       painted.pixels.begin(), paint::equal),
            "switching frames retains finished artwork");
    editor.select_frame(3, true);
    require(editor.document.atlas.sequence == std::vector<int>({0, 3}),
            "control frame selection retains sequence");
    editor.execute("atlas-next");
    require(editor.document.atlas.active == 0 && editor.document.atlas.sequence.size() == 2,
            "next loops selected sequence");
    editor.execute("atlas-previous");
    require(editor.document.atlas.active == 3, "previous loops selected sequence");
    editor.execute("atlas-gallery");
    window.perform_layout();
    routed_button(window, "gallery-frame-0");
    require(editor.document.atlas.active == 0, "expanded gallery routes frame selection");
    routed_button(window, "dialog-ok");
    editor.execute("atlas-whole");
    require(editor.document.image.width == 128 && !white(editor.document.image.get(4, 5)),
            "whole sheet includes frame edits");
    editor.execute("atlas-leave");
    require(editor.document.atlas.kind == paint::AtlasKind::None && editor.document.image.width == 128,
            "leaving atlas restores edited sheet");
    editor.execute("undo");
    require(editor.document.atlas.kind == paint::AtlasKind::Sheet, "undo restores atlas structure");
    editor.execute("atlas-leave");
    TestServices services;
    gf::HostSession session(window, service_capabilities(), &services);
    require(session.dispatch({1, 0, gf::HostAttachEvent{{1280, 820}, 1}}).accepted(),
            "atlas test host attaches");
    services.path = (std::filesystem::temp_directory_path() / "rainstar-forms-atlas.cur").string();
    require(!editor.save(true) && window.find("icon-size-16"), "cursor save waits for explicit frame sizes");
    routed_button(window, "dialog-ok");
    require(editor.document.atlas.kind == paint::AtlasKind::Cursor && editor.document.atlas.count() == 4 &&
                !editor.document.dirty(),
            "size acceptance creates and saves all cursor frames");
    editor.execute("atlas-hotspot");
    window.perform_layout();
    routed_button(window, "hotspot-pick");
    fixture.click(13, 17);
    require(!editor.pick_hotspot &&
                editor.document.atlas.icons[editor.document.atlas.active].hotspot_x == 13 &&
                editor.document.atlas.icons[editor.document.atlas.active].hotspot_y == 17,
            "canvas picks current frame hotspot");
    require(editor.save(false), "cursor frames save without repeated size dialog");
    paint::ImageContainer saved = paint::load_container(services.path);
    require(saved.frames.size() == 4 && saved.frames.back().hotspot_x == 13 &&
                saved.frames.back().hotspot_y == 17,
            "CUR reload preserves all frames and hotspot");
    std::filesystem::remove(services.path);
}
void atlas_large_sheet_uses_visible_thumbnail_resources() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    paint::AtlasGrid grid;
    grid.columns = 64;
    grid.rows = 64;
    editor.document.configure_atlas(grid);
    routed_button(window, "atlas-tab");
    window.perform_layout();
    require(window.image_resource_snapshot().resource_count < 400,
            "4096-frame atlas retains images only for visible thumbnails");
    editor.select_frame(4095);
    window.perform_layout();
    require(window.find("atlas-frame-4095") && window.image_resource_snapshot().resource_count < 400,
            "scrolling to last frame keeps image resources bounded");
    std::uint64_t before_gallery = window.image_resource_snapshot().resource_count;
    editor.execute("atlas-gallery");
    window.perform_layout();
    require(window.find("gallery-frame-4095") && window.image_resource_snapshot().resource_count < 500,
            "large gallery retains only visible thumbnail images");
    gf::Rect selected = (*window.find("gallery-frame-4095")).absolute_bounds();
    gf::Rect gallery_bounds = (*window.find("gallery-frames")).absolute_bounds();
    require(selected.y >= gallery_bounds.y && selected.bottom() <= gallery_bounds.bottom(),
            "opening gallery reveals current frame after layout");
    routed_button(window, "dialog-ok");
    require(window.image_resource_snapshot().resource_count == before_gallery,
            "closing gallery releases its thumbnail resources");
}
void desktop_transactions_drop_and_handles() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    TestServices services;
    gf::HostSession session(window, service_capabilities(), &services);
    require(session.dispatch({1, 0, gf::HostAttachEvent{{1280, 820}, 1}}).accepted(),
            "desktop test host attaches");
    editor.document.image.set(4, 4, {230, 170, 70, 129});
    paint::Image original = editor.document.image;
    editor.execute("properties");
    window.perform_layout();
    std::shared_ptr<gf::CheckBox> monochrome =
        std::dynamic_pointer_cast<gf::CheckBox>(window.find("properties-monochrome"));
    require(static_cast<bool>(monochrome), "properties exposes monochrome conversion");
    (*monochrome).set_checked(true);
    routed_button(window, "dialog-cancel");
    require(paint::equal(editor.document.image.get(4, 4), original.get(4, 4)),
            "properties cancel preserves image");
    editor.execute("properties");
    window.perform_layout();
    monochrome = std::dynamic_pointer_cast<gf::CheckBox>(window.find("properties-monochrome"));
    (*monochrome).set_checked(true);
    routed_button(window, "dialog-ok");
    require(paint::equal(editor.document.image.get(4, 4), {255, 255, 255, 129}),
            "monochrome conversion preserves alpha");
    editor.execute("undo");
    require(paint::equal(editor.document.image.get(4, 4), original.get(4, 4)),
            "properties transaction can be undone");
    editor.execute("print-preview");
    window.perform_layout();
    require(window.find("print-preview-image") != nullptr, "print preview contains actual document bitmap");
    routed_button(window, "dialog-ok");
    editor.document.image.set(4, 4, {230, 170, 70, 255});
    original = editor.document.image;
    fixture.drag(128, 96, 151, 115);
    require(editor.document.image.width == 151 && editor.document.image.height == 115 &&
                paint::equal(editor.document.image.get(4, 4), original.get(4, 4)),
            "canvas handle expands boundary without scaling artwork");
    editor.execute("undo");
    require(editor.document.image.width == 128 && editor.document.image.height == 96,
            "canvas handle resize is undoable");
    editor.document.select({10, 10, 30, 20});
    editor.refresh();
    fixture.drag(40, 30, 60, 45);
    require(editor.document.selection.image.width == 50 && editor.document.selection.image.height == 35,
            "selection corner resizes floating artwork");
    editor.execute("release");
    paint::Image drop_image;
    drop_image.reset(37, 29, {70, 110, 190, 255});
    std::string path = (std::filesystem::temp_directory_path() / "rainstar-forms-drop.png").string();
    paint::save_image(drop_image, path);
    gf::DragEvent drag;
    drag.action = gf::DragAction::enter;
    drag.session_id = 123;
    drag.position = fixture.position(20, 20);
    drag.allowed_effects = gf::DragEffect::copy;
    drag.items = {gf::DragFileListData{{path}}};
    require(window.dispatch_drag(drag).accepted_effect == gf::DragEffect::copy,
            "file drop advertises copy acceptance");
    drag.action = gf::DragAction::drop;
    require(window.dispatch_drag(drag).accepted_effect == gf::DragEffect::none &&
                editor.document.image.width == 128,
            "unsaved-work cancellation rejects dropped file");
    services.choice = gf::HostDialogChoice::no;
    drag.action = gf::DragAction::enter;
    ++drag.session_id;
    static_cast<void>(window.dispatch_drag(drag));
    drag.action = gf::DragAction::drop;
    require(window.dispatch_drag(drag).accepted_effect == gf::DragEffect::copy &&
                editor.document.image.width == 37 && editor.recent.paths.front() == path,
            "accepted drop loads image and remembers path");
    editor.document.new_image(33, 25);
    editor.document.checkpoint();
    editor.refresh();
    services.path = (std::filesystem::temp_directory_path() / "rainstar-forms-deferred.ico").string();
    services.choice = gf::HostDialogChoice::yes;
    editor.execute("new");
    require(window.find("icon-size-16") && editor.deferred_command == "new",
            "new waits while icon save sizes are chosen");
    routed_button(window, "dialog-ok");
    require(editor.document.filename.empty() && editor.document.atlas.kind == paint::AtlasKind::None &&
                !editor.document.dirty(),
            "successful icon save resumes New request");
    paint::ImageContainer saved = paint::load_container(services.path);
    require(saved.frames.size() == 4, "deferred save writes chosen icon sizes before replacing image");
    std::filesystem::remove(path);
    std::filesystem::remove(services.path);
}
class PreviewPainter final : public gf::Painter {
  public:
    int pencil_pixels = 0, eraser_discs = 0;
    void save() override {}
    void restore() override {}
    void translate(gf::Point) override {}
    void clip_rect(gf::Rect) override {}
    void fill_rect(gf::Rect rectangle, gf::Color color) override {
        if (color.red == 20 && color.green == 70 && color.blue == 110 && rectangle.width == 8 &&
            rectangle.height == 8) {
            ++pencil_pixels;
        }
    }
    void fill_rounded_rect(gf::Rect rectangle, double radius, gf::Color color) override {
        if (color.red == 245 && color.green == 65 && color.blue == 118 && color.alpha < 255 && radius > 0 &&
            rectangle.width == rectangle.height) {
            ++eraser_discs;
        }
    }
    void stroke_rect(gf::Rect, gf::Color, double) override {}
    void draw_line(gf::Point, gf::Point, gf::Color, double) override {}
    void draw_text_utf8(gf::Point, std::string_view, gf::FontSpec, gf::Color) override {}
    void draw_image(gf::ImageId, gf::Rect, double) override {}
};
void pencil_and_eraser_hover_are_display_only() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.canvas().set_view(8, {0, 0});
    editor.document.ink.primary = {20, 70, 110, 255};
    paint::Image original = editor.document.image;
    std::size_t undo = editor.document.undo_history.size();
    fixture.pointer(gf::PointerAction::move, 20, 30, gf::PointerButton::none);
    PreviewPainter pencil;
    editor.paint_canvas_overlay(pencil, {});
    require(pencil.pencil_pixels == 1, "pencil fills exactly the image pixel under its tip at zoom");
    editor.choose_tool(paint::Tool::Eraser);
    editor.document.ink.size = 20;
    PreviewPainter eraser;
    editor.paint_canvas_overlay(eraser, {});
    require(eraser.eraser_discs == 1, "hard eraser shows the original translucent pink disc");
    editor.eraser_soft = true;
    PreviewPainter soft;
    editor.paint_canvas_overlay(soft, {});
    require(soft.eraser_discs == 32, "soft eraser preview follows the original spherical shells");
    require(editor.document.undo_history.size() == undo &&
                std::memcmp(editor.document.image.pixels.data(), original.pixels.data(),
                            original.pixels.size() * sizeof(paint::Color)) == 0,
            "hover previews never modify artwork or undo history");
}
void zoom_out_clamps_each_axis() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.document.new_image(1000, 100);
    editor.refresh();
    window.resize({500, 500});
    window.perform_layout();
    gf::Rect viewport = editor.canvas().client_rectangle();
    require(viewport.width == 500, "zoom fixture has the specified 500-pixel viewport");
    editor.canvas().set_view(1, {500, 20});
    std::shared_ptr<gf::TrackBar> slider =
        std::dynamic_pointer_cast<gf::TrackBar>(window.find("zoom-slider"));
    (*slider).set_value(std::log2(0.75));
    gui_drawing::PointF origin = editor.canvas().view_origin();
    require(std::abs(-origin.x * 0.75 + 250) < 1e-9, "zoom-out exposes the right edge without empty space");
    require(std::abs(-origin.y * 0.75 - (viewport.height - 75) / 2) < 1e-9,
            "short axis centers while wide axis still exceeds viewport");
    double previous = -origin.x * 0.75;
    for (int step = 1; step <= 30; ++step) {
        double scale = 0.75 - step * 0.01;
        (*slider).set_value(std::log2(scale));
        double translation = -editor.canvas().view_origin().x * scale;
        require(translation >= previous && translation - previous <= 10.000001,
                "continuous zoom-out approaches and crosses fit without a final snap");
        previous = translation;
    }
    (*slider).set_value(std::log2(0.25));
    require(std::abs(-editor.canvas().view_origin().x * 0.25 - 125) < 1e-9,
            "further shrinking remains centered");
    editor.execute("fit");
    double scale = editor.canvas().zoom();
    require(std::abs(-editor.canvas().view_origin().x * scale - (500 - 1000 * scale) / 2) < 1e-9 &&
                std::abs(-editor.canvas().view_origin().y * scale - (viewport.height - 100 * scale) / 2) <
                    1e-9,
            "explicit Fit uses balanced margins on both axes");
}
void restored_help_and_selection_workflows() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    require(!window.find("patterns-stamp") && !window.find("patterns-path") && !window.find("reshape"),
            "patterns page does not duplicate tools or expose mesh");
    routed_button(window, "tool-0");
    routed_button(window, "tool-tab");
    require(!(*window.find("context-reshape")).visible(), "mesh hidden before a selection exists");
    editor.choose_tool(paint::Tool::Lasso);
    fixture.pointer(gf::PointerAction::down, 10, 10);
    fixture.pointer(gf::PointerAction::move, 45, 10);
    fixture.pointer(gf::PointerAction::move, 45, 45);
    fixture.pointer(gf::PointerAction::up, 10, 45);
    window.perform_layout();
    require((*require_button(window, "tool-0")).selected(), "lasso retains Select tool highlight");
    require((*window.find("context-reshape")).visible() && (*window.find("rotation-degrees")).visible(),
            "completed lasso reveals selection mesh and rotation controls");
    std::shared_ptr<gf::NumericUpDown> angle =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("rotation-degrees"));
    (*angle).set_value(30);
    routed_button(window, "rotate-custom");
    await_background(fixture);
    require(editor.document.selection.active, "rotation applies through the Selection ribbon");
    editor.execute("release");
    double width = editor.canvas().client_rectangle().width;
    gf::KeyEvent help{gf::KeyAction::down, gf::PhysicalKey::f1};
    static_cast<void>(window.dispatch_key(help));
    window.perform_layout();
    require(editor.show_help && editor.canvas().client_rectangle().width == width - 370,
            "F1 opens the original docked book and reduces canvas viewport");
    require((*window.find("help-body-1")).visible() && !(*window.find("help-body-2")).visible(),
            "first chapter is open and remaining topics are folded");
    routed_button(window, "help-topic-2");
    require((*window.find("help-body-2")).visible(), "help chapters expand through normal routed buttons");
    static_cast<void>(window.dispatch_key(help));
    window.perform_layout();
    require(!editor.show_help && editor.canvas().client_rectangle().width == width, "F1 closes book");
}
void stamp_scrubs_one_undo_gesture() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.image.reset(128, 96, {255, 255, 255, 255});
    for (int y = 4; y < 12; ++y) {
        for (int x = 4; x < 12; ++x) {
            editor.document.image.set(x, y, {200, 30, 60, 255});
        }
    }
    editor.choose_tool(paint::Tool::Stamp);
    editor.document.stamp_shape = paint::StampShape::Square;
    editor.stamp_width = 8;
    fixture.click(8, 8);
    await_background(fixture);
    std::size_t checkpoints = editor.document.undo_history.size();
    fixture.drag(25, 40, 100, 40);
    require(editor.document.undo_history.size() == checkpoints + 1, "stamp scrub has one undo checkpoint");
    for (int x = 25; x <= 100; ++x) {
        require(!white(editor.document.image.get(x, 40)), "stamp deposits continuously across sparse motion");
    }
    editor.execute("undo");
    for (int x = 25; x <= 100; ++x) {
        require(white(editor.document.image.get(x, 40)), "one undo removes the entire scrub");
    }
    require(!white(editor.document.image.get(8, 8)), "stamp scrub and undo preserve original sample");
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
        zoom_out_clamps_each_axis();
        pencil_and_eraser_hover_are_display_only();
        restored_help_and_selection_workflows();
        stamp_scrubs_one_undo_gesture();
        atlas_grid_frames_and_cursor_save();
        desktop_transactions_drop_and_handles();
        atlas_large_sheet_uses_visible_thumbnail_resources();
        background_rotation_mesh_and_stamp();
        skew_transaction_and_undo();
        canvas_text_editing_and_commit();
        ribbon_tabs_status_and_context();
        ribbon_galleries_and_modal_transactions();
        std::cout << "GUI.Forms: RGBA, routed capture, undo, material strokes, curves/save, selections, "
                     "path, stamp and anchored zoom passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
