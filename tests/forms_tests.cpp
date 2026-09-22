#include "codecs.hpp"
#include "cursors/tool_cursors.hpp"
#include "conv.hpp"
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
        (*editor).settings.drag_shapes = true; // Existing fixtures exercise the optional drag workflow.
        (*editor).document.new_image(128, 96);
        (*editor).refresh();
        window = std::make_unique<gf::Window>(editor, gf::Size{1280, 820});
        (*window).perform_layout();
    }
    gf::Point position(double x, double y) {
        paint::forms::PaintCanvas& canvas = (*editor).canvas();
        gui_drawing::PointF origin = canvas.view_origin();
        const gui_drawing::PointF point = canvas.view_point({x, y});
        return canvas.point_to_window(
            {(point.x - origin.x) * canvas.zoom(), (point.y - origin.y) * canvas.zoom()});
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
void tool_cursor_routes() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    for (int i = 0; i <= static_cast<int>(paint::Tool::Spirograph); ++i) {
        editor.document.tool = static_cast<paint::Tool>(i);
        fixture.pointer(gf::PointerAction::move, 64, 48, gf::PointerButton::none);
        if (editor.document.tool == paint::Tool::Stamp && editor.document.stamp.pixels.empty()) {
            require(!editor.canvas().effective_cursor_images(), "empty stamp keeps a visible stock cursor");
            continue;
        }
        require(editor.canvas().effective_cursor_images() ==
                    paint::forms::tool_cursor_images(editor.document.tool),
                "pointer route projects the current tool cursor");
    }
    editor.document.tool = paint::Tool::Pencil;
    fixture.pointer(gf::PointerAction::move, 128, 96, gf::PointerButton::none);
    require(!editor.canvas().effective_cursor_images() &&
                editor.canvas().effective_cursor() == gf::CursorKind::resize_diagonal_down,
            "canvas corner uses native resize cursor");
    fixture.pointer(gf::PointerAction::move, 64, 48, gf::PointerButton::none);
    require(editor.canvas().effective_cursor_images() == paint::forms::tool_cursor_images(paint::Tool::Pencil),
            "leaving resize handle restores pencil tip");
    editor.document.tool = paint::Tool::Picker;
    fixture.pointer(gf::PointerAction::down, 50, 40);
    fixture.pointer(gf::PointerAction::move, 70, 60);
    require(!editor.canvas().effective_cursor_images() &&
                editor.canvas().effective_cursor() == gf::CursorKind::hand,
            "picker drag communicates panning with a hand");
    fixture.pointer(gf::PointerAction::up, 70, 60);
    require(editor.canvas().effective_cursor_images() == paint::forms::tool_cursor_images(paint::Tool::Picker),
            "ending pan restores the dropper tip");
}
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
    std::string path, message;
    gf::HostDialogChoice choice = gf::HostDialogChoice::cancel;
    bool fail_dialogs = false;
    std::vector<std::uint64_t> dialog_ids;
    gf::HostImage clipboard;
    std::vector<std::string> clipboard_files;

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
    gf::HostClipboardFilesResult read_clipboard_files_impl() override {
        return {{}, clipboard_files, 1};
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
            message = std::get<gf::HostMessageDialogRequest>(request.payload).message;
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
    const std::uint64_t revision_before_invalid_clipboard = editor.document.revision;
    services.clipboard = {2, 2, 4, std::vector<std::byte>(4)};
    editor.execute("paste");
    require(editor.document.revision == revision_before_invalid_clipboard &&
                !editor.document.selection.active,
            "inconsistent toolkit clipboard geometry is rejected before copying pixels");
    services.clipboard = {};
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
void startup_file_opens_after_window_attachment() {
    const std::string path = (std::filesystem::temp_directory_path() / "rainstar-startup-forms.png").string();
    paint::Image image;
    image.reset(41, 23, {255, 255, 255, 255});
    image.set(9, 7, {63, 141, 207, 128});
    paint::save_image(image, path);
    const std::shared_ptr<paint::forms::Editor> editor =
        gf::make_control<paint::forms::Editor>(gf::StableId("startup.editor"));
    gf::Window window(editor, {1280, 820});
    (*editor).ready(window, {}, path);
    window.perform_layout();
    require((*editor).document.filename == path && !(*editor).document.dirty(),
            "startup opens the supplied file as a saved document");
    require((*editor).document.image.width == 41 && (*editor).document.image.height == 23 &&
                paint::equal((*editor).document.image.get(9, 7), {63, 141, 207, 128}),
            "startup preserves decoded dimensions and straight RGBA");
    require((*(*editor).canvas().bitmap()).width() == 41 &&
                (*editor).canvas().last_resource_error() == gf::ImageResourceError::none,
            "startup publishes the image only after the canvas has a window");
    std::filesystem::remove(path);
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
            "right pencil stroke uses Alt with a solid Primary");
    paint::select_pattern(editor.document.ink, paint::Pattern::Checker);
    fixture.pointer(gf::PointerAction::down, 10, 20, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::up, 20, 20, gf::PointerButton::secondary);
    require(paint::equal(editor.document.image.get(15, 20), editor.document.ink.secondary),
            "right stroke uses enabled Alt");
}
void solid_primary_keeps_alternate_drawing() {
    for (paint::Tool tool : {paint::Tool::Pencil, paint::Tool::Brush, paint::Tool::Fill}) {
        Fixture fixture;
        paint::forms::Editor& editor = *fixture.editor;
        editor.choose_tool(tool);
        editor.document.ink.primary = {20, 80, 180, 255};
        editor.document.ink.secondary = {220, 40, 70, 255};
        paint::select_pattern(editor.document.ink, paint::Pattern::Solid);
        paint::select_pattern(editor.document.alt_ink, paint::Pattern::Solid);
        fixture.pointer(gf::PointerAction::down, 30, 30, gf::PointerButton::secondary);
        fixture.pointer(gf::PointerAction::up, 30, 30, gf::PointerButton::secondary);
        require(paint::equal(editor.document.image.get(30, 30), editor.document.ink.secondary),
                "solid Primary permits Alt pencil, brush and fill drawing");
        require(editor.document.undo_history.size() == 1, "Alt drawing creates one undo transaction");
        editor.execute("undo");
        require(white(editor.document.image.get(30, 30)), "Alt drawing remains undoable");
        editor.execute("redo");
        require(paint::equal(editor.document.image.get(30, 30), editor.document.ink.secondary),
                "Alt drawing remains redoable");
    }
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
    if (!button) {
        throw std::runtime_error("expected retained button: " + id);
    }
    return button;
}
void routed_button(gf::Window& window, const std::string& id,
                   gf::PointerButton mouse = gf::PointerButton::primary) {
    std::shared_ptr<gf::ButtonBase> button = std::dynamic_pointer_cast<gf::ButtonBase>(window.find(id));
    if (!button) {
        throw std::runtime_error("expected routed button: " + id);
    }
    window.perform_layout();
    gf::Rect bounds = (*button).absolute_bounds();
    gf::Point point{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
    if (!window.dispatch_pointer({gf::PointerAction::down, mouse, point})) {
        throw std::runtime_error("button consumes routed pointer down: " + id +
                                 " bounds=" + std::to_string(bounds.x) + "," + std::to_string(bounds.y) +
                                 "," + std::to_string(bounds.width) + "," + std::to_string(bounds.height));
    }
    if (!window.dispatch_pointer({gf::PointerAction::up, mouse, point})) {
        throw std::runtime_error("button consumes routed pointer up: " + id);
    }
}
void open_tab(gf::Window& window, const std::string& id) {
    if (!(*require_button(window, id)).selected() ||
        (*window.find("ribbon")).committed_arranged_bounds().height < 40) {
        routed_button(window, id);
    }
}
void pencil_is_independent_of_brush_material() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.ink.primary = {23, 68, 115, 255};
    editor.document.ink.brush = paint::Brush::Marker;
    editor.document.ink.pattern = paint::Pattern::Diagonal;
    editor.document.ink.transparent_pattern = true;
    editor.document.ink.pigment_load = 0;
    editor.choose_tool(paint::Tool::Brush);
    open_tab(*fixture.window, "home-tab");
    routed_button(*fixture.window, "tool-2");
    fixture.drag(12.25, 22.75, 105.25, 22.75);
    for (int x = 12; x <= 105; ++x) {
        require(paint::equal(editor.document.image.get(x, 22), editor.document.ink.primary),
                "Home Pencil must draw every pixel after a patterned Marker brush");
        require(white(editor.document.image.get(x, 21)) && white(editor.document.image.get(x, 23)),
                "Home Pencil stays exactly one pixel wide");
    }
    require(editor.document.ink.pattern == paint::Pattern::Diagonal &&
                editor.document.ink.brush == paint::Brush::Marker,
            "Pencil leaves the stored brush material available for returning to Brushes");
}
void ribbon_galleries_and_modal_transactions() {
    Fixture fixture;
    gf::Window& window = *fixture.window;
    paint::forms::Editor& editor = *fixture.editor;
    routed_button(window, "brush-menu");
    window.perform_layout();
    require(!window.find("ribbon-popup") && (*window.find("material-brush-4")).visible(),
            "brush gallery opens in Materials pane");
    require((*require_button(window, "material-brush-4")).image_list() != nullptr,
            "brush gallery uses rendered artwork previews");
    routed_button(window, "material-brush-4");
    require(editor.document.tool == paint::Tool::Brush && editor.document.ink.brush == paint::Brush::Oil &&
                !window.find("ribbon-popup"),
            "gallery selection changes brush and closes popup");
    open_tab(window, "patterns-tab");
    window.perform_layout();
    routed_button(window, "r-pattern-12");
    require(editor.document.ink.pattern == paint::Pattern::Checker,
            "pattern gallery changes the actual ink pattern");
    editor.choose_shape(paint::Shape::Bezier);
    fixture.drag(12, 20, 85, 70);
    require(editor.document.curve.line_set, "curve is live before shape switch");
    open_tab(window, "home-tab");
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
    routed_button(window, "color-space-okhsl");
    const std::shared_ptr<paint::forms::EditorDialog> color_dialog =
        std::dynamic_pointer_cast<paint::forms::EditorDialog>(window.find("editor-dialog"));
    require((*color_dialog).picker_space == paint::PickerSpace::OKHSL && window.focus_scope_depth() == 1,
            "side-by-side OKHSL button switches space without another popup");
    routed_button(window, "color-mosaic");
    require(window.find("editor-dialog") != nullptr, "Mosaic remains in the live color dialog");
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
    open_tab(window, "view-tab");
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
    editor.choose_tool(paint::Tool::Brush);
    open_tab(window, "patterns-tab");
    require((*window.find("grain-scale")).visible() && !window.find("ribbon-popup"),
            "patterns live on a ribbon page");
    routed_button(window, "r-pattern-12");
    require(editor.document.ink.pattern == paint::Pattern::Checker, "ribbon swatch selects ink pattern");
    std::shared_ptr<gf::NumericUpDown> grain =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("grain-scale"));
    (*grain).set_value(1.75);
    require(editor.document.ink.grain_scale == 1.75, "material settings update drawing ink");
    editor.choose_tool(paint::Tool::Stamp);
    open_tab(window, "tool-tab");
    require((*window.find("stamp-shapes-menu")).visible() && !(*window.find("grain-scale")).visible(),
            "stamp context hides unrelated material settings");
    routed_button(window, "stamp-shapes-menu");
    routed_button(window, "popup-stamp-shape-1");
    require(editor.document.stamp_shape == paint::StampShape::Pill, "context changes stamp capture shape");
    editor.choose_tool(paint::Tool::Brush);
    require(!(*window.find("stamp-shapes-menu")).visible() && !(*window.find("grain-scale")).visible() &&
                (*window.find("tool-size")).visible(),
            "context swaps tool settings while materials remain on their shared pane");
    open_tab(window, "home-tab");
    routed_button(window, "shapes-menu");
    window.perform_layout();
    require((*require_button(window, "popup-shape-3")).accessible_name() == "Rectangle",
            "expanded shapes expose distinct names");
    for (int i = 0; i < paint::shape_count; ++i) {
        std::shared_ptr<gf::Button> shape = require_button(window, "popup-shape-" + std::to_string(i));
        require((*shape).image_list() && (*(*shape).image_list()).contains_key((*shape).image_key()),
                "every expanded shape resolves its actual image resource");
    }
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
void persistent_selection_painting() {
    const paint::Tool tools[] = {paint::Tool::Pencil, paint::Tool::Brush, paint::Tool::Eraser,
                                 paint::Tool::Fill, paint::Tool::Shape};
    for (paint::Tool tool : tools) {
        Fixture fixture;
        paint::forms::Editor& editor = *fixture.editor;
        editor.document.image.reset(128, 96, {220, 225, 230, 255});
        const paint::Image base = editor.document.image;
        editor.document.select({30, 20, 60, 50}, {{30, 20}, {90, 20}, {70, 70}, {45, 70}});
        editor.choose_tool(paint::Tool::Select);
        editor.choose_tool(tool);
        editor.document.ink.primary = {190, 30, 75, 255};
        editor.document.ink.size = 30;
        editor.document.shape = paint::Shape::Rectangle;
        editor.document.shape_fill = true;
        editor.refresh();
        if (tool == paint::Tool::Fill) {
            fixture.click(54, 42);
        } else if (tool == paint::Tool::Shape) {
            fixture.drag(5, 5, 115, 85);
        } else {
            fixture.drag(5, 42, 115, 42);
        }
        require(editor.document.selection.active && editor.document.selection.on_canvas,
                "canvas selection persists across tools and drawing");
        bool changed = false;
        for (int y = 0; y < 96; ++y) {
            for (int x = 0; x < 128; ++x) {
                const bool different = !paint::equal(base.get(x, y), editor.document.image.get(x, y));
                require(!different || editor.document.selection.contains(x, y),
                        "painting never changes pixels outside lasso mask");
                changed = changed || different;
            }
        }
        require(changed, "selected canvas receives paint");
        const paint::Image painted = editor.document.image;
        editor.execute("undo");
        require(editor.document.selection.active && editor.document.selection.on_canvas &&
                    editor.document.image.pixels.size() == base.pixels.size() &&
                    std::memcmp(editor.document.image.pixels.data(), base.pixels.data(),
                                base.pixels.size() * sizeof(paint::Color)) == 0,
                "stroke undo restores canvas while retaining selection");
        editor.execute("redo");
        require(editor.document.selection.active &&
                    std::memcmp(editor.document.image.pixels.data(), painted.pixels.data(),
                                painted.pixels.size() * sizeof(paint::Color)) == 0,
                "stroke redo retains selection and exact artwork");
    }
}
void selection_holes_flood_and_floating_paste() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    paint::SelectionMask mask;
    mask.bounds = {20, 20, 80, 50};
    mask.coverage.resize(80 * 50);
    for (int y = 0; y < 50; ++y) {
        for (int x = 0; x < 80; ++x) {
            mask.coverage[y * 80 + x] =
                (x < 25 || x >= 55) && !(x >= 8 && x < 16 && y >= 15 && y < 35) ? 255 : 0;
        }
    }
    editor.document.select_mask(mask);
    editor.choose_tool(paint::Tool::Fill);
    editor.document.ink.primary = {40, 90, 180, 255};
    fixture.click(25, 42);
    require(!white(editor.document.image.get(25, 42)) && white(editor.document.image.get(85, 42)) &&
                white(editor.document.image.get(30, 42)),
            "fill respects holes and cannot cross unselected space to another island");
    const std::uint64_t revision = editor.document.revision;
    static_cast<void>((*fixture.window).dispatch_key({gf::KeyAction::down, gf::PhysicalKey::q}));
    require(editor.document.selection.on_canvas && editor.document.revision == revision,
            "unrelated keys do not lift a persistent selection or create history");
    fixture.click(60, 42);
    require(white(editor.document.image.get(85, 42)), "fill outside selection does nothing");
    editor.choose_tool(paint::Tool::Pencil);
    fixture.drag(5, 10, 115, 10);
    require(white(editor.document.image.get(60, 10)), "outside strokes are clipped");
    editor.execute("release");
    fixture.drag(5, 10, 115, 10);
    require(!white(editor.document.image.get(60, 10)), "deselect restores unrestricted painting");
    paint::Image pasted;
    pasted.reset(30, 20, {220, 30, 40, 255});
    editor.document.paste(pasted, 45, 30);
    editor.choose_tool(paint::Tool::Pencil);
    editor.document.ink.primary = {10, 80, 150, 255};
    fixture.drag(5, 42, 115, 42);
    require(editor.document.selection.active && !editor.document.selection.canvas_selection &&
                paint::equal(editor.document.image.get(60, 42), editor.document.ink.primary) &&
                paint::equal(editor.document.visible_image().get(60, 42), pasted.get(0, 0)) &&
                paint::equal(editor.document.image.get(10, 42), editor.document.ink.primary),
            "paste stays above unrestricted underlying canvas painting");
    editor.execute("undo");
    require(editor.document.selection.active && !editor.document.selection.canvas_selection,
            "undoing paint below paste retains floating paste");
}
void selection_text_path_carpet_and_move() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.select({30, 20, 60, 50});
    editor.choose_tool(paint::Tool::Brush);
    editor.brush_family = paint::BrushFamily::Carpet;
    editor.carpet_tile.reset(8, 8, {60, 120, 180, 255});
    editor.document.ink.size = 30;
    fixture.drag(5, 42, 115, 42);
    require(white(editor.document.image.get(20, 42)) && !white(editor.document.image.get(50, 42)),
            "carpet is clipped to persistent selection");
    editor.choose_tool(paint::Tool::Text);
    fixture.click(8, 22);
    require((*fixture.window).dispatch_text({"MMMMMMMMMMMM"}), "masked text receives input");
    editor.execute("text-place");
    for (int y = 0; y < 96; ++y) {
        for (int x = 0; x < 128; ++x) {
            require(editor.document.selection.contains(x, y) || white(editor.document.image.get(x, y)),
                    "text placement preserves every pixel outside selection");
        }
    }
    editor.choose_tool(paint::Tool::Stamp);
    fixture.click(55, 42);
    await_background(fixture);
    fixture.drag(10, 35, 110, 35);
    for (int y = 0; y < 96; ++y) {
        for (int x = 0; x < 128; ++x) {
            require(editor.document.selection.contains(x, y) || white(editor.document.image.get(x, y)),
                    "stamp scrub preserves every pixel outside selection");
        }
    }
    editor.choose_tool(paint::Tool::Path);
    fixture.click(5, 60);
    fixture.click(115, 60);
    editor.execute("finish-path");
    require(editor.document.selection.active && white(editor.document.image.get(10, 60)) &&
                !white(editor.document.image.get(50, 60)),
            "retained path clips and keeps mask");
    editor.choose_tool(paint::Tool::Select);
    const paint::Image before = editor.document.image;
    const paint::Image copied = editor.document.selected_image();
    require(paint::equal(copied.get(20, 22), before.get(50, 42)), "copy reads newly painted selected pixels");
    fixture.drag(50, 42, 60, 52);
    require(editor.document.selection.x == 40 && editor.document.selection.y == 30 &&
                paint::equal(editor.document.visible_image().get(60, 52), before.get(50, 42)),
            "moving mask lifts current painted artwork");
    editor.execute("undo");
    require(editor.document.selection.on_canvas && editor.document.selection.x == 30 &&
                std::memcmp(before.pixels.data(), editor.document.image.pixels.data(),
                            before.pixels.size() * sizeof(paint::Color)) == 0,
            "move undo restores exact painted canvas and mask");
}
void selection_skew_handles() {
    for (int handle = 0; handle < 4; ++handle) {
        Fixture fixture;
        paint::forms::Editor& editor = *fixture.editor;
        editor.document.image.reset(128, 96, {77, 88, 99, 0});
        editor.document.select({30, 24, 40, 32});
        editor.document.settle_selection();
        editor.choose_tool(paint::Tool::Select);
        const double hx[] = {50, 86, 50, 14}, hy[] = {8, 40, 72, 40};
        const bool horizontal = handle == 0 || handle == 2;
        fixture.drag(hx[handle], hy[handle], hx[handle] + (horizontal ? 12 : 0),
                     hy[handle] + (horizontal ? 0 : -12));
        await_background(fixture);
        require(!editor.warp_active() && editor.document.selection.canvas_selection &&
                    editor.document.selection.image.width == (horizontal ? 52 : 40) &&
                    editor.document.selection.image.height == (horizontal ? 32 : 44),
                "four skew handles move their edge along the advertised axis");
        int selected = 0;
        for (std::uint8_t coverage : editor.document.selection.coverage) {
            selected += coverage != 0;
        }
        require(selected == 40 * 32, "skew transforms geometric mask even over transparent canvas");
        editor.choose_tool(paint::Tool::Pencil);
        editor.document.ink.primary = {180, 30, 80, 255};
        fixture.drag(5, 40, 120, 40);
        require(editor.document.selection.active, "skewed selection persists as painting mask");
        editor.execute("undo");
        editor.execute("undo");
        require(editor.document.selection.on_canvas && editor.document.selection.x == 30 &&
                    editor.document.selection.y == 24 && editor.document.selection.image.width == 40,
                "skew undo restores original selection and canvas");
    }
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
    paint::render_affine(field, map, bounds.w, bounds.h, expected, paint::WarpSampling::Area);
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
    open_tab(window, "atlas-tab");
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
    std::string filename = editor.document.filename;
    std::size_t undo_count = editor.document.undo_history.size();
    paint::Image before_drop = editor.document.image;
    require(window.dispatch_drag(drag).accepted_effect == gf::DragEffect::copy &&
                editor.document.image.width == 128 && editor.document.filename == filename &&
                editor.document.selection.active && editor.document.selection.image.width == 37 &&
                editor.document.selection.x == 20 && editor.document.selection.y == 20,
            "drop pastes floating artwork at the pointer and preserves the current document");
    require(editor.document.undo_history.size() == undo_count + 1 &&
                std::memcmp(before_drop.pixels.data(), editor.document.image.pixels.data(),
                            before_drop.pixels.size() * sizeof(paint::Color)) == 0,
            "drop keeps existing pixels intact and creates one paste transaction");
    fixture.drag(30, 30, 40, 40);
    require(editor.document.selection.x == 30 && editor.document.selection.y == 30,
            "dropped artwork moves before placement");
    require(window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::escape}),
            "drop focuses the canvas so Escape places the image");
    require(!editor.document.selection.active &&
                paint::equal(editor.document.image.get(35, 35), drop_image.get(5, 5)),
            "Escape commits the dropped image at its moved position");
    editor.execute("undo");
    require(std::memcmp(before_drop.pixels.data(), editor.document.image.pixels.data(),
                        before_drop.pixels.size() * sizeof(paint::Color)) == 0,
            "undo restores the document before the drop");
    services.clipboard = {1, 1, 4, std::vector<std::byte>(4, std::byte{255})};
    services.clipboard_files = {path};
    editor.execute("paste");
    require(editor.document.selection.active && editor.document.selection.image.width == 37 &&
                editor.document.selection.image.height == 29 &&
                paint::equal(editor.document.selection.image.get(5, 5), drop_image.get(5, 5)),
            "copied image files take priority over their clipboard icon bitmap");
    editor.execute("release");
    services.clipboard_files = {std::string("bad\0path", 8)};
    gf::HostClipboardFilesResult invalid = services.read_clipboard_files();
    require(!invalid.status.accepted() && invalid.paths_utf8.empty(),
            "host rejects embedded NUL in a clipboard file path");
    services.clipboard_files.assign(65, path);
    invalid = services.read_clipboard_files();
    require(invalid.status.error == gf::HostServiceError::too_large && invalid.paths_utf8.empty(),
            "host bounds the number of clipboard file references");
    services.clipboard_files.clear();
    editor.execute("paste");
    require(editor.document.selection.image.width == 1,
            "ordinary bitmap paste remains available without file references");
    editor.execute("release");
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
    int pencil_pixels = 0, eraser_discs = 0, lens_samples = 0, guide_lines = 0;
    bool lens_caption = false;
    gf::ImageId image;
    gf::Rect image_bounds;
    std::vector<gf::ImageId> painted_images;
    std::vector<gf::Point> text_caret_points;
    void save() override {}
    void restore() override {}
    void translate(gf::Point) override {}
    void clip_rect(gf::Rect) override {}
    void fill_rect(gf::Rect rectangle, gf::Color color) override {
        if (color.red == 20 && color.green == 70 && color.blue == 110 && rectangle.width == 1 &&
            rectangle.height == 1) {
            ++lens_samples;
        }
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
    void draw_line(gf::Point first, gf::Point last, gf::Color color, double) override {
        if (color.red == 29 && color.green == 103 && color.blue == 180) {
            text_caret_points.push_back(first);
            text_caret_points.push_back(last);
        }
        if (color.red == 26 && color.green == 112 && color.blue == 174 && color.alpha == 230) {
            ++guide_lines;
        }
    }
    void draw_text_utf8(gf::Point, std::string_view text, gf::FontSpec, gf::Color) override {
        if (text == "8×") {
            lens_caption = true;
        }
    }
    void draw_image(gf::ImageId id, gf::Rect bounds, double) override {
        image = id;
        image_bounds = bounds;
        painted_images.push_back(id);
    }
};
void text_caret_deadlines_damage_only_the_transformed_caret() {
    for (int transformed = 0; transformed < 2; ++transformed) {
        Fixture fixture;
        gf::Window& window = *fixture.window;
        paint::forms::Editor& editor = *fixture.editor;
        routed_button(window, "text");
        fixture.click(12, 14);
        require(window.dispatch_text({"Hi"}), "text input starts caret workload");
        editor.text.resize({12, 14, 110, 70});
        editor.text.style.skew = transformed ? 0.3 : 0;
        editor.text.style.perspective = transformed ? 0.2 : 0;
        editor.text.style.warp = transformed ? 0.25 : 0;
        editor.canvas().set_view(1.5, {-8, -6});
        editor.refresh();
        editor.text_focus(true);
        PreviewPainter overlay;
        editor.paint_canvas_overlay(overlay, {});
        require(!overlay.text_caret_points.empty(), "active text paints a caret");
        PreviewPainter initial;
        const std::optional<gf::PaintReceipt> receipt = window.paint(initial);
        if (receipt) {
            static_cast<void>(window.notify_presented(*receipt));
        }
        static_cast<void>(window.take_damage());
        const gf::FramePollResult frame =
            window.poll_frame_schedule(gf::FrameClock::now() + std::chrono::milliseconds(600));
        require(frame.deadlines_fired > 0, "caret deadline fires through the window scheduler");
        const gf::Rect damage = window.take_damage().bounds();
        require(!damage.empty() && damage.width < 100 && damage.height < 100,
                "caret deadline does not invalidate the canvas or whole window");
        for (const gf::Point point : overlay.text_caret_points) {
            const gf::Point native = editor.canvas().point_to_window(point);
            require(native.x >= damage.x && native.x <= damage.x + damage.width && native.y >= damage.y &&
                        native.y <= damage.y + damage.height,
                    "caret damage covers every transformed painted segment");
        }
        PreviewPainter hidden;
        editor.paint_canvas_overlay(hidden, {});
        require(hidden.text_caret_points.empty(), "deadline hides the caret without altering text");
        require(editor.text.edit.content == "Hi" && !editor.document.dirty(),
                "caret blink preserves the text and document");
    }
}
void transforms_preview_before_release() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.image.reset(128, 96, {40, 110, 170, 255});
    editor.document.select({20, 20, 40, 30});
    editor.refresh();
    paint::Image original = editor.document.selection.image;
    std::size_t undo = editor.document.undo_history.size();
    fixture.pointer(gf::PointerAction::down, 60, 50);
    fixture.pointer(gf::PointerAction::move, 80, 65);
    PreviewPainter stretch;
    editor.paint_canvas_overlay(stretch, {});
    require(stretch.image.value && stretch.image_bounds.width == 60 && stretch.image_bounds.height == 45,
            "selection stretching publishes visible pixels before pointer release");
    require(editor.document.selection.image.width == 40 && editor.document.undo_history.size() == undo,
            "stretch preview leaves original samples and history untouched");
    fixture.pointer(gf::PointerAction::move, 45, 40);
    PreviewPainter contract;
    editor.paint_canvas_overlay(contract, {});
    require(contract.image.value && contract.image_bounds.width == 25 && contract.image_bounds.height == 20,
            "selection contraction updates visible pixels while dragging");
    fixture.pointer(gf::PointerAction::up, 45, 40);
    paint::Image expected;
    paint::conv_resize_area(original, 25, 20, expected);
    require(editor.document.selection.image.pixels.size() == expected.pixels.size() &&
                std::memcmp(expected.pixels.data(), editor.document.selection.image.pixels.data(),
                            expected.pixels.size() * sizeof(paint::Color)) == 0,
            "release commits authoritative CONV resize pixels");
    // Rotation handle is 18 screen pixels beyond the upper-right corner.
    double cx = 32.5, cy = 30, hx = 63, hy = 2;
    fixture.pointer(gf::PointerAction::down, hx, hy);
    double angle = 2 * std::acos(-1.0) / 180;
    double rx = cx + std::cos(angle) * (hx - cx) - std::sin(angle) * (hy - cy);
    double ry = cy + std::sin(angle) * (hx - cx) + std::cos(angle) * (hy - cy);
    fixture.pointer(gf::PointerAction::move, rx, ry);
    PreviewPainter rotation;
    editor.paint_canvas_overlay(rotation, {});
    require(rotation.image.value && std::abs(editor.rotation_angle - 2) < 0.01 &&
                rotation.image_bounds.width > 25 && rotation.image_bounds.height > 20,
            "first two degrees of rotation publish pixels without waiting for the worker");
    std::optional<gf::ImageResourceView> pixels = (*fixture.window).image_resources().find(rotation.image);
    require(pixels.has_value(), "rotation preview owns a live image resource");
    int visible = 0;
    for (std::size_t i = 0; i + 3 < (*pixels).encoded.size(); i += 4) {
        if ((*pixels).encoded[i] == std::byte{170} && (*pixels).encoded[i + 1] == std::byte{110} &&
            (*pixels).encoded[i + 2] == std::byte{40} && (*pixels).encoded[i + 3] == std::byte{255}) {
            ++visible;
        }
    }
    require(visible > 100, "the immediate rotation contains the selection's visible colored pixels");
    editor.cancel_warp();
    await_background(fixture);
    PreviewPainter canceled;
    editor.paint_canvas_overlay(canceled, {});
    require(!canceled.image.value && editor.document.selection.image.width == 25,
            "cancel removes display preview and retains original selection");
}
void selection_handles_repaint_during_capture() {
    const double handles[8][2] = {{0, 0}, {0.5, 0}, {1, 0}, {1, 0.5}, {1, 1}, {0.5, 1}, {0, 1}, {0, 0.5}};
    for (int handle = 0; handle < 8; ++handle) {
        Fixture fixture;
        paint::forms::Editor& editor = *fixture.editor;
        gf::Window& window = *fixture.window;
        paint::Image sample;
        sample.reset(40, 30, {40, 110, 170, 255});
        editor.document.paste(sample, 30, 25);
        editor.canvas().set_view(2, {-8, -6});
        editor.refresh();
        PreviewPainter initial;
        std::optional<gf::PaintReceipt> receipt = window.paint(initial);
        if (receipt) {
            static_cast<void>(window.notify_presented(*receipt));
        }
        double x = 30 + handles[handle][0] * 40, y = 25 + handles[handle][1] * 30;
        fixture.pointer(gf::PointerAction::down, x, y);
        std::uint64_t prior = 0;
        for (int move = 0; move < 2; ++move) {
            double delta = move == 0 ? 12 : -6;
            double dx = handles[handle][0] == 0.5 ? 0 : handles[handle][0] == 0 ? -delta : delta;
            double dy = handles[handle][1] == 0.5 ? 0 : handles[handle][1] == 0 ? -delta : delta;
            fixture.pointer(gf::PointerAction::move, x + dx, y + dy);
            PreviewPainter overlay;
            editor.paint_canvas_overlay(overlay, {});
            require(overlay.image.value && overlay.image.value != prior &&
                        editor.canvas().has_pointer_capture(),
                    "each resize movement publishes fresh preview pixels while capture remains active");
            prior = overlay.image.value;
            PreviewPainter frame;
            receipt = window.paint(frame);
            bool drawn = false;
            for (gf::ImageId image : frame.painted_images) {
                if (image == overlay.image) {
                    drawn = true;
                }
            }
            require(receipt.has_value() && drawn,
                    "retained window repaint includes the new resize image before mouse-up");
            static_cast<void>(window.notify_presented(*receipt));
            int width = 40 + (handles[handle][0] == 0.5 ? 0 : static_cast<int>(delta));
            int height = 30 + (handles[handle][1] == 0.5 ? 0 : static_cast<int>(delta));
            std::shared_ptr<gf::Label> status =
                std::dynamic_pointer_cast<gf::Label>(window.find("selection-status"));
            require((*status).text() ==
                        std::to_string(width) + " × " + std::to_string(height) + " px selected",
                    "selection status follows live side or corner resize dimensions");
            require(editor.document.selection.image.width == 40 &&
                        editor.document.selection.image.height == 30,
                    "all handle previews preserve original samples until release");
        }
        editor.execute("release");
    }
}
void large_selection_preview_workload() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.image.reset(1024, 768, {40, 110, 170, 255});
    editor.document.select({0, 0, 900, 550});
    editor.refresh();
    std::size_t undo = editor.document.undo_history.size();
    fixture.pointer(gf::PointerAction::down, 900, 550);
    std::vector<double> milliseconds;
    for (int step = 1; step <= 12; ++step) {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        fixture.pointer(gf::PointerAction::move, 900 + step * 3, 550 - step * 2);
        milliseconds.push_back(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    }
    PreviewPainter painter;
    editor.paint_canvas_overlay(painter, {});
    std::optional<gf::ImageResourceView> resource = (*fixture.window).image_resources().find(painter.image);
    require(resource && (*resource).encoded.size() <= 4 * 1051000,
            "large selection live preview stays bounded to approximately one megapixel");
    require(editor.document.selection.image.width == 900 && editor.document.undo_history.size() == undo,
            "continuous large preview does not resample the source or add history");
    std::sort(milliseconds.begin(), milliseconds.end());
    std::cout << "900x550 selection, 12 live resize updates: median " << milliseconds[6] << " ms, maximum "
              << milliseconds.back() << " ms (preview generation only)\n";
    editor.execute("release");
}
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
void magnifier_hover_is_display_only() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.image.set(20, 30, {20, 70, 110, 255});
    editor.choose_tool(paint::Tool::Magnifier);
    std::size_t undo = editor.document.undo_history.size();
    paint::Image before = editor.document.image;
    fixture.pointer(gf::PointerAction::move, 20, 30, gf::PointerButton::none);
    PreviewPainter lens;
    editor.paint_canvas_overlay(lens, {});
    require(lens.lens_samples == 64 && lens.lens_caption,
            "magnifier previews the pointed source pixel at eight screen pixels per image pixel");
    require(editor.document.undo_history.size() == undo &&
                std::memcmp(before.pixels.data(), editor.document.image.pixels.data(),
                            before.pixels.size() * 4) == 0,
            "magnifier hover never edits the document");
    fixture.pointer(gf::PointerAction::leave, 20, 30, gf::PointerButton::none);
    PreviewPainter gone;
    editor.paint_canvas_overlay(gone, {});
    require(!gone.lens_caption, "magnifier disappears after leaving the canvas");
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
    open_tab(window, "tool-tab");
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
    routed_button(window, "help-topic-1");
    routed_button(window, "help-topic-2");
    require((*window.find("help-body-2")).visible(), "help chapters expand through normal routed buttons");
    static_cast<void>(window.dispatch_key(help));
    window.perform_layout();
    require(!editor.show_help && editor.canvas().client_rectangle().width == width, "F1 closes book");
}
bool display_white(paint::forms::Editor& editor, int x, int y) {
    std::shared_ptr<gui_drawing::Bitmap> bitmap = editor.canvas().bitmap();
    gui_drawing::BitmapLockView view = (*bitmap).lock(gui_drawing::BitmapLockMode::read);
    const std::byte* pixel = view.data + y * view.row_bytes + x * 4;
    bool result = pixel[0] == std::byte{255} && pixel[1] == std::byte{255} && pixel[2] == std::byte{255};
    (*bitmap).unlock(view.token);
    return result;
}
void path_hover_snap_and_controls() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    routed_button(window, "tool-9");
    require(editor.document.tool == paint::Tool::Path && !window.find("ribbon-popup"),
            "Path has one main action without a duplicate continuous dropdown");
    fixture.click(15, 20);
    std::size_t undo = editor.document.undo_history.size();
    fixture.pointer(gf::PointerAction::move, 95, 50, gf::PointerButton::none);
    require(white(editor.document.image.get(55, 35)) && !display_white(editor, 55, 35),
            "pending path segment floats in presentation without entering document pixels");
    require(editor.document.undo_history.size() == undo, "path hover makes no undo records");
    fixture.click(95, 50);
    fixture.pointer(gf::PointerAction::move, 19, 23, gf::PointerButton::none);
    paint::Image snapped = editor.document.image;
    gui_drawing::BitmapLockView display = (*editor.canvas().bitmap()).lock(gui_drawing::BitmapLockMode::read);
    for (int y = 0; y < snapped.height; ++y) {
        for (int x = 0; x < snapped.width; ++x) {
            const std::byte* pixel = display.data + y * display.row_bytes + x * 4;
            paint::Color expected = snapped.get(x, y);
            require(pixel[0] == static_cast<std::byte>(expected.b) &&
                        pixel[1] == static_cast<std::byte>(expected.g) &&
                        pixel[2] == static_cast<std::byte>(expected.r),
                    "hovering a retained junction preserves the old run without a false closing segment");
        }
    }
    (*editor.canvas().bitmap()).unlock(display.token);
    fixture.click(19, 23);
    require(editor.document.path.nodes.back().x == 15 && editor.document.path.nodes.back().y == 20,
            "click snaps to the same anchor shown by the floating preview");
    editor.execute("finish-path");
    fixture.pointer(gf::PointerAction::move, 80, 80, gf::PointerButton::none);
    require(display_white(editor, 80, 80), "ending a run removes its floating segment");
    editor.execute("release");
    editor.document.new_image(128, 96);
    editor.choose_tool(paint::Tool::Path);
    open_tab(window, "tool-tab");
    std::shared_ptr<gf::CheckBox> continuous =
        std::dynamic_pointer_cast<gf::CheckBox>(window.find("continuous-path"));
    require((*continuous).checked(), "continuous path starts checked for an open chain");
    fixture.click(15, 20);
    fixture.click(95, 20);
    fixture.click(95, 80);
    require(white(editor.document.image.get(55, 50)), "checked continuous path has no closing edge");
    routed_button(window, "continuous-path");
    require(!(*continuous).checked() && !editor.document.continuous_path &&
                !white(editor.document.image.get(55, 50)),
            "unchecked continuous path closes the polygon and matches checkbox state");
    routed_button(window, "continuous-path");
    require((*continuous).checked() && white(editor.document.image.get(55, 50)),
            "checked continuous path removes the closing edge again");
    require(!(*window.find("r-pattern-0")).visible(), "path context does not duplicate Patterns");
    open_tab(window, "home-tab");
    editor.document.ink.brush = paint::Brush::Airbrush;
    routed_button(window, "primary");
    open_tab(window, "patterns-tab");
    routed_button(window, "r-pattern-0");
    require(editor.document.ink.brush == paint::Brush::Round,
            "Solid outline cannot retain the airbrush's random deposition");
}
void path_node_drag_and_overlap() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.document.image.set(50, 50, {45, 110, 90, 255});
    editor.choose_tool(paint::Tool::Path);
    fixture.click(15, 20);
    fixture.click(95, 20);
    fixture.click(95, 75);
    paint::Image old = editor.document.image;
    fixture.click(15, 20);
    require(!editor.document.path.extending && editor.document.path.runs.size() == 1 &&
                editor.document.path.nodes.size() == 4 && !white(editor.document.image.get(55, 47)),
            "clicking an earlier junction commits the closing segment before ending the run");
    fixture.click(15, 20);
    fixture.click(15, 75);
    editor.execute("finish-path");
    editor.document.ink.primary = {160, 30, 60, 255};
    fixture.click(5, 45);
    fixture.click(110, 45);
    editor.execute("finish-path");
    require(!white(editor.document.image.get(60, 20)) && !white(editor.document.image.get(95, 65)),
            "overlapping new paths preserve old segments");
    std::size_t undo = editor.document.undo_history.size();
    fixture.pointer(gf::PointerAction::down, 15, 20, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::move, 30, 30, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::move, 35, 35, gf::PointerButton::secondary);
    require(editor.document.path.nodes[0].x == 35 && editor.document.path.nodes[3].x == 35 &&
                white(editor.document.image.get(40, 20)) && !white(editor.document.image.get(65, 27)),
            "right-drag redraws every branch connected to the junction before release");
    require(paint::equal(editor.document.image.get(50, 50), {45, 110, 90, 255}) &&
                !white(editor.document.image.get(100, 45)),
            "moving a junction preserves the background and independent crossing run");
    fixture.pointer(gf::PointerAction::up, 35, 35, gf::PointerButton::secondary);
    require(editor.document.undo_history.size() == undo + 1, "one node drag creates one undo step");
    editor.execute("undo");
    require(editor.document.path.nodes[0].x == 15 && editor.document.path.nodes[3].x == 15,
            "node undo restores the whole junction");
    editor.execute("redo");
    require(editor.document.path.nodes[0].x == 35, "node redo restores the moved junction");
    require(window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::escape}), "Escape commits path");
    require(editor.document.path.nodes.empty() && !white(editor.document.image.get(100, 45)),
            "Escape clears nodes and preserves rasterized artwork");
}
void click_move_click_shapes() {
    require(!paint::EditorSettings{}.drag_shapes, "click placement is the default");
    const paint::Shape shapes[] = {paint::Shape::Line, paint::Shape::Rectangle, paint::Shape::Star5,
                                   paint::Shape::Bezier, paint::Shape::Arc};
    for (paint::Shape shape : shapes) {
        Fixture fixture;
        paint::forms::Editor& editor = *fixture.editor;
        editor.settings.drag_shapes = false;
        editor.choose_shape(shape);
        fixture.drag(20, 20, 80, 60);
        require(!editor.canvas().has_pointer_capture() && editor.document.undo_history.empty() &&
                    !editor.document.dirty(),
                "first release neither places geometry nor keeps capture");
        fixture.pointer(gf::PointerAction::move, 100, 70, gf::PointerButton::none);
        fixture.click(100, 70);
        require(editor.document.undo_history.size() == 1 && editor.document.dirty(),
                "second click places exactly one geometry transaction");
        const paint::Image committed = editor.document.image;
        fixture.pointer(gf::PointerAction::move, 115, 80, gf::PointerButton::none);
        require(std::memcmp(committed.pixels.data(), editor.document.image.pixels.data(),
                            committed.pixels.size() * sizeof(paint::Color)) == 0,
                "mouse movement after second click cannot extend geometry");
        editor.execute("undo");
        require(!editor.document.dirty(), "one undo removes click-placed geometry");
    }
    for (int cancel = 0; cancel < 3; ++cancel) {
        Fixture fixture;
        paint::forms::Editor& editor = *fixture.editor;
        editor.settings.drag_shapes = false;
        editor.choose_shape(paint::Shape::Rectangle);
        fixture.click(20, 20);
        fixture.pointer(gf::PointerAction::move, 90, 65, gf::PointerButton::none);
        require(!display_white(editor, 20, 40) && white(editor.document.image.get(20, 40)),
                "unheld mouse movement updates display-only shape preview");
        if (cancel == 0) {
            static_cast<void>((*fixture.window).dispatch_key({gf::KeyAction::down, gf::PhysicalKey::escape}));
        } else if (cancel == 1) {
            fixture.pointer(gf::PointerAction::down, 90, 65, gf::PointerButton::secondary);
            fixture.pointer(gf::PointerAction::up, 90, 65, gf::PointerButton::secondary);
        } else {
            editor.choose_tool(paint::Tool::Pencil);
        }
        require(editor.document.undo_history.empty() && !editor.document.dirty() &&
                    !editor.canvas().has_pointer_capture() && display_white(editor, 20, 40),
                "cancel or tool change removes pending shape without artwork or history");
    }
}
void centered_closed_shapes() {
    const paint::Shape shapes[] = {paint::Shape::Rectangle, paint::Shape::RoundedRectangle,
                                   paint::Shape::Diamond,   paint::Shape::Triangle,
                                   paint::Shape::Heart,     paint::Shape::RightArrow,
                                   paint::Shape::Star5,     paint::Shape::Hexagon};
    for (bool drag : {false, true}) {
        for (paint::Shape shape : shapes) {
            Fixture fixture;
            paint::forms::Editor& editor = *fixture.editor;
            editor.settings.drag_shapes = drag;
            editor.choose_shape(shape);
            editor.document.shape_fill = true;
            const gf::Modifier ctrl = gf::Modifier::control;
            fixture.pointer(gf::PointerAction::down, 60, 45, gf::PointerButton::primary, ctrl);
            if (!drag) {
                fixture.pointer(gf::PointerAction::up, 60, 45, gf::PointerButton::primary, ctrl);
            }
            fixture.pointer(gf::PointerAction::move, 72, 61, gf::PointerButton::none, ctrl);
            if (!drag) {
                fixture.pointer(gf::PointerAction::down, 72, 61, gf::PointerButton::primary, ctrl);
            }
            fixture.pointer(gf::PointerAction::up, 72, 61, gf::PointerButton::primary, ctrl);
            paint::Image expected;
            expected.reset(128, 96);
            const bool radial = shape == paint::Shape::Star5 || shape == paint::Shape::Hexagon;
            const paint::Point first = radial ? paint::Point{40, 25} : paint::Point{48, 29};
            const paint::Point last = radial ? paint::Point{80, 65} : paint::Point{72, 61};
            const paint::Ink body = editor.document.body_ink();
            paint::draw_shape(expected, shape, first, last, editor.document.primary_ink(), true, true,
                              body.brush, &body);
            require(std::memcmp(expected.pixels.data(), editor.document.image.pixels.data(),
                                expected.pixels.size() * sizeof(paint::Color)) == 0,
                    "Ctrl closed geometry grows around its first click in both gesture modes");
        }
    }
}
void centered_circle_and_materials() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.choose_shape(paint::Shape::Circle);
    editor.document.ink.size = 1;
    fixture.pointer(gf::PointerAction::down, 60, 45, gf::PointerButton::primary, gf::Modifier::control);
    fixture.pointer(gf::PointerAction::move, 72, 61, gf::PointerButton::primary, gf::Modifier::control);
    require(!display_white(editor, 40, 45) && !display_white(editor, 80, 45) &&
                !display_white(editor, 60, 25) && !display_white(editor, 60, 65) &&
                white(editor.document.image.get(40, 45)),
            "Ctrl circle previews radius equal to cursor distance around the initial center");
    fixture.pointer(gf::PointerAction::up, 72, 61, gf::PointerButton::primary, gf::Modifier::control);
    require(!white(editor.document.image.get(40, 45)) && !white(editor.document.image.get(80, 45)),
            "Ctrl circle commit matches centered preview");
    paint::select_brush(editor.document.ink, paint::Brush::Crayon);
    editor.refresh();
    routed_button(window, "secondary");
    open_tab(window, "patterns-tab");
    require((*window.find("material-brush-4")).visible() && (*window.find("r-pattern-12")).visible() &&
                (*window.find("grain-scale")).visible(),
            "fill brushes, patterns and material settings share one pane");
    routed_button(window, "material-brush-4");
    require(editor.document.alt_ink.brush == paint::Brush::Oil && !editor.document.shape_fill &&
                editor.document.ink.brush == paint::Brush::Crayon,
            "fill brush changes independently from the line");
    routed_button(window, "material-edge");
    routed_button(window, "material-brush-5");
    require(editor.document.ink.brush == paint::Brush::Crayon &&
                editor.document.shape_fill_brush == paint::Brush::Oil,
            "line brush changes independently from fill");
    routed_button(window, "smooth-lines");
    require(!editor.document.ink.smooth, "Smooth lines is a visible working toggle in Materials");
    open_tab(window, "tool-tab");
    require((*window.find("tool-size")).visible() && (*window.find("outline")).visible() &&
                (*window.find("fill")).visible() && (*window.find("smooth-lines")).visible() &&
                !(*window.find("grain-scale")).visible() && !(*window.find("grain-angle")).visible() &&
                !(*window.find("new-grain")).visible() && !window.find("edge-medium") &&
                !window.find("fill-medium"),
            "shape tools contain only tool settings and use shared Primary and Alt materials");
    open_tab(window, "patterns-tab");
    require((*window.find("grain-angle")).visible() && (*window.find("new-grain")).visible(),
            "all texture settings are available together in Materials");
}
void await_carpet(Fixture& fixture) {
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!(*(*fixture.window).find("dialog-ok")).enabled()) {
        static_cast<void>((*fixture.window).drain_posted_work());
        require(std::chrono::steady_clock::now() < deadline, "carpet render finishes on the UI dispatcher");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
void dither_dialog_and_brush() {
    Fixture fixture;
    paint::forms::Editor& editor=*fixture.editor;
    gf::Window& window=*fixture.window;
    for(int y=0;y<96;++y) for(int x=0;x<128;++x) editor.document.image.set(x,y,{static_cast<std::uint8_t>(x*2),static_cast<std::uint8_t>(y*2),110,255});
    editor.document.select({16,16,80,60});
    editor.document.settle_selection();
    paint::Image before=editor.document.image;
    std::size_t history=editor.document.undo_history.size();
    editor.execute("selection-dither");window.perform_layout();
    require(window.find("dither-preview")!=nullptr,"Dither command opens preview dialog");
    const std::shared_ptr<gf::NumericUpDown> count=std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("dither-colors"));
    const std::shared_ptr<gf::ComboBox> pattern=std::dynamic_pointer_cast<gf::ComboBox>(window.find("dither-pattern"));
    (*count).set_value(8);(*pattern).set_selected_index(3);await_carpet(fixture);
    require(editor.document.undo_history.size()==history && std::memcmp(before.pixels.data(),editor.document.image.pixels.data(),before.pixels.size()*sizeof(paint::Color))==0,"Dither preview does not edit document or history");
    routed_button(window,"dialog-ok");
    require(editor.document.undo_history.size()==history+1 && editor.dither_options.pattern==paint::DitherPattern::Drift,"Dither applies once and remembers pattern");
    require(editor.document.selection.active && editor.document.selection.canvas_selection,"Dither retains selection");
    for(int y=0;y<96;++y) for(int x=0;x<128;++x) if(!editor.document.selection.contains(x,y)) require(paint::equal(before.get(x,y),editor.document.image.get(x,y)),"Dither preserves pixels outside selection");
    editor.execute("undo");
    require(std::memcmp(before.pixels.data(),editor.document.image.pixels.data(),before.pixels.size()*sizeof(paint::Color))==0,"Dither dialog edit undoes exactly");
    editor.execute("selection-dither");window.perform_layout();routed_button(window,"dialog-cancel");
    static_cast<void>(window.drain_posted_work());
    require(!window.find("editor-dialog"),"Dither cancellation joins worker and dismisses dialog");
    open_tab(window,"home-tab");window.perform_layout();
    const gf::Rect bounds=(*window.find("brush-menu")).absolute_bounds();
    const gf::Point arrow{bounds.x+bounds.width/2,bounds.bottom()-5};
    window.dispatch_pointer({gf::PointerAction::down,gf::PointerButton::primary,arrow});
    window.dispatch_pointer({gf::PointerAction::up,gf::PointerButton::primary,arrow});window.perform_layout();
    routed_button(window,"popup-family-dither");
    require(editor.brush_family==paint::BrushFamily::Dither,"Brush menu selects dithering family");
    require((*window.find("dither-brush-noise")).visible(),"Dither brush exposes its noise mode");
    routed_button(window,"dither-brush-noise");
    require(editor.dither_brush_mode==paint::DitherBrushMode::Noise,"Dither noise option routes");
    editor.document.ink.size=32;
    fixture.drag(0,40,120,40);
    require(std::memcmp(before.pixels.data(),editor.document.image.pixels.data(),before.pixels.size()*sizeof(paint::Color))!=0,"Routed dither brush edits existing pixels");
    for(int y=0;y<96;++y) for(int x=0;x<128;++x) if(!editor.document.selection.contains(x,y)) require(paint::equal(before.get(x,y),editor.document.image.get(x,y)),"Dither brush obeys persistent selection");
    editor.execute("undo");
    require(std::memcmp(before.pixels.data(),editor.document.image.pixels.data(),before.pixels.size()*sizeof(paint::Color))==0,"Dither brush stroke undoes once");
}
void carpet_generator_controls() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    require(!window.find("stroke-state-tracker"), "archived tracker controls remain in shipping UI");
    editor.open_editor_dialog(paint::forms::EditorDialogKind::carpet);
    window.perform_layout();
    await_carpet(fixture);
    require(window.find("carpet-slider-9") && window.find("carpet-hillshade"), "carpet controls missing");
    const std::shared_ptr<gf::TextBox> dye =
        std::dynamic_pointer_cast<gf::TextBox>(window.find("carpet-color"));
    (*dye).set_text("#12");
    static_cast<void>(window.drain_posted_work());
    require(!(*window.find("dialog-ok")).enabled(), "invalid dye text permits accepting stale texture");
    (*dye).set_text("#1645C5");
    await_carpet(fixture);
    const std::shared_ptr<gf::TrackBar> tilt =
        std::dynamic_pointer_cast<gf::TrackBar>(window.find("carpet-slider-8"));
    (*tilt).set_value(30);
    (*tilt).set_value(35);
    await_carpet(fixture);
    routed_button(window, "carpet-hillshade");
    await_carpet(fixture);
    routed_button(window, "dialog-ok");
    require(editor.brush_family == paint::BrushFamily::Carpet && editor.carpet_parameters.view == 35 &&
                editor.carpet_parameters.hillshade && editor.carpet_tile.width == 512,
            "carpet controls not applied");
    editor.choose_tool(paint::Tool::Brush);
    fixture.drag(20, 40, 100, 40);
    require(!white(editor.document.image.get(50, 40)), "carpet brush paints no texture");
    editor.execute("undo");
    require(white(editor.document.image.get(50, 40)), "carpet stroke does not undo as one gesture");
    editor.open_editor_dialog(paint::forms::EditorDialogKind::carpet);
    window.perform_layout();
    // Closing an active worker must cancel/join without publishing to a dead dialog.
    routed_button(window, "dialog-cancel");
    require(editor.carpet_parameters.view == 35, "cancel changes committed carpet parameters");
}
void spirograph_apparatus_and_ink() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.document.new_image(640, 480);
    editor.refresh();
    const gf::Rect menu = (*window.find("tool-9")).absolute_bounds();
    const gf::Point arrow{menu.x + menu.width / 2, menu.bottom() - 4};
    window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, arrow});
    window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::primary, arrow});
    routed_button(window, "popup-spirograph");
    require(editor.spiro.active && editor.document.tool == paint::Tool::Spirograph,
            "graph menu activates spirograph");
    require(window.find("spiro-peg-1") && (*window.find("spiro-peg-1")).visible(),
            "spirograph peg tray missing");
    editor.execute("fit");
    editor.settings.rotate_view = true;
    editor.rotate_view(.35);
    const std::uint64_t revision = editor.document.revision;
    const std::vector<paint::Color> blank = editor.document.image.pixels;
    for (int h : {0, 2}) {
        const gf::Rect tray = (*window.find(h == 0 ? "spiro-peg-1" : "spiro-peg-4")).absolute_bounds();
        const gf::Point from{tray.x + tray.width / 2, tray.y + tray.height / 2};
        const paint::Point hole = editor.spiro.hole(h, editor.spiro.angle);
        const gf::Point to = fixture.position(hole.x, hole.y);
        window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, from});
        window.dispatch_pointer({gf::PointerAction::move, gf::PointerButton::primary, to});
        window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::primary, to});
        require(editor.spiro.pegs[h].seated && !editor.spiro.pegs[h].loaded,
                "tray drag fails to seat an empty peg");
    }
    editor.document.ink.primary = {200, 30, 40, 255};
    editor.document.ink.secondary = {20, 70, 190, 255};
    routed_button(window, "spiro-fill");
    paint::Point hole = editor.spiro.hole(0, editor.spiro.angle);
    fixture.click(hole.x, hole.y);
    hole = editor.spiro.hole(2, editor.spiro.angle);
    fixture.pointer(gf::PointerAction::down, hole.x, hole.y, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::up, hole.x, hole.y, gf::PointerButton::secondary);
    require(editor.spiro.pegs[0].loaded && editor.spiro.pegs[2].loaded && editor.spiro.pegs[2].ink.b == 190,
            "Fill loads Primary and Alt into pegs");
    require(editor.document.revision == revision &&
                std::memcmp(blank.data(), editor.document.image.pixels.data(),
                            blank.size() * sizeof(paint::Color)) == 0,
            "loading pegs floods the canvas or changes its history");
    routed_button(window, "spiro-operate");
    hole = editor.spiro.hole(0, editor.spiro.angle);
    fixture.click(hole.x, hole.y);
    require(editor.spiro.selected_peg == 0 && editor.spiro.pegs[0].seated,
            "clicking a peg moves it instead of selecting it");
    const gf::Rect medium = (*window.find("spiro-brushes-menu")).absolute_bounds();
    window.dispatch_pointer(
        {gf::PointerAction::down, gf::PointerButton::primary, {medium.x + 8, medium.y + 8}});
    window.dispatch_pointer(
        {gf::PointerAction::up, gf::PointerButton::primary, {medium.x + 8, medium.y + 8}});
    routed_button(window, "popup-peg-brush-12");
    require(editor.spiro.pegs[0].effect.brush == paint::Brush::Gel &&
                editor.spiro.pegs[2].effect.brush == paint::Brush::Round && editor.spiro.pegs[0].ink.r == 200,
            "peg gallery changes the wrong ink or brush");
    open_tab(window, "patterns-tab");
    routed_button(window, "material-brush-8");
    require(editor.spiro.pegs[0].effect.brush == paint::Brush::Watercolor,
            "ordinary material gallery does not target the highlighted peg");
    routed_button(window, "material-brush-12");
    open_tab(window, "tool-tab");
    hole = editor.spiro.hole(0, editor.spiro.angle);
    const paint::Point grip = editor.spiro.wheel_center(editor.spiro.angle);
    fixture.drag(hole.x, hole.y, grip.x, grip.y);
    require(editor.spiro.pegs[0].loaded, "invalid peg move inside wheel loses the original peg");
    fixture.pointer(gf::PointerAction::down, hole.x, hole.y);
    fixture.pointer(gf::PointerAction::move, 20, 20);
    window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::escape});
    require(editor.spiro.pegs[0].loaded && !editor.canvas().has_pointer_capture(),
            "Escape fails to restore a moved peg");
    const gf::Rect tray = (*window.find("spiro-peg-1")).absolute_bounds();
    window.dispatch_pointer({gf::PointerAction::down,
                             gf::PointerButton::primary,
                             {tray.x + tray.width / 2, tray.y + tray.height / 2}});
    window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::escape});
    require(!(*window.find("ribbon")).has_pointer_capture(), "Escape leaves a tray peg captured");
    const paint::Point old = editor.spiro.center;
    const double r = editor.spiro.guide_radius() + 7 * editor.spiro.scale;
    fixture.drag(old.x - r, old.y, old.x - r + 17, old.y + 12);
    require(std::abs(editor.spiro.center.x - old.x - 17) < 1e-7 &&
                std::abs(editor.spiro.center.y - old.y - 12) < 1e-7,
            "guide movement does not carry the assembly");
    require(editor.document.revision == revision, "moving support creates an ink history entry");
    editor.document.select({390, 0, 200, 480});
    const paint::Point wheel = editor.spiro.wheel_center(editor.spiro.angle);
    fixture.pointer(gf::PointerAction::down, wheel.x, wheel.y);
    for (int i = 1; i <= 50; ++i) {
        const paint::Point target = editor.spiro.wheel_center(i * .025);
        fixture.pointer(gf::PointerAction::move, target.x, target.y);
    }
    const paint::Point end = editor.spiro.wheel_center(1.25);
    fixture.pointer(gf::PointerAction::up, end.x, end.y);
    require(std::abs(editor.spiro.angle - 1.25) < 1e-6,
            "dragging the center does not operate the rolling insert");
    bool red = false, blue = false;
    for (paint::Color c : editor.document.image.pixels) {
        red = red || (c.r > c.b + 50);
        blue = blue || (c.b > c.r + 50);
    }
    require(red && blue, "loaded pegs do not draw both colors");
    for (int y = 0; y < 480; ++y) {
        for (int x = 0; x < 390; ++x) {
            require(white(editor.document.image.get(x, y)),
                    "spirograph ink escapes active selection in rotated view");
        }
    }
    const std::vector<paint::Color> drawn = editor.document.image.pixels;
    editor.execute("undo");
    require(std::memcmp(blank.data(), editor.document.image.pixels.data(),
                        blank.size() * sizeof(paint::Color)) == 0,
            "multi-peg gesture is not one undo step");
    editor.execute("redo");
    require(std::memcmp(drawn.data(), editor.document.image.pixels.data(),
                        drawn.size() * sizeof(paint::Color)) == 0,
            "spirograph redo differs from original ink");
    routed_button(window, "spiro-remove");
    require(!editor.spiro.inserted && !editor.spiro.pegs[0].seated, "removing insert retains pegs");
    const paint::Point close = editor.spiro.close_position();
    fixture.click(close.x, close.y);
    require(!editor.spiro.active && std::memcmp(drawn.data(), editor.document.image.pixels.data(),
                                                drawn.size() * sizeof(paint::Color)) == 0,
            "closing apparatus erases its finished drawing");
}
void spirograph_guide_dismissal() {
    for (const paint::Tool tool : {paint::Tool::Select, paint::Tool::Lasso, paint::Tool::Path,
                                   paint::Tool::Shape, paint::Tool::Text, paint::Tool::Reshape,
                                   paint::Tool::Pencil, paint::Tool::Brush, paint::Tool::Fill,
                                   paint::Tool::Eraser, paint::Tool::Stamp, paint::Tool::Picker,
                                   paint::Tool::Magnifier, paint::Tool::Freehand}) {
        Fixture fixture;
        paint::forms::Editor& editor = *fixture.editor;
        editor.document.image.set(40, 40, {21, 43, 68, 97});
        const paint::Image original = editor.document.image;
        const std::uint64_t revision = editor.document.revision;
        editor.guide.nodes = {{20, 20}, {80, 20}, {80, 70}, {20, 70}};
        editor.guide.closed = true;
        editor.guide.rebuild_boundary();
        editor.start_spirograph();
        paint::SpiroPeg peg;
        peg.seated = true;
        editor.spiro.seat(0, peg);
        editor.spiro.fill(0, {20, 70, 190, 255});
        editor.spiro.select_peg(0);
        editor.choose_tool(tool);
        const bool retained = tool == paint::Tool::Pencil || tool == paint::Tool::Brush ||
                              tool == paint::Tool::Fill || tool == paint::Tool::Eraser ||
                              tool == paint::Tool::Stamp || tool == paint::Tool::Picker ||
                              tool == paint::Tool::Magnifier || tool == paint::Tool::Freehand;
        require(editor.guide.active() == retained && editor.spiro.active == retained &&
                    editor.spiro.pegs[0].loaded == retained &&
                    editor.spiro.selected_peg == (retained ? 0 : -1),
                "tool switching gives spirograph a different lifetime from guides");
        require(editor.document.tool == tool && editor.document.revision == revision &&
                    std::equal(original.pixels.begin(), original.pixels.end(),
                               editor.document.image.pixels.begin(), paint::equal),
                "dismissing apparatus changes the selected tool, painted pixels, or history");
    }
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.start_spirograph();
    paint::SpiroPeg peg;
    peg.seated = true;
    editor.spiro.seat(0, peg);
    editor.spiro.fill(0, {20, 70, 190, 255});
    const paint::Point wheel = editor.spiro.wheel_center(0);
    fixture.pointer(gf::PointerAction::down, wheel.x, wheel.y);
    const paint::Point target = editor.spiro.wheel_center(.5);
    fixture.pointer(gf::PointerAction::move, target.x, target.y);
    const paint::Image drawn = editor.document.image;
    const std::uint64_t revision = editor.document.revision;
    require(editor.canvas().has_pointer_capture(), "wheel gesture did not capture pointer");
    editor.choose_shape(paint::Shape::Rectangle);
    require(!editor.spiro.active && !editor.canvas().has_pointer_capture() &&
                editor.document.tool == paint::Tool::Shape && editor.document.revision == revision &&
                std::equal(drawn.pixels.begin(), drawn.pixels.end(),
                           editor.document.image.pixels.begin(), paint::equal),
            "shape gallery leaves apparatus or capture behind, or erases an unfinished wheel stroke");
    editor.start_spirograph();
    require(!editor.spiro.pegs[0].seated && editor.spiro.selected_peg == -1,
            "reopening a dismissed apparatus resurrects its old pegs");
    editor.execute("cut");
    require(!editor.spiro.active && editor.document.tool == paint::Tool::Pencil &&
                editor.document.revision == revision &&
                std::equal(drawn.pixels.begin(), drawn.pixels.end(),
                           editor.document.image.pixels.begin(), paint::equal),
            "Cut on temporary apparatus cuts the canvas instead of dismissing the guide");
}
void independent_color_materials_and_no_color() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    routed_button(window, "secondary");
    routed_button(window, "swatch-6");
    require(paint::equal(editor.document.ink.secondary, paint::forms::ribbon_color(6)),
            "Pencil permits selecting Alt and assigning its color with solid Primary");
    routed_button(window, "primary");
    routed_button(window, "swatch-5");
    require((*window.find("secondary")).enabled() &&
                paint::equal(editor.document.ink.secondary, paint::forms::ribbon_color(6)),
            "choosing a Primary color preserves the selectable Alt color");
    editor.choose_tool(paint::Tool::Brush);
    open_tab(window, "patterns-tab");
    require(!window.find("material-brush-0") && !window.find("material-enabled"),
            "Solid is one material choice without a duplicate Brush or Enabled switch");
    require((*window.find("material-fill")).enabled() && (*window.find("alt-carries-body")).enabled(),
            "Primary Solid leaves Alt and body assignment available");
    routed_button(window, "material-brush-4");
    routed_button(window, "r-pattern-12");
    require(editor.document.ink.pattern == paint::Pattern::Checker &&
                editor.document.ink.brush == paint::Brush::Round &&
                !(*require_button(window, "material-brush-4")).selected(),
            "selecting a pattern clears the old brush and its gallery selection");
    routed_button(window, "material-brush-5", gf::PointerButton::secondary);
    require(editor.document.alt_ink.brush == paint::Brush::Crayon &&
                editor.document.alt_ink.pattern == paint::Pattern::Solid &&
                editor.document.ink.pattern == paint::Pattern::Checker,
            "Alt can carry a brush while Primary carries a pattern");
    routed_button(window, "r-pattern-8", gf::PointerButton::secondary);
    require(editor.document.alt_ink.brush == paint::Brush::Round &&
                editor.document.alt_ink.pattern == paint::Pattern::Horizontal,
            "selecting an Alt pattern clears its old brush");
    std::shared_ptr<gf::NumericUpDown> grain =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("grain-scale"));
    (*grain).set_value(2);
    require(editor.document.alt_ink.grain_scale == 2 && editor.document.ink.grain_scale == 1,
            "material controls edit the selected slot");
    open_tab(window, "home-tab");
    routed_button(window, "swatch-6");
    require(editor.document.alt_ink.pattern == paint::Pattern::Horizontal &&
                paint::equal(editor.document.ink.secondary, paint::forms::ribbon_color(6)),
            "changing color preserves the chosen material");
    open_tab(window, "patterns-tab");
    routed_button(window, "r-pattern-18");
    const paint::Ink primary = editor.document.primary_ink();
    require(editor.document.alt_ink.pattern == paint::Pattern::None && paint::patterned(primary, 5, 1).a == 0,
            "Alt No color leaves Primary pattern gaps transparent");
    routed_button(window, "r-pattern-0");
    editor.document.ink.primary = {170, 20, 40, 255};
    editor.document.ink.secondary = {20, 70, 180, 255};
    editor.choose_shape(paint::Shape::Rectangle);
    editor.document.shape_outline = false;
    editor.document.shape_fill = true;
    fixture.drag(10, 10, 60, 45);
    require(paint::equal(editor.document.image.get(20, 20), editor.document.ink.primary) &&
                paint::equal(editor.document.image.get(24, 20), editor.document.ink.secondary),
            "body uses Primary pattern and Alt gaps while body assignment is off");
    open_tab(window, "patterns-tab");
    routed_button(window, "alt-carries-body");
    require(editor.document.alt_carries_body, "Alt carries body is a working Materials toggle");
    fixture.drag(65, 10, 120, 45);
    require(paint::equal(editor.document.image.get(80, 20), editor.document.ink.secondary),
            "Alt carries body fills with Alt independently of Primary's pattern");
    routed_button(window, "material-edge");
    routed_button(window, "r-pattern-0");
    require(paint::solid_material(editor.document.ink) && (*window.find("material-fill")).enabled() &&
                (*window.find("alt-carries-body")).enabled(),
            "Solid clears Primary texture without disabling Alt or body assignment");
    fixture.drag(10, 50, 110, 85);
    require(paint::equal(editor.document.image.get(40, 65), editor.document.ink.secondary),
            "Alt still carries the shape body when Primary is solid");
    const std::size_t history = editor.document.undo_history.size();
    fixture.pointer(gf::PointerAction::down, 10, 90, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::up, 110, 90, gf::PointerButton::secondary);
    require(editor.document.undo_history.size() == history + 1,
            "right drawing with solid Primary creates one undo transaction");
}

void ribbon_collapse_and_reopen() {
    Fixture fixture;
    gf::Window& window = *fixture.window;
    paint::forms::Editor& editor = *fixture.editor;
    routed_button(window, "home-tab");
    window.perform_layout();
    require(editor.canvas().absolute_bounds().y == 27 && !(*window.find("paste")).visible() &&
                (*window.find("view-tab")).visible(),
            "active tab collapses the ribbon and gives its space to canvas");
    routed_button(window, "home-tab");
    window.perform_layout();
    require(editor.canvas().absolute_bounds().y == 143 && (*window.find("paste")).visible(),
            "clicking the active tab again expands its controls");
    routed_button(window, "home-tab");
    routed_button(window, "view-tab");
    window.perform_layout();
    require(editor.canvas().absolute_bounds().y == 143 && (*window.find("show-rulers")).visible(),
            "a different tab expands the ribbon onto that page");
    routed_button(window, "show-rulers");
    routed_button(window, "view-tab");
    window.perform_layout();
    require(editor.canvas().absolute_bounds().y == 47 && editor.canvas().absolute_bounds().x == 20,
            "rulers follow the collapsed tab row");
    routed_button(window, "tool-tab");
    window.perform_layout();
    require(editor.canvas().absolute_bounds().y == 163 && (*window.find("tool-size")).visible(),
            "tool tab expands with correct ruler offset");
}
void about_reports_build_version() {
    Fixture fixture;
    gf::Window& window = *fixture.window;
    TestServices services;
    gf::HostSession session(window, service_capabilities(), &services);
    (*fixture.editor).execute("about");
    window.perform_layout();
    const std::shared_ptr<gf::Label> version =
        std::dynamic_pointer_cast<gf::Label>(window.find("about-version"));
    require(version && (*version).text() == "Rainstar Paint " RAINSTAR_VERSION && services.message.empty(),
            "About reports the release version inside the application's own dialog");
    require((*window.find("about-dedication")).absolute_bounds().bottom() <
                (*window.find("about-credits")).absolute_bounds().y,
            "About dedication precedes the credits");
    routed_button(window, "dialog-ok");
    require(!window.find("editor-dialog"), "Close dismisses the in-house About dialog");
    (*fixture.editor).execute("about");
    gf::KeyEvent enter;
    enter.action = gf::KeyAction::down;
    enter.physical_key = gf::PhysicalKey::enter;
    window.dispatch_key(enter);
    require(!window.find("editor-dialog"), "Enter closes About without applying editor settings");
}
void ribbon_preserves_artwork_position() {
    Fixture fixture;
    gf::Window& window = *fixture.window;
    paint::forms::Editor& editor = *fixture.editor;
    for (double zoom : {0.25, 1.0, 3.75, 16.0}) {
        for (bool rulers : {false, true}) {
            open_tab(window, "home-tab");
            editor.show_rulers = rulers;
            window.perform_layout();
            editor.canvas().set_view(zoom, {-42.5, 67.25});
            const gf::Rect before = editor.canvas().bitmap_to_client({85, 90, 1, 1});
            const double screen_y = before.y + editor.canvas().absolute_bounds().y;
            for (const char* tab : {"home-tab", "home-tab", "home-tab", "view-tab"}) {
                routed_button(window, tab);
                window.perform_layout();
                const gf::Rect after = editor.canvas().bitmap_to_client({85, 90, 1, 1});
                require(std::abs(after.y + editor.canvas().absolute_bounds().y - screen_y) < 0.000001 &&
                            after.x == before.x && editor.canvas().zoom() == zoom,
                        "ribbon collapse, reopening and tab changes preserve the artwork's screen position");
            }
        }
    }
}
void select_all_delete_without_drag() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.document.image.reset(128, 96, {30, 80, 160, 255});
    editor.refresh();
    routed_button(window, "shape-3");
    require(window.focused_control() != window.find("canvas"), "ribbon retains focus before Select all");
    gf::KeyEvent select{gf::KeyAction::down, gf::PhysicalKey::a};
    select.modifiers = gf::Modifier::control;
    require(window.dispatch_key(select), "Ctrl+A selects artwork from ribbon focus");
    require(editor.document.selection.active && window.focused_control() == window.find("canvas"),
            "Select all transfers keyboard focus to the selected artwork");
    require(window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::delete_forward}),
            "Delete works immediately after Select all without a pointer gesture");
    require(!editor.document.selection.active && white(editor.document.image.get(64, 48)),
            "Select all Delete clears the picture");
    editor.execute("undo");
    require(paint::equal(editor.document.image.get(64, 48), {30, 80, 160, 255}),
            "clearing all artwork remains undoable");
}
void crop_and_help_actions() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    std::size_t undo = editor.document.undo_history.size();
    routed_button(window, "crop");
    require(editor.document.tool == paint::Tool::Select && editor.document.image.width == 128 &&
                editor.document.undo_history.size() == undo,
            "Crop without a selection activates Selection without editing the image");
    fixture.drag(20, 20, 60, 50);
    routed_button(window, "context-crop");
    require(editor.document.image.width == 40 && editor.document.image.height == 30,
            "Crop with a selection crops normally");
    gf::KeyEvent help{gf::KeyAction::down, gf::PhysicalKey::f1};
    static_cast<void>(window.dispatch_key(help));
    require((*require_button(window, "help")).selected(), "F1 highlights the Help button");
    routed_button(window, "help-close");
    require(!editor.show_help && !(*require_button(window, "help")).selected(),
            "visible Close Help button closes the book and clears the highlight");
    routed_button(window, "help");
    require(editor.show_help && (*require_button(window, "help")).selected(),
            "Help button opens and highlights itself");
    routed_button(window, "help");
    require(!editor.show_help, "selected Help button closes the book");
}
void stamp_reset_and_recapture() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.image.set(20, 20, {231, 45, 60, 255});
    editor.document.image.set(60, 20, {20, 150, 90, 255});
    editor.choose_tool(paint::Tool::Stamp);
    editor.stamp_width = editor.stamp_height = 12;
    editor.document.stamp_shape = paint::StampShape::Square;
    fixture.click(20, 20);
    require(!editor.background_busy(), "unaltered stamp is immediately ready without a transform job");
    fixture.click(20, 60);
    require(paint::equal(editor.document.image.get(20, 60), {231, 45, 60, 255}),
            "unaltered stamp preserves its exact source pixels");
    editor.stamp_angle = 37;
    editor.regenerate_stamp();
    fixture.pointer(gf::PointerAction::down, 20, 60, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::up, 20, 60, gf::PointerButton::secondary);
    require(editor.document.stamp.pixels.empty() && editor.stamp_angle == 0 && editor.stamp_scale == 1,
            "right click clears capture and transform state even during background work");
    fixture.click(60, 20);
    fixture.click(90, 60);
    require(paint::equal(editor.document.image.get(90, 60), {20, 150, 90, 255}),
            "new stamp can be captured and placed immediately after right click");
    await_background(fixture);
    fixture.click(60, 60);
    require(paint::equal(editor.document.image.get(60, 60), {20, 150, 90, 255}),
            "stale transform completion cannot replace the newly captured stamp");
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
void freehand_wand_picker_and_size() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.choose_tool(paint::Tool::Freehand);
    editor.document.shape_fill = true;
    editor.document.shape_outline = false;
    editor.document.ink.primary = {34, 126, 55, 255};
    editor.document.ink.secondary = {220, 80, 24, 255};
    editor.document.alt_carries_body = true;
    fixture.pointer(gf::PointerAction::down, 20, 20);
    fixture.pointer(gf::PointerAction::move, 75, 20);
    fixture.pointer(gf::PointerAction::move, 55, 70);
    fixture.pointer(gf::PointerAction::up, 20, 20);
    require(paint::equal(editor.document.image.get(45, 35), editor.document.ink.secondary),
            "freehand closes on release and uses alternate body material");
    require(white(editor.document.image.get(90, 70)), "freehand leaves outside untouched");
    editor.execute("undo");
    require(white(editor.document.image.get(45, 35)), "one undo removes a freehand body");
    editor.document.image.set(20, 20, {34, 126, 55, 255});
    editor.document.image.set(100, 70, {34, 126, 55, 255});
    editor.refresh();
    editor.lasso_mode = paint::LassoMode::Wand;
    editor.choose_tool(paint::Tool::Lasso);
    fixture.click(20, 20);
    require(editor.document.selection.active && editor.document.selection.image.width == 81 &&
                editor.document.selection.image.height == 51,
            "wand includes disconnected equal colors");
    editor.choose_tool(paint::Tool::Pencil);
    editor.document.ink.primary = {99, 11, 190, 255};
    fixture.drag(20, 20, 100, 70);
    require(paint::equal(editor.document.image.get(100, 70), {99, 11, 190, 255}) &&
                white(editor.document.image.get(60, 45)),
            "wand remains a painting mask between disconnected islands");
    editor.document.commit_selection();
    editor.choose_tool(paint::Tool::Picker);
    fixture.click(100, 70);
    require(paint::equal(editor.document.ink.primary, {99, 11, 190, 255}), "picker click samples primary");
    const paint::Color before = editor.document.ink.primary;
    const gui_drawing::PointF origin = editor.canvas().view_origin();
    fixture.pointer(gf::PointerAction::down, 40, 40);
    const gf::Point start = fixture.position(40, 40);
    window.dispatch_pointer(
        {gf::PointerAction::move, gf::PointerButton::primary, {start.x + 25, start.y + 10}});
    window.dispatch_pointer(
        {gf::PointerAction::up, gf::PointerButton::primary, {start.x + 25, start.y + 10}});
    require(paint::equal(before, editor.document.ink.primary) &&
                editor.canvas().view_origin().x == origin.x - 25,
            "picker drag pans without sampling a new color");
    const gui_drawing::PointF panned = editor.canvas().view_origin();
    window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::right});
    require(editor.canvas().view_origin().x == panned.x + 32, "right arrow pans screen pixels");
    routed_button(window, "status-tool-size");
    const std::shared_ptr<gf::NumericUpDown> size =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("custom-tool-size"));
    require(static_cast<bool>(size), "status size opens custom size popup");
    (*size).set_value(173);
    routed_button(window, "dialog-ok");
    require(editor.document.ink.size == 173, "custom size exceeds the old 64 pixel limit");
}
void working_view_coordinates_and_reset() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    require(!editor.settings.rotate_view, "rotation handle defaults off");
    editor.settings.rotate_view = true;
    editor.refresh();
    editor.execute("fit");
    const std::vector<paint::Color> before = editor.document.image.pixels;
    const std::uint64_t revision = editor.document.revision;
    for (double angle : {0.2, 1.5707963267948966, -1.9, 3.141592653589793}) {
        editor.rotate_view(angle);
        const gf::Point position = fixture.position(30.25, 45.75);
        const gui_drawing::PointF actual =
            editor.canvas().client_to_bitmap(editor.canvas().point_from_window(position));
        require(std::abs(actual.x - 30.25) < 1e-9 && std::abs(actual.y - 45.75) < 1e-9,
                "arbitrary view rotations preserve inverse pointer mapping");
    }
    require(std::memcmp(before.data(), editor.document.image.pixels.data(),
                        before.size() * sizeof(paint::Color)) == 0 &&
                editor.document.revision == revision,
            "rotating the view never edits pixels or history");
    editor.rotate_view(0);
    const gf::Point grip = editor.canvas().point_to_window(editor.canvas().rotation_handle());
    const gf::Point pivot =
        fixture.position(editor.document.image.width / 2.0, editor.document.image.height / 2.0);
    const double dx = grip.x - pivot.x, dy = grip.y - pivot.y;
    const gf::Point turned{pivot.x + std::cos(0.3) * dx - std::sin(0.3) * dy,
                           pivot.y + std::sin(0.3) * dx + std::cos(0.3) * dy};
    window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, grip});
    require(editor.canvas().has_pointer_capture(), "rotation grip captures the drag");
    window.dispatch_pointer({gf::PointerAction::move, gf::PointerButton::primary, turned});
    window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::primary, turned});
    require(std::abs(editor.canvas().view_angle - 0.3) < 1e-9 && !editor.canvas().has_pointer_capture() &&
                editor.document.revision == revision,
            "drag rotates only the working view and releases capture");
    editor.rotate_view(0.55);
    editor.choose_tool(paint::Tool::Pencil);
    fixture.drag(25.5, 25.5, 70.5, 45.5);
    require(!white(editor.document.image.get(25, 25)) && !white(editor.document.image.get(70, 45)),
            "painting in rotated view writes the original document coordinates");
    const std::vector<paint::Color> painted = editor.document.image.pixels;
    const std::uint64_t painted_revision = editor.document.revision;
    const gf::Point handle = editor.canvas().point_to_window(editor.canvas().rotation_handle());
    window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::secondary, handle});
    window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::secondary, handle});
    require(editor.canvas().view_angle == 0 && editor.document.revision == painted_revision &&
                std::memcmp(painted.data(), editor.document.image.pixels.data(),
                            painted.size() * sizeof(paint::Color)) == 0,
            "right-click grip restores the unresampled canvas without an undo entry");
    paint::Image pasted;
    pasted.reset(8, 6, {240, 100, 30, 255});
    editor.document.paste(pasted, 45, 30);
    editor.rotate_view(-0.4);
    const std::uint64_t floating_revision = editor.document.revision;
    const gf::Point reset = editor.canvas().point_to_window(editor.canvas().rotation_handle());
    window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::secondary, reset});
    window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::secondary, reset});
    require(editor.canvas().view_angle == 0 && editor.document.selection.active &&
                editor.document.selection.x == 45 && editor.document.selection.y == 30 &&
                editor.document.revision == floating_revision &&
                paint::equal(editor.document.selection.image.get(0, 0), pasted.get(0, 0)),
            "right-click view reset preserves floating pixels, placement and history");
}
void rotated_frames_use_published_images() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.settings.rotate_view = true;
    editor.document.image.set(35, 35, {20, 70, 110, 255});
    editor.rotate_view(0.35);
    editor.execute("fit");
    PreviewPainter warm;
    static_cast<void>(window.paint(warm));
    for (int step = 0; step < 8; ++step) {
        editor.rotate_view(0.35 + step * 0.08);
        if (step == 3) {
            fixture.drag(25, 30, 80, 60);
        }
        if (step == 5) {
            editor.choose_tool(paint::Tool::Stamp);
            fixture.click(35, 35);
        }
        fixture.pointer(gf::PointerAction::move, 60 + step, 55);
        static_cast<void>(window.poll_frame_schedule(gf::FrameClock::now() + std::chrono::milliseconds(20)));
        // The native host snapshots its image registry before Window::paint.
        const std::vector<gf::ImageId> available = window.image_resources().image_ids();
        const std::uint64_t revision = window.image_resources().snapshot().revision;
        PreviewPainter frame;
        editor.canvas().on_paint(frame, editor.canvas().client_rectangle());
        editor.paint_canvas_overlay(frame, {});
        require(window.image_resources().snapshot().revision == revision,
                "rotation and stamp painting must not replace resources after native synchronization");
        require(!frame.painted_images.empty(), "every rotated frame contains a published canvas image");
        for (gf::ImageId image : frame.painted_images) {
            require(std::find(available.begin(), available.end(), image) != available.end(),
                    "every drawn image was available before the frame started");
        }
    }
    await_background(fixture);
    const std::uint64_t revision = window.image_resources().snapshot().revision;
    PreviewPainter prepared;
    editor.canvas().on_paint(prepared, editor.canvas().client_rectangle());
    editor.paint_canvas_overlay(prepared, {});
    require(window.image_resources().snapshot().revision == revision,
            "worker completion publishes the prepared image before painting");
}
void drain_canvas_work(Fixture& fixture) {
    const gf::FrameTime deadline = gf::FrameClock::now() + std::chrono::seconds(10);
    do {
        static_cast<void>((*fixture.window).drain_posted_work());
        static_cast<void>((*fixture.window).poll_frame_schedule(gf::FrameClock::now()));
        require(gf::FrameClock::now() < deadline, "canvas work settles without an idle render loop");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while ((*fixture.editor).canvas().view_busy() || (*fixture.editor).background_busy());
}
void canvas_work_is_coalesced_and_stamp_is_cached() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.settings.rotate_view = true;
    editor.document.image.set(35, 35, {20, 70, 110, 255});
    editor.rotate_view(0.35);
    editor.execute("fit");
    drain_canvas_work(fixture);
    const paint::forms::CanvasWorkStatistics initial = editor.canvas().work_statistics();
    for (int i = 0; i < 120; ++i) {
        editor.rotate_view(0.35 + i * 0.001);
    }
    require(editor.canvas().work_statistics().view_renders == initial.view_renders,
            "a burst of rotation events does not render between display deadlines");
    drain_canvas_work(fixture);
    require(editor.canvas().work_statistics().view_renders == initial.view_renders + 1 &&
                editor.canvas().work_statistics().view_preparations == initial.view_preparations,
            "120 angle updates share one render and the same CONV field");
    const std::uint64_t preparations = editor.canvas().work_statistics().view_preparations;
    fixture.pointer(gf::PointerAction::down, 20, 25);
    for (int i = 0; i < 80; ++i) {
        fixture.pointer(gf::PointerAction::move, 20 + i, 25 + i * 0.3);
    }
    require(editor.canvas().work_statistics().view_preparations == preparations,
            "moving stroke does not repeatedly prepare obsolete source images");
    fixture.pointer(gf::PointerAction::up, 99, 48.7);
    drain_canvas_work(fixture);
    require(editor.canvas().work_statistics().view_preparations == preparations + 1,
            "release prepares the latest completed stroke exactly once");
    require(!white(editor.document.image.get(99, 48)), "batched rendering preserves the stroke endpoint");
    editor.choose_tool(paint::Tool::Stamp);
    fixture.click(35, 35);
    drain_canvas_work(fixture);
    const std::uint64_t stamp_preparations = editor.canvas().work_statistics().stamp_preparations;
    fixture.click(65, 55);
    drain_canvas_work(fixture);
    fixture.click(70, 55);
    drain_canvas_work(fixture);
    require(editor.canvas().work_statistics().stamp_preparations == stamp_preparations,
            "stamp preparation survives repeated placement and mouse-up");
    const std::uint64_t stamp_renders = editor.canvas().work_statistics().stamp_renders;
    for (int i = 0; i < 50; ++i) {
        fixture.pointer(gf::PointerAction::move, 70, 55);
    }
    drain_canvas_work(fixture);
    require(editor.canvas().work_statistics().stamp_renders == stamp_renders,
            "unchanged stamp preview skips both resampling and upload");
    editor.stamp_hardness = 0.4;
    editor.update_stamp_hardness();
    editor.prepare_stamp_view();
    drain_canvas_work(fixture);
    require(editor.canvas().work_statistics().stamp_preparations == stamp_preparations + 1,
            "new stamp pixels invalidate the prepared stamp even when its geometry is unchanged");
    const paint::forms::CanvasWorkStatistics idle = editor.canvas().work_statistics();
    static_cast<void>(window.poll_frame_schedule(gf::FrameClock::now() + std::chrono::seconds(1)));
    require(!editor.canvas().view_busy() &&
                editor.canvas().work_statistics().view_renders == idle.view_renders &&
                editor.canvas().work_statistics().stamp_renders == idle.stamp_renders,
            "settled canvas and stamp leave no recurring work timer");
    std::cout << "Canvas work: 120 rotations -> 1 render; 80 stroke moves -> 1 preparation; "
                 "stamp reuse and idle-work checks passed.\n";
}
void interface_colors_and_spacing() {
    const gf::Color royal = gf::Color::rgba(118, 159, 223);
    const gf::Color gold = gf::Color::rgba(255, 207, 102);
    require(paint::forms::interface_color(royal, 220) == royal, "royal blue is byte-exact");
    const paint::Lab reference = paint::to_oklab({118, 159, 223, 255});
    for (int hue = 0; hue < 360; ++hue) {
        const gf::Color color = paint::forms::interface_color(royal, hue);
        const paint::Lab lab = paint::to_oklab({color.red, color.green, color.blue, 255});
        require(std::abs(lab.l - reference.l) < 0.005, "theme wheel preserves perceptual lightness");
        require(paint::forms::interface_color(gold, hue) == gold, "selected tools remain golden at every hue");
    }
    Fixture fixture;
    gf::Window& window = *fixture.window;
    paint::forms::Editor& editor = *fixture.editor;
    const std::shared_ptr<const gf::Theme> source = (*window.find("save")).theme_override();
    for (int hue : {330, 280, 140, 220}) {
        editor.settings.interface_hue = hue;
        editor.refresh();
        window.perform_layout();
        const gf::Rect tools = (*window.find("tool-0")).absolute_bounds();
        const gf::Rect brushes = (*window.find("brush-menu")).absolute_bounds();
        require(tools.y == brushes.y, "tools top row aligns with the adjacent group");
        require((*window.find("tool-6")).absolute_bounds().right() < brushes.x,
                "last tool clears the divider and brush group");
    }
    require((*window.find("save")).theme_override() == source,
            "returning to royal blue restores authored recipes without cumulative recoloring");
}
void recovery_editor_contracts() {
    Fixture fixture;
    TestServices services;
    gf::HostSession host(*fixture.window, service_capabilities(), &services);
    require(host.dispatch({1, 0, gf::HostAttachEvent{{1180, 820}, 1}}).accepted(), "recovery fixture attaches");
    paint::forms::Editor& editor = *fixture.editor;
    const std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("rainstar-editor-recovery-" + std::to_string(paint::recovery_time_ms()));
    editor.recovery_root = root;
    editor.reset_recovery(false, "original.png");
    editor.document.new_image(24, 20);
    editor.document.checkpoint();
    editor.document.image.set(3, 4, {90, 40, 170, 255});
    editor.refresh();
    editor.recovery_tick();
    editor.settle_recovery();
    require(editor.recovery_session != nullptr, "dirty editor creates a snapshot");
    const std::string id = (*editor.recovery_session).id();
    require(paint::equal((*editor.recovery_session).read().image.frames[0].image.get(3, 4), {90, 40, 170, 255}),
            "automatic snapshot uses original-coordinate document pixels");
    const std::uint64_t generation = (*editor.recovery_session).read().generation;
    editor.recovery_deadline = {};
    editor.recovery_tick();
    require(!editor.recovery_job.valid() && (*editor.recovery_session).read().generation == generation,
            "unchanged idle document does not rewrite recovery");
    editor.document.paste(editor.document.image);
    editor.document.selection.x = 5;
    editor.refresh();
    editor.recovery_deadline = {};
    editor.recovery_tick(); editor.settle_recovery();
    require(paint::equal((*editor.recovery_session).read().image.frames[0].image.get(8, 4), {90, 40, 170, 255}) &&
                editor.document.selection.active, "floating artwork is captured without committing it");
    editor.settings.recovery_enabled = false;
    editor.refresh(); editor.recovery_deadline = {};
    editor.recovery_tick();
    require(!editor.recovery_job.valid(), "disabled recovery never queues new writes");
    editor.reset_recovery(false);
    editor.document.new_image(24, 20);
    services.choice = gf::HostDialogChoice::no;
    editor.recover_document();
    require(!editor.document.dirty() && paint::RecoverySession::candidates(root).size() == 1,
            "declining recovery retains artwork for later");
    services.choice = gf::HostDialogChoice::yes;
    editor.start_recovery("");
    require(editor.recovery_startup_pending && !editor.document.dirty(),
            "startup defers its recovery prompt until after native window initialization");
    editor.recovery_tick();
    require(!editor.recovery_startup_pending && editor.document.dirty() && editor.document.filename.empty() &&
                (*editor.recovery_session).id() == id && editor.recovery_metadata.opened_path == "original.png",
            "restoration retains identity and provenance but opens an unsaved copy");
    services.path = (root / "recovered.png").string();
    require(editor.save(false), "recovered document saves explicitly");
    editor.reset_recovery(false);
    require(paint::RecoverySession::candidates(root).empty(), "successful save retires recovery snapshots");
    std::filesystem::remove_all(root);
}
void scroll_distance_bounds_and_settings() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.document.new_image(1600, 1200);
    editor.refresh();
    window.perform_layout();
    gf::RasterCanvas& canvas = editor.canvas();
    const gf::Rect area = canvas.committed_arranged_bounds();
    const gf::Point position = canvas.point_to_window({area.width / 2, area.height / 2});
    gf::PointerEvent event{gf::PointerAction::wheel, gf::PointerButton::none, position};
    for (double scale : {0.5, 1.0, 4.0}) {
        canvas.set_zoom(scale);
        canvas.set_view_origin({800 - area.width / (2 * scale), 600 - area.height / (2 * scale)});
        const gui_drawing::PointF before = canvas.view_origin();
        event.wheel_delta = {2, -3};
        window.dispatch_pointer(event);
        require(std::abs((canvas.view_origin().x - before.x) * scale + 12) < 1e-9 &&
                    std::abs((canvas.view_origin().y - before.y) * scale - 18) < 1e-9,
                "two axis scrolling travels modest screen distances at every zoom");
        for (double direction : {-1.0, 1.0}) {
            event.wheel_delta = {direction * 100000, direction * 100000};
            window.dispatch_pointer(event);
            const double center_x = canvas.view_origin().x + area.width / (2 * scale);
            const double center_y = canvas.view_origin().y + area.height / (2 * scale);
            require(std::abs(center_x - (direction < 0 ? 1600 : 0)) < 1e-9 &&
                        std::abs(center_y - (direction < 0 ? 1200 : 0)) < 1e-9,
                    "large diagonal wheel input stops the viewport center at both canvas edges");
        }
    }
    editor.settings.storage_path =
        (std::filesystem::temp_directory_path() / "rainstar-forms-settings-test.txt").string();
    editor.execute("settings");
    window.perform_layout();
    std::shared_ptr<gf::NumericUpDown> distance =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("settings-scroll"));
    require(distance && (*distance).value() == 6, "settings presents modest default scroll distance");
    std::shared_ptr<gf::TrackBar> hue = std::dynamic_pointer_cast<gf::TrackBar>(window.find("settings-theme-hue"));
    (*hue).set_value(330);
    require(editor.preview_interface_hue == 330 && editor.settings.interface_hue == 220,
            "theme preview is immediate but does not save before OK");
    (*distance).set_value(2.5);
    std::shared_ptr<gf::ComboBox> backing =
        std::dynamic_pointer_cast<gf::ComboBox>(window.find("settings-background"));
    (*backing).set_selected_index(static_cast<std::size_t>(paint::CanvasBacking::BrownFelt));
    std::shared_ptr<gf::ComboBox> gesture =
        std::dynamic_pointer_cast<gf::ComboBox>(window.find("settings-shape-gesture"));
    require(gesture && (*gesture).selected_index().value_or(0) == 1, "settings exposes active drag option");
    (*gesture).set_selected_index(0);
    routed_button(window, "dialog-cancel");
    require(editor.settings.drag_shapes && editor.settings.scroll_distance == 6 &&
                editor.settings.canvas_backing == paint::CanvasBacking::PaleFelt,
            "cancel leaves scroll distance and canvas surround unchanged");
    require(editor.preview_interface_hue == -1 && editor.settings.interface_hue == 220,
            "cancel restores the original interface hue");
    editor.execute("settings");
    window.perform_layout();
    hue = std::dynamic_pointer_cast<gf::TrackBar>(window.find("settings-theme-hue"));
    (*hue).set_value(280);
    std::shared_ptr<gf::NumericUpDown> recovery_interval =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("settings-recovery-seconds"));
    (*recovery_interval).set_value(30);
    distance = std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("settings-scroll"));
    (*distance).set_value(2.5);
    backing = std::dynamic_pointer_cast<gf::ComboBox>(window.find("settings-background"));
    (*backing).set_selected_index(static_cast<std::size_t>(paint::CanvasBacking::TanFelt));
    const paint::Image unchanged = editor.document.image;
    const std::shared_ptr<gf::ComboBox> alpha_background =
        std::dynamic_pointer_cast<gf::ComboBox>(window.find("settings-alpha-background"));
    const std::shared_ptr<gf::TextBox> alpha_color =
        std::dynamic_pointer_cast<gf::TextBox>(window.find("settings-alpha-color"));
    gesture = std::dynamic_pointer_cast<gf::ComboBox>(window.find("settings-shape-gesture"));
    (*gesture).set_selected_index(0);
    (*alpha_background).set_selected_index(1);
    (*alpha_color).set_text("#ED82C1");
    routed_button(window, "dialog-ok");
    paint::EditorSettings reloaded;
    reloaded.storage_path = editor.settings.storage_path;
    reloaded.load();
    require(reloaded.interface_hue == 280 && reloaded.recovery_enabled && reloaded.recovery_seconds == 30,
            "theme and crash-recovery settings persist together");
    require(!reloaded.drag_shapes && !editor.settings.drag_shapes && reloaded.scroll_distance == 2.5 &&
                editor.settings.scroll_distance == 2.5,
            "accepted scroll distance persists between launches");
    require(reloaded.canvas_backing == paint::CanvasBacking::TanFelt &&
                editor.settings.canvas_backing == paint::CanvasBacking::TanFelt,
            "accepted surround persists between launches");
    require(reloaded.solid_transparency && paint::equal(reloaded.transparency_color, {237, 130, 193, 255}) &&
                editor.settings.solid_transparency,
            "solid transparency color persists between launches");
    require(std::equal(unchanged.pixels.begin(), unchanged.pixels.end(), editor.document.image.pixels.begin(),
                       paint::equal),
            "transparency display settings never change document RGBA");
    std::filesystem::remove(editor.settings.storage_path);
}
void stamp_material_keeps_one_hardness_mask() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.choose_tool(paint::Tool::Stamp);
    editor.stamp_width = editor.stamp_height = 20;
    editor.stamp_hardness = 0.25;
    editor.document.stamp_shape = paint::StampShape::Square;
    for (int y = 15; y < 35; ++y) {
        for (int x = 15; x < 85; ++x) {
            editor.document.image.set(
                x, y, x < 40 ? paint::Color{220, 70, 50, 255} : paint::Color{40, 130, 100, 255});
        }
    }
    fixture.click(25, 25);
    const paint::Image original = editor.document.stamp;
    require(original.get(0, 10).a < original.get(10, 10).a, "stamp hardness feathers its capture boundary");
    editor.add_stamp_material();
    require(editor.document.stamp.pixels.empty() && editor.document.tool == paint::Tool::Stamp,
            "Add material re-enters capture mode without leaving Stamp");
    fixture.click(75, 25);
    require(original.pixels.size() == editor.document.stamp.pixels.size(),
            "added material retains the stamp dimensions");
    for (std::size_t index = 0; index < original.pixels.size(); ++index) {
        require(original.pixels[index].a == editor.document.stamp.pixels[index].a,
                "adding material applies hardness once and preserves the retained silhouette");
    }
    fixture.click(70, 65);
    editor.execute("undo");
    require(white(editor.document.image.get(70, 65)), "mixed stamp placement is undoable as one gesture");
}
void compact_ribbon_keeps_icons_and_fields() {
    Fixture fixture;
    gf::Window& window = *fixture.window;
    window.resize({800, 600});
    window.perform_layout();
    for (const char* id : {"tool-2", "tool-3", "tool-4", "shape-0", "shape-2", "shape-3", "brush-menu"}) {
        const std::shared_ptr<gf::Button> button = std::dynamic_pointer_cast<gf::Button>(window.find(id));
        require(button && (*button).image_list() &&
                    static_cast<bool>((*(*button).image_list()).resolve((*button).image_key())),
                "compact ribbon retains every small and large icon resource");
    }
    require((*window.find("home-tab")).committed_arranged_bounds().x >= 56,
            "compact Home tab remains clear of the File menu");
    require((*window.find("help")).committed_arranged_bounds().right() <= 800 &&
                (*window.find("help")).committed_arranged_bounds().width >= 24,
            "compact Help button remains fully visible and usable");
    require((*window.find("dimensions-status")).committed_arranged_bounds().right() <=
                (*window.find("status-zoom-reset")).committed_arranged_bounds().x,
            "compact status dimensions do not overlap the zoom controls");
}
void cobalt_tabs_and_split_button_routes() {
    Fixture fixture;
    gf::Window& window = *fixture.window;
    paint::forms::Editor& editor = *fixture.editor;
    editor.choose_tool(paint::Tool::Fill);
    for (double width : {800.0, 1280.0, 1760.0}) {
        window.resize({width, 700});
        open_tab(window, "view-tab");
        window.perform_layout();
        const gf::Rect help = (*window.find("help")).committed_arranged_bounds();
        require(help.width == 30 && help.right() == width,
                "Help stays anchored to the right edge at every width");
        const gf::Rect active = (*window.find("view-tab")).absolute_bounds();
        const gf::Rect passive = (*window.find("patterns-tab")).absolute_bounds();
        require(active.y < passive.y && active.height > passive.height,
                "selected sheet is raised above the darker rear tabs");
        // This point is inside the selected tab's overlap with the next sheet.
        // Its visible foreground face must own the pointer event as well.
        const gf::Point point{active.right() - 0.5, active.y + active.height / 2};
        window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, point});
        window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::primary, point});
        window.perform_layout();
        require((*require_button(window, "view-tab")).selected() &&
                    (*window.find("ribbon")).committed_arranged_bounds().height == 27,
                "foreground tab overlap activates that tab and preserves collapse behavior");
        open_tab(window, "home-tab");
        window.perform_layout();
        require((*window.find("home-tab")).tab_index() < (*window.find("view-tab")).tab_index() &&
                    (*window.find("view-tab")).tab_index() < (*window.find("patterns-tab")).tab_index(),
                "changing the foreground sheet preserves the logical keyboard order");
        for (const char* id : {"paste", "brush-menu", "tool-10", "tool-9", "tool-11"}) {
            const gf::Rect bounds = (*window.find(id)).absolute_bounds();
            const gf::Point arrow{bounds.x + bounds.width / 2, bounds.bottom() - 5};
            window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, arrow});
            window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::primary, arrow});
            window.perform_layout();
            require(window.find("ribbon-popup") != nullptr,
                    "persistent divider leaves the split dropdown target active at every width");
            window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::escape});
            require(!window.find("ribbon-popup"), "Escape dismisses each split-button popup");
        }
    }
}
void guide_atlas_and_text_effect_interactions() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.choose_tool(paint::Tool::Guide);
    require(!editor.guide.fill, "new guide follows the ribbon fill switch");
    routed_button(window, "fill-switch");
    require(editor.guide.fill, "filled-square ribbon switch enables guide body protection");
    fixture.click(20, 20);
    fixture.click(60, 20);
    fixture.click(60, 60);
    fixture.click(20, 60);
    fixture.click(20, 20);
    require(editor.guide.closed && !editor.document.dirty(),
            "guide is closed and never enters document history");
    editor.choose_tool(paint::Tool::Pencil);
    fixture.drag(5, 40, 80, 40);
    require(white(editor.document.image.get(40, 40)) && !white(editor.document.image.get(10, 40)),
            "routed pencil paints around guide stencil");
    editor.choose_tool(paint::Tool::Select);
    require(!editor.guide.active(), "selection deconstructs guide");
    fixture.drag(10, 10, 30, 30);
    editor.choose_tool(paint::Tool::Guide);
    require(editor.guide.closed && !editor.document.selection.active,
            "selection converts directly to guide polygon");
    editor.choose_tool(paint::Tool::Lasso);
    editor.document.image.set(11, 11, {73, 92, 121, 111});
    const paint::Image guide_source = editor.document.image;
    paint::SelectionMask feather;
    feather.bounds = {10, 10, 3, 3};
    feather.coverage = {0, 64, 0, 64, 255, 64, 0, 64, 0};
    editor.document.select_mask(feather);
    editor.choose_tool(paint::Tool::Guide);
    require(std::equal(editor.document.image.pixels.begin(), editor.document.image.pixels.end(),
                       guide_source.pixels.begin(), paint::equal),
            "selection-to-guide leaves feathered source pixels byte-identical");
    editor.execute("atlas-grid");
    window.perform_layout();
    (*std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("atlas-0"))).set_value(2);
    routed_button(window, "dialog-ok");
    require(!editor.guide.active() && editor.document.atlas.active >= 0,
            "atlas conversion clears stale guide");
    editor.choose_tool(paint::Tool::Pencil);
    editor.atlas_wrap = true;
    editor.atlas_preserve_alpha = true;
    for (std::size_t index = 0; index < editor.document.image.pixels.size(); ++index) {
        editor.document.image.pixels[index] = {30, 60, 90, static_cast<std::uint8_t>(index % 256)};
    }
    paint::Image before = editor.document.image;
    editor.document.ink.size = 7;
    editor.document.ink.primary = {240, 40, 90, 255};
    editor.refresh();
    fixture.drag(1, 1, -3, -3);
    for (std::size_t index = 0; index < before.pixels.size(); ++index) {
        require(before.pixels[index].a == editor.document.image.pixels[index].a,
                "atlas painting preserves every alpha byte");
    }
    int w = before.width, h = before.height;
    require(editor.document.image.get(w - 2, h - 2).r != before.get(w - 2, h - 2).r,
            "corner stroke wraps across both atlas axes");
    int active = editor.document.atlas.active;
    paint::Image painted = editor.document.image;
    editor.set_reference_frame(1);
    require(editor.document.atlas.active == active && !editor.atlas_reference.pixels.empty() &&
                std::memcmp(editor.document.image.pixels.data(), painted.pixels.data(),
                            painted.pixels.size() * sizeof(paint::Color)) == 0,
            "reference overlay leaves current frame and exported pixels untouched");
    editor.execute("atlas-reference-clear");
    require(editor.atlas_reference.pixels.empty(), "reference dismisses independently");
    editor.execute("text");
    fixture.click(2, 2);
    window.dispatch_text({"BO"});
    routed_button(window, "text-contour");
    std::shared_ptr<gf::NumericUpDown> skew =
        std::dynamic_pointer_cast<gf::NumericUpDown>(window.find("text-skew"));
    (*skew).set_value(0.3);
    require(editor.text.style.contour && editor.text.style.skew == 0.3,
            "text effect controls update editable session");
    painted = editor.document.image;
    paint::composite(painted, editor.text.preview, editor.text.bounds.x, editor.text.bounds.y);
    routed_button(window, "text-place");
    require(std::memcmp(editor.document.image.pixels.data(), painted.pixels.data(),
                        painted.pixels.size() * sizeof(paint::Color)) == 0,
            "transformed contour text places exactly its preview");
}
void draw_lasso(Fixture& fixture, const std::vector<paint::Point>& points, gf::Modifier modifier) {
    fixture.pointer(gf::PointerAction::down, points[0].x, points[0].y, gf::PointerButton::primary, modifier);
    for (std::size_t index = 1; index < points.size(); ++index) {
        fixture.pointer(gf::PointerAction::move, points[index].x, points[index].y, gf::PointerButton::primary,
                        modifier);
    }
    fixture.pointer(gf::PointerAction::up, points[0].x, points[0].y, gf::PointerButton::primary, modifier);
}
bool selected_pixel(const paint::Document& document, int x, int y) {
    const paint::FloatingSelection& selection = document.selection;
    x -= selection.x;
    y -= selection.y;
    return selection.active && selection.image.contains(x, y) &&
           (selection.coverage.empty() ||
            selection.coverage[static_cast<std::size_t>(y) * selection.image.width + x]);
}
void lasso_add_subtract_and_void_expansion() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.document.image.reset(128, 96, {249, 249, 249, 255});
    for (int y = 20; y < 60; ++y) {
        for (int x = 20; x < 60; ++x) {
            editor.document.image.set(x, y, {40, 90, 120, 255});
        }
    }
    const paint::Image original = editor.document.image;
    editor.choose_tool(paint::Tool::Lasso);
    editor.lasso_mode = paint::LassoMode::Tighten;
    draw_lasso(fixture, {{10, 10}, {70, 10}, {70, 70}, {10, 70}}, gf::Modifier::none);
    require(selected_pixel(editor.document, 30, 30) && !selected_pixel(editor.document, 15, 15),
            "tight lasso creates the object silhouette");
    draw_lasso(fixture, {{85, 25}, {110, 25}, {110, 50}, {85, 50}}, gf::Modifier::control);
    require(selected_pixel(editor.document, 30, 30) && selected_pixel(editor.document, 95, 35),
            "Ctrl adds a freeform island even when tightening mode would reject its uniform background");
    draw_lasso(fixture, {{30, 30}, {48, 30}, {48, 48}, {30, 48}}, gf::Modifier::alt);
    require(!selected_pixel(editor.document, 38, 38) && selected_pixel(editor.document, 25, 25) &&
                selected_pixel(editor.document, 95, 35),
            "Alt removes an interior freeform hole without moving or replacing the other islands");
    const paint::Image visible = editor.document.visible_image();
    require(std::equal(original.pixels.begin(), original.pixels.end(), visible.pixels.begin(), paint::equal),
            "editing the selection mask leaves every source RGBA pixel unchanged");
    const std::vector<std::vector<paint::Point>> contours =
        paint::mask_contours(editor.document.selection.coverage, editor.document.selection.image.width,
                             editor.document.selection.image.height);
    require(contours.size() == 3, "marching boundary data retains two islands and their interior hole");
    editor.document.commit_selection();
    editor.document.image.reset(128, 96, {40, 90, 120, 255});
    for (int y = 12; y < 80; ++y) {
        for (int x = 12; x < 115; ++x) {
            editor.document.image.set(x, y, {249, 249, 249, 255});
        }
    }
    editor.lasso_mode = paint::LassoMode::InnerVoid;
    draw_lasso(fixture, {{45, 30}, {70, 30}, {70, 50}, {45, 50}}, gf::Modifier::none);
    require(editor.document.selection.x == 12 && editor.document.selection.y == 12 &&
                editor.document.selection.image.width == 103 && editor.document.selection.image.height == 68,
            "inner-void lasso expands to the enclosing object beyond every edge of the drawn loop");
}
void cancel_pending_lines_and_guide_preview() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.choose_tool(paint::Tool::Path);
    fixture.click(20, 20);
    fixture.pointer(gf::PointerAction::move, 80, 60, gf::PointerButton::none);
    fixture.pointer(gf::PointerAction::down, 20, 20, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::up, 20, 20, gf::PointerButton::secondary);
    require(editor.document.path.nodes.empty() && !editor.document.path.extending &&
                editor.document.undo_history.empty() && !editor.document.dirty() &&
                display_white(editor, 50, 40),
            "right-click cancels a pending path and its first anchor, preview and undo entry");
    fixture.click(20, 20);
    fixture.click(80, 20);
    editor.execute("finish-path");
    const std::size_t history = editor.document.undo_history.size();
    fixture.click(20, 60);
    fixture.pointer(gf::PointerAction::down, 100, 70, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::up, 100, 70, gf::PointerButton::secondary);
    require(editor.document.path.nodes.size() == 2 && editor.document.path.runs.size() == 1 &&
                editor.document.undo_history.size() == history && !white(editor.document.image.get(50, 20)),
            "canceling an isolated new line preserves all committed runs and their history");
    editor.choose_shape(paint::Shape::Line);
    fixture.pointer(gf::PointerAction::down, 20, 50);
    fixture.pointer(gf::PointerAction::move, 90, 70);
    fixture.pointer(gf::PointerAction::down, 90, 70, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::up, 90, 70, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::up, 90, 70);
    require(white(editor.document.image.get(55, 60)) && !editor.canvas().has_pointer_capture(),
            "right-click cancels a dragged Line without placing it on either button release");
    editor.choose_tool(paint::Tool::Guide);
    fixture.click(20, 40);
    fixture.pointer(gf::PointerAction::move, 100, 60, gf::PointerButton::none);
    PreviewPainter pending;
    editor.paint_canvas_overlay(pending, {});
    require(pending.guide_lines == 1 && editor.guide.nodes.size() == 1,
            "Guide draws one floating preview segment without committing a second vertex");
    fixture.pointer(gf::PointerAction::down, 100, 60, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::up, 100, 60, gf::PointerButton::secondary);
    PreviewPainter canceled;
    editor.paint_canvas_overlay(canceled, {});
    require(editor.guide.nodes.empty() && canceled.guide_lines == 0,
            "right-click removes an initial Guide anchor and its preview");
    fixture.click(20, 40);
    fixture.click(100, 40);
    fixture.pointer(gf::PointerAction::move, 100, 80, gf::PointerButton::none);
    PreviewPainter continuing;
    editor.paint_canvas_overlay(continuing, {});
    require(continuing.guide_lines == 2, "Guide preview follows the retained last vertex");
    fixture.pointer(gf::PointerAction::down, 100, 80, gf::PointerButton::secondary);
    fixture.pointer(gf::PointerAction::up, 100, 80, gf::PointerButton::secondary);
    PreviewPainter finished;
    editor.paint_canvas_overlay(finished, {});
    require(finished.guide_lines == 1 && editor.guide.nodes.size() == 2 && !editor.guide.closed,
            "right-click removes only the pending Guide segment and keeps its committed edge");
}
void backward_path_and_guide_connections() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    editor.choose_tool(paint::Tool::Path);
    fixture.click(20, 20);
    fixture.click(100, 20);
    fixture.click(100, 80);
    fixture.pointer(gf::PointerAction::move, 21, 21, gf::PointerButton::none);
    require(!display_white(editor, 60, 50) && white(editor.document.image.get(60, 50)),
            "backward closing segment is visible before clicking and still uncommitted");
    fixture.click(21, 21);
    require(!white(editor.document.image.get(60, 50)) && !editor.document.path.extending,
            "returning to an earlier node actually paints the backward connection");
    fixture.click(20, 80);
    fixture.click(102, 22);
    require(!white(editor.document.image.get(60, 50)) && editor.document.path.nodes.back().x == 100 &&
                editor.document.path.nodes.back().y == 20 && !editor.document.path.extending,
            "a later run connects back to a retained earlier junction");
    editor.execute("release");
    editor.choose_tool(paint::Tool::Guide);
    fixture.click(20, 20);
    fixture.click(100, 20);
    fixture.click(100, 70);
    fixture.click(101, 21);
    require(editor.guide.nodes.size() == 4 && editor.guide.nodes.back().x == 100 &&
                editor.guide.nodes.back().y == 20 && !editor.guide.closed,
            "guide click joins an earlier noninitial vertex without moving it");
    fixture.drag(100, 20, 90, 25);
    require(editor.guide.nodes[1].x == 90 && editor.guide.nodes[3].x == 90,
            "dragging a shared guide junction moves every attached segment");
    fixture.click(20, 70);
    fixture.click(21, 21);
    require(editor.guide.closed && editor.guide.nodes[0].x == 20 && editor.guide.nodes[0].y == 20,
            "clicking the first guide node closes it with exact snapping");
}
void stamp_material_union_and_menu_toggle() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.document.image.set(20, 20, {220, 40, 50, 255});
    editor.document.image.set(66, 20, {40, 160, 90, 255});
    editor.choose_tool(paint::Tool::Stamp);
    editor.document.stamp_shape = paint::StampShape::Square;
    editor.stamp_width = editor.stamp_height = 20;
    fixture.click(25, 20);
    const paint::Image source = editor.document.image;
    editor.choose_tool(paint::Tool::Pencil);
    editor.add_stamp_material();
    require(editor.document.tool == paint::Tool::Stamp, "Add material activates Stamp from another tool");
    fixture.pointer(gf::PointerAction::move, 65, 20, gf::PointerButton::none);
    fixture.click(65, 20);
    require(editor.document.stamp.get(5, 10).a == 255 && editor.document.stamp.get(11, 10).a == 255,
            "new material extends the retained stamp into its formerly transparent region");
    require(std::equal(source.pixels.begin(), source.pixels.end(), editor.document.image.pixels.begin(),
                       paint::equal),
            "moving and capturing added material never paints into the source picture");
    open_tab(window, "home-tab");
    const std::shared_ptr<gf::DropDownButton> stamp =
        std::dynamic_pointer_cast<gf::DropDownButton>(window.find("tool-10"));
    const gf::Rect button = (*stamp).absolute_bounds();
    const gf::Point arrow{button.x + button.width / 2, button.bottom() - 5};
    window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, arrow});
    window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::primary, arrow});
    window.perform_layout();
    require(window.find("ribbon-popup") != nullptr, "Stamp disclosure opens its menu");
    const gf::Point main{button.x + button.width / 2, button.y + button.height / 3};
    window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, main});
    window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::primary, main});
    require(!window.find("ribbon-popup"), "clicking the main Stamp button closes its open menu");
}
void guide_dismissal_preserves_paint_and_curves() {
    for (const paint::Tool tool : {paint::Tool::Select, paint::Tool::Lasso, paint::Tool::Path,
                                   paint::Tool::Shape, paint::Tool::Text, paint::Tool::Reshape}) {
        Fixture fixture;
        paint::forms::Editor& editor = *fixture.editor;
        editor.document.image.reset(128, 96, {40, 90, 130, 255});
        editor.document.image.set(40, 40, {21, 43, 68, 97});
        const paint::Image original = editor.document.image;
        editor.choose_tool(paint::Tool::Guide);
        fixture.click(20, 20);
        fixture.click(80, 20);
        fixture.click(80, 70);
        fixture.click(20, 70);
        editor.execute("guide-set");
        editor.choose_tool(tool);
        require(!editor.guide.active() && std::equal(original.pixels.begin(), original.pixels.end(),
                                                     editor.document.image.pixels.begin(), paint::equal),
                "switching away from a guide preserves every document pixel, including alpha");
    }
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.document.image.reset(128, 96, {40, 90, 130, 255});
    const paint::Image original = editor.document.image;
    const std::uint64_t revision = editor.document.revision;
    editor.choose_tool(paint::Tool::Guide);
    fixture.click(20, 30);
    fixture.click(100, 30);
    fixture.click(100, 80);
    fixture.click(20, 80);
    editor.execute("guide-set");
    editor.guide.fill = true;
    editor.execute("cut");
    require(!editor.guide.active() && editor.document.tool == paint::Tool::Pencil &&
                editor.document.revision == revision && !editor.document.selection.active &&
                std::equal(original.pixels.begin(), original.pixels.end(),
                           editor.document.image.pixels.begin(), paint::equal),
            "Cut dismisses a stencil without invoking whole-image Cut or changing history");
    editor.choose_tool(paint::Tool::Guide);
    fixture.click(20, 30);
    fixture.click(100, 30);
    fixture.click(100, 80);
    fixture.click(20, 80);
    editor.execute("guide-set");
    editor.guide.fill = true;
    open_tab(window, "home-tab");
    const gf::Rect button = (*window.find("tool-11")).absolute_bounds();
    const gf::Point arrow{button.x + button.width / 2, button.bottom() - 5};
    window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, arrow});
    window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::primary, arrow});
    window.perform_layout();
    routed_button(window, "popup-guide-swap");
    window.perform_layout();
    routed_button(window, "popup-guide-swap-bezier");
    fixture.click(60, 30);
    require(editor.guide.segments.size() == 1, "Guide split menu selects a curve without painting a path");
    fixture.drag(46.6666666667, 30, 46.6666666667, 0);
    fixture.drag(73.3333333333, 30, 73.3333333333, 0);
    require(editor.guide.blocked(60, 18) > 0.9 && editor.guide.blocked(60, 2) == 0 &&
                std::equal(original.pixels.begin(), original.pixels.end(),
                           editor.document.image.pixels.begin(), paint::equal),
            "curved guide moves its protection boundary without changing the painting");
    editor.begin_guide_swap(paint::CurveKind::Arc);
    fixture.click(100, 55);
    fixture.drag(100, 55, 116, 55);
    require(editor.guide.segments.size() == 2 && editor.guide.blocked(110, 55) > 0.9,
            "Guide Arc handles curve the stencil boundary");
    editor.choose_tool(paint::Tool::Pencil);
    fixture.drag(2, 18, 124, 18);
    require(paint::equal(editor.document.image.get(60, 18), original.get(60, 18)) &&
                !paint::equal(editor.document.image.get(5, 18), original.get(5, 18)),
            "painting respects the curved guide body");
    const paint::Image painted = editor.document.image;
    editor.choose_tool(paint::Tool::Guide);
    require(!editor.guide.active() && editor.document.tool == paint::Tool::Pencil &&
                std::equal(painted.pixels.begin(), painted.pixels.end(), editor.document.image.pixels.begin(),
                           paint::equal),
            "Guide toggles off from a painting tool and leaves its pixels intact");
    editor.choose_tool(paint::Tool::Guide);
    fixture.click(20, 20);
    editor.choose_tool(paint::Tool::Guide);
    require(editor.guide.nodes.empty() && editor.document.tool == paint::Tool::Pencil,
            "Guide toggles off a pending first anchor as well");
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
void custom_pattern_canvas_is_independent() {
    Fixture fixture;
    paint::forms::Editor& editor = *fixture.editor;
    gf::Window& window = *fixture.window;
    editor.document.image.set(9, 9, {55, 99, 123, 255});
    const std::vector<paint::Color> original = editor.document.image.pixels;
    const std::uint64_t revision = editor.document.revision;
    const double zoom = editor.canvas().zoom();
    editor.toggle_pattern_canvas();
    window.perform_layout();
    const std::shared_ptr<paint::forms::PatternCanvas> pattern =
        std::dynamic_pointer_cast<paint::forms::PatternCanvas>(window.find("custom-pattern-canvas"));
    require(pattern && editor.pattern_editing, "custom pattern canvas opens");
    require((*editor.custom_pattern).width == 8 && (*editor.custom_pattern).height == 8,
            "custom pattern starts at eight square pixels");
    const gf::Rect bounds = (*pattern).tile_bounds();
    const gf::Point start = (*pattern).point_to_window({bounds.x + bounds.width / 16, bounds.y + bounds.height / 16});
    const gf::Point end = (*pattern).point_to_window({bounds.x + bounds.width * 15 / 16, bounds.y + bounds.height / 16});
    window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::primary, start});
    window.dispatch_pointer({gf::PointerAction::move, gf::PointerButton::primary, end});
    window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::primary, end});
    for (int x = 0; x < 8; ++x) { require((*editor.custom_pattern).get(x, 0).r == 0, "sparse pencil motion fills every crossed pixel"); }
    window.dispatch_pointer({gf::PointerAction::down, gf::PointerButton::secondary, start});
    window.dispatch_pointer({gf::PointerAction::up, gf::PointerButton::secondary, start});
    require(white((*editor.custom_pattern).get(0, 0)), "right pencil is white");
    editor.execute("undo");
    require((*editor.custom_pattern).get(0, 0).r == 0, "pattern undo affects pattern only");
    (*pattern).resize_tile(11, 5);
    require((*editor.custom_pattern).width == 11 && (*editor.custom_pattern).height == 5 &&
            white((*editor.custom_pattern).get(10, 0)) && (*editor.custom_pattern).get(7, 0).r == 0,
            "resize preserves overlap and fills new area white");
    (*pattern).undo();
    require((*editor.custom_pattern).width == 8, "pattern resize is undoable");
    editor.execute("new");
    require(editor.document.image.width == 128, "main document commands cannot leak into pattern editing");
    editor.toggle_pattern_canvas();
    window.perform_layout();
    require(!editor.pattern_editing && editor.canvas().zoom() == zoom && editor.document.revision == revision,
            "return preserves view and document history");
    require(std::memcmp(original.data(), editor.document.image.pixels.data(), original.size() * sizeof(paint::Color)) == 0,
            "pattern editing never changes main pixels");
    paint::Ink ink = editor.document.ink;
    ink.pattern = paint::Pattern::Custom;
    ink.primary = {123, 22, 33, 255};
    ink.secondary = {77, 88, 99, 255};
    require(paint::equal(paint::patterned(ink, -1, 0), ink.primary) &&
            paint::equal(paint::patterned(ink, 8, 1), ink.secondary), "custom mask repeats in both coordinate directions using material colors");
    editor.toggle_pattern_canvas();
    require((*editor.custom_pattern).get(7, 0).r == 0, "tile survives canvas switches");
    for (int i = static_cast<int>(paint::Pattern::Buttons); i < static_cast<int>(paint::Pattern::Custom); ++i) {
        ink.pattern = static_cast<paint::Pattern>(i);
        int foreground = 0;
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                const paint::Color c = paint::patterned(ink, x, y);
                foreground += paint::equal(c, ink.primary) ? 1 : 0;
                require(paint::equal(c, paint::patterned(ink, x + 16, y + 16)), "classic tiles repeat seamlessly");
            }
        }
        require(foreground > 0 && foreground < 256, "each classic tile contains both materials");
    }
}
int main() {
    try {
        custom_pattern_canvas_is_independent();
        tool_cursor_routes();
        freehand_wand_picker_and_size();
        working_view_coordinates_and_reset();
        rotated_frames_use_published_images();
        canvas_work_is_coalesced_and_stamp_is_cached();
        persistent_selection_painting();
        selection_holes_flood_and_floating_paste();
        selection_text_path_carpet_and_move();
        selection_skew_handles();
        text_caret_deadlines_damage_only_the_transformed_caret();
        pencil_is_independent_of_brush_material();
        backward_path_and_guide_connections();
        cancel_pending_lines_and_guide_preview();
        lasso_add_subtract_and_void_expansion();
        stamp_material_union_and_menu_toggle();
        dialog_clipboard_and_close_contracts();
        display_preserves_document_and_hidden_rgb();
        startup_file_opens_after_window_attachment();
        captured_stroke_undo_and_right_color();
        solid_primary_keeps_alternate_drawing();
        material_deposition_does_not_depend_on_event_count();
        retained_curve_save_undo_and_release();
        selection_move_path_and_stamp();
        interface_colors_and_spacing();
        recovery_editor_contracts();
        scroll_distance_bounds_and_settings();
        stamp_material_keeps_one_hardness_mask();
        compact_ribbon_keeps_icons_and_fields();
        cobalt_tabs_and_split_button_routes();
        guide_atlas_and_text_effect_interactions();
        guide_dismissal_preserves_paint_and_curves();
        zoom_anchors_the_point();
        magnifier_hover_is_display_only();
        zoom_out_clamps_each_axis();
        pencil_and_eraser_hover_are_display_only();
        restored_help_and_selection_workflows();
        stamp_scrubs_one_undo_gesture();
        path_hover_snap_and_controls();
        path_node_drag_and_overlap();
        click_move_click_shapes();
        centered_closed_shapes();
        centered_circle_and_materials();
        dither_dialog_and_brush();
        carpet_generator_controls();
        spirograph_apparatus_and_ink();
        spirograph_guide_dismissal();
        independent_color_materials_and_no_color();
        select_all_delete_without_drag();
        ribbon_collapse_and_reopen();
        about_reports_build_version();
        ribbon_preserves_artwork_position();
        crop_and_help_actions();
        stamp_reset_and_recapture();
        atlas_grid_frames_and_cursor_save();
        desktop_transactions_drop_and_handles();
        transforms_preview_before_release();
        selection_handles_repaint_during_capture();
        large_selection_preview_workload();
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
