#include "forms/editor.hpp"
#include "codecs.hpp"
#include "forms/display.hpp"
#include "platform.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <gui_forms/host.hpp>
#include <numbers>
#include <stdexcept>
namespace paint::forms {
namespace gf = gui_forms;
namespace {
constexpr int sizes[] = {1, 2, 3, 4, 5, 7, 8, 12, 16, 24, 32, 48, 64};
const Tool tools[] = {Tool::Select,    Tool::Lasso, Tool::Pencil, Tool::Fill, Tool::Eraser, Tool::Picker,
                      Tool::Magnifier, Tool::Brush, Tool::Shape,  Tool::Path, Tool::Stamp};
const char* tool_names[] = {"Select", "Lasso", "Pencil", "Fill", "Eraser", "Picker",
                            "Zoom",   "Brush", "Shape",  "Path", "Stamp"};
Rect rectangle(Point start, Point end) {
    return {static_cast<int>(std::floor(std::min(start.x, end.x))),
            static_cast<int>(std::floor(std::min(start.y, end.y))),
            std::max(1, static_cast<int>(std::ceil(std::abs(end.x - start.x)))),
            std::max(1, static_cast<int>(std::ceil(std::abs(end.y - start.y))))};
}
bool point_inside(Rect rectangle, Point point) {
    return point.x >= rectangle.x && point.y >= rectangle.y && point.x < rectangle.x + rectangle.w &&
           point.y < rectangle.y + rectangle.h;
}
std::vector<std::string> names(const char* const* values, int count) {
    std::vector<std::string> result;
    for (int index = 0; index < count; ++index) {
        result.emplace_back(values[index]);
    }
    return result;
}
} // namespace
PaintCanvas::PaintCanvas(gf::StableId id, std::weak_ptr<Editor> editor)
    : RasterCanvas(std::move(id)), editor_(std::move(editor)) {}
void PaintCanvas::on_pointer(gf::PointerEvent& event) {
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    (*editor).pointer(event);
    event.handled = event.action == gf::PointerAction::down || event.action == gf::PointerAction::up ||
                    event.action == gf::PointerAction::move || event.action == gf::PointerAction::wheel;
}
void PaintCanvas::on_paint_overlay(gf::Painter& painter, gf::Rect damage) {
    std::shared_ptr<Editor> editor = editor_.lock();
    if (editor) {
        (*editor).paint_canvas_overlay(painter, damage);
    }
}
Editor::Editor(gf::StableId id) : Control(std::move(id)) {}
void Editor::initialize_control_tree() {
    canvas_ = gf::make_control<PaintCanvas>(gf::StableId("canvas"),
                                            std::static_pointer_cast<Editor>(shared_from_this()));
    (*canvas_).set_focusable(true);
    (*canvas_).set_tab_stop(true);
    (*canvas_).set_view(1.0, {-16, -16});
    (*canvas_).set_canvas_background(gf::Color::rgba(211, 221, 232));
    add_child(canvas_);
    menu_ = gf::make_control<gf::MenuStrip>(gf::StableId("menus"));
    (*menu_).set_items(
        {{"file",
          "File",
          {menu_item("new", "New"), menu_item("open", "Open…"), menu_item("save", "Save"),
           menu_item("save-as", "Save as…"),
#if RAINSTAR_FORMS_NATIVE_PRINT
           menu_item("print", "Print…"), menu_item("page-setup", "Page setup…"),
#endif
           menu_item("quit", "Exit")}},
         {"edit",
          "Edit",
          {menu_item("undo", "Undo"), menu_item("redo", "Redo"), menu_item("cut", "Cut"),
           menu_item("copy", "Copy"), menu_item("paste", "Paste"), menu_item("select-all", "Select all"),
           menu_item("invert-selection", "Invert selection"), menu_item("delete", "Delete selection")}},
         {"image",
          "Image",
          {menu_item("crop", "Crop"), menu_item("rotate-right", "Rotate right 90°"),
           menu_item("rotate-left", "Rotate left 90°"), menu_item("flip-horizontal", "Flip horizontal"),
           menu_item("flip-vertical", "Flip vertical"), menu_item("invert", "Invert colors")}},
         {"view",
          "View",
          {menu_item("zoom-in", "Zoom in"), menu_item("zoom-out", "Zoom out"),
           menu_item("actual-size", "Actual size"), menu_item("fit", "Fit canvas")}},
         {"help",
          "Help",
          {menu_item("help", "Controls and port status"), menu_item("about", "About Rainstar Paint")}}});
    add_child(menu_);
    add_button("paste", "Paste", {8, 40, 66, 58});
    add_button("cut", "Cut", {78, 40, 55, 27});
    add_button("copy", "Copy", {78, 71, 55, 27});
    add_button("crop", "Crop", {145, 40, 64, 27});
    add_button("rotate-right", "Rotate", {145, 71, 64, 27});
    for (std::size_t index = 0; index < std::size(tools); ++index) {
        double x = 224 + static_cast<double>(index % 6) * 62;
        double y = 40 + static_cast<double>(index / 6) * 31;
        add_button("tool-" + std::to_string(index), tool_names[index], {x, y, 59, 27});
    }
    brush_ = gf::make_control<gf::ComboBox>(gf::StableId("brush"));
    shape_ = gf::make_control<gf::ComboBox>(gf::StableId("shape"));
    pattern_ = gf::make_control<gf::ComboBox>(gf::StableId("pattern"));
    size_ = gf::make_control<gf::ComboBox>(gf::StableId("size"));
    (*brush_).set_items(names(brush_names, brush_count));
    (*shape_).set_items(names(shape_names, shape_count));
    (*pattern_).set_items(names(pattern_names, 18));
    std::vector<std::string> size_names;
    for (int size : sizes) {
        size_names.push_back(std::to_string(size) + " px");
    }
    (*size_).set_items(std::move(size_names));
    (*brush_).set_selected_index(0);
    (*shape_).set_selected_index(3);
    (*pattern_).set_selected_index(0);
    (*size_).set_selected_index(2);
    (*brush_).set_requested_bounds({610, 40, 136, 27});
    (*shape_).set_requested_bounds({610, 71, 136, 27});
    (*pattern_).set_requested_bounds({754, 40, 136, 27});
    (*size_).set_requested_bounds({754, 71, 136, 27});
    add_child(brush_);
    add_child(shape_);
    add_child(pattern_);
    add_child(size_);
    subscriptions_.push_back((*brush_).selected_index_changed().subscribe(
        *this, gf::Delegate<std::optional<std::size_t>>::bind<Editor, &Editor::brush_changed>(*this)));
    subscriptions_.push_back((*shape_).selected_index_changed().subscribe(
        *this, gf::Delegate<std::optional<std::size_t>>::bind<Editor, &Editor::shape_changed>(*this)));
    subscriptions_.push_back((*pattern_).selected_index_changed().subscribe(
        *this, gf::Delegate<std::optional<std::size_t>>::bind<Editor, &Editor::pattern_changed>(*this)));
    subscriptions_.push_back((*size_).selected_index_changed().subscribe(
        *this, gf::Delegate<std::optional<std::size_t>>::bind<Editor, &Editor::size_changed>(*this)));
    add_button("primary", "Color 1", {905, 40, 65, 27});
    add_button("secondary", "Color 2", {978, 40, 65, 27});
    add_button("outline", "Outline", {905, 71, 65, 27});
    add_button("fill", "Fill", {978, 71, 65, 27});
    add_button("transparent-pattern", "Transparent pattern", {754, 104, 136, 25});
    add_button("transparent-selection", "Transparent selection", {224, 104, 182, 25});
    add_button("continuous-path", "Continuous path", {412, 104, 130, 25});
    status_ = gf::make_control<gf::Label>(gf::StableId("status"));
    (*status_).set_font({gf::FontRole::control, 12, 400, false});
    add_child(status_);
    refresh();
}
void Editor::add_button(const std::string& id, const std::string& text, gf::Rect bounds) {
    std::shared_ptr<gf::Button> button = gf::make_control<gf::Button>(gf::StableId(id), text);
    (*button).set_requested_bounds(bounds);
    (*button).set_visual_style(gf::ButtonVisualStyle::flat);
    (*button).set_flat_border_width(1);
    subscriptions_.push_back((*button).clicked().subscribe(
        *this, gf::Delegate<gf::ButtonBase&>::bind<Editor, &Editor::button_clicked>(*this)));
    buttons_.push_back(button);
    add_child(button);
}
gf::MenuItemSpec Editor::menu_item(const std::string& id, const std::string& text) {
    std::shared_ptr<gf::Command> command = std::make_shared<gf::Command>(id, text);
    subscriptions_.push_back((*command).invoked().subscribe(
        *this, gf::Delegate<const gf::CommandInvocation&>::bind<Editor, &Editor::command_invoked>(*this)));
    commands_.push_back(command);
    return {id, gf::MenuItemKind::command, command, text};
}
void Editor::arrange(gf::Rect bounds) {
    arrange_self(bounds);
    set_child_layout(menu_, {0, 0, bounds.width, 31});
    set_child_layout(canvas_, {0, 144, bounds.width, std::max(1.0, bounds.height - 170)});
    set_child_layout(status_, {12, bounds.height - 25, bounds.width - 24, 25});
    for (std::size_t index = 0; index < buttons_.size(); ++index) {
        set_child_layout(buttons_[index], (*buttons_[index]).requested_bounds());
    }
    set_child_layout(brush_, (*brush_).requested_bounds());
    set_child_layout(shape_, (*shape_).requested_bounds());
    set_child_layout(pattern_, (*pattern_).requested_bounds());
    set_child_layout(size_, (*size_).requested_bounds());
}
void Editor::on_paint(gf::Painter& painter, gf::Rect) {
    gf::Rect bounds = committed_arranged_bounds();
    painter.fill_rect({0, 0, bounds.width, bounds.height}, gf::Color::rgba(241, 246, 251));
    painter.draw_line({0, 31}, {bounds.width, 31}, gf::Color::rgba(175, 191, 210), 1);
    for (double x : {139.0, 217.0, 601.0, 747.0, 897.0}) {
        painter.draw_line({x, 37}, {x, 135}, gf::Color::rgba(199, 210, 224), 1);
    }
    const gf::FontSpec font{gf::FontRole::control, 11, 400, false};
    painter.draw_text_utf8({43, 119}, "Clipboard", font, gf::Color::rgba(80, 96, 115));
    painter.draw_text_utf8({160, 119}, "Image", font, gf::Color::rgba(80, 96, 115));
    painter.draw_text_utf8({631, 120}, "Brush / shape", font, gf::Color::rgba(80, 96, 115));
    painter.fill_rect({912, 111, 24, 16}, gf::Color::rgba(document.ink.primary.r, document.ink.primary.g,
                                                          document.ink.primary.b, document.ink.primary.a));
    painter.stroke_rect({912, 111, 24, 16}, gf::Color::rgba(70, 85, 100), 1);
    painter.fill_rect({985, 111, 24, 16},
                      gf::Color::rgba(document.ink.secondary.r, document.ink.secondary.g,
                                      document.ink.secondary.b, document.ink.secondary.a));
    painter.stroke_rect({985, 111, 24, 16}, gf::Color::rgba(70, 85, 100), 1);
}
gf::Point Editor::screen(Point point) const {
    gui_drawing::PointF origin = (*canvas_).view_origin();
    double zoom = (*canvas_).zoom();
    return {(point.x - origin.x) * zoom, (point.y - origin.y) * zoom};
}
void Editor::paint_canvas_overlay(gf::Painter& painter, gf::Rect) {
    painter.save();
    painter.clip_rect(
        {0, 0, (*canvas_).committed_arranged_bounds().width, (*canvas_).committed_arranged_bounds().height});
    if (document.selection.active ||
        (dragging_ && (document.tool == Tool::Select || document.tool == Tool::Lasso))) {
        Rect bounds = document.selection.active
                          ? Rect{document.selection.x, document.selection.y, document.selection.image.width,
                                 document.selection.image.height}
                          : rectangle(start_, current_);
        gf::Point top = screen({static_cast<double>(bounds.x), static_cast<double>(bounds.y)});
        painter.stroke_rect({top.x, top.y, bounds.w * (*canvas_).zoom(), bounds.h * (*canvas_).zoom()},
                            gf::Color::rgba(30, 100, 190), 1);
        for (std::size_t index = 1; index < lasso_.size(); ++index) {
            painter.draw_line(screen(lasso_[index - 1]), screen(lasso_[index]), gf::Color::rgba(30, 100, 190),
                              1);
        }
    }
    if (document.curve.line_set) {
        for (int index = 0; index < document.curve.geometry.handle_count(); ++index) {
            gf::Point point = screen(document.curve.geometry.handle(index));
            if (document.curve.geometry.kind == CurveKind::Bezier) {
                painter.draw_line(
                    screen(index == 0 ? document.curve.geometry.start : document.curve.geometry.end), point,
                    gf::Color::rgba(50, 110, 180), 1);
            }
            painter.fill_rect({point.x - 5, point.y - 5, 10, 10}, gf::Color::rgba(255, 255, 255));
            painter.stroke_rect({point.x - 5, point.y - 5, 10, 10}, gf::Color::rgba(30, 100, 190), 2);
        }
    }
    for (std::size_t index = 0; index < document.path.nodes.size(); ++index) {
        gf::Point point = screen(document.path.nodes[index]);
        painter.fill_rect({point.x - 3, point.y - 3, 6, 6}, gf::Color::rgba(30, 100, 190));
    }
    painter.restore();
}
void Editor::ready(gf::Window&, gf::ApplicationWindowHandle handle) {
    handle_ = handle;
    refresh();
}
void Editor::closing(gf::HostCloseRequest& request) {
    request.cancel = !can_replace();
}
gf::RasterCanvas& Editor::canvas() {
    return *canvas_;
}
void Editor::refresh() {
    synchronizing_controls_ = true;
    (*brush_).set_selected_index(static_cast<std::size_t>(document.ink.brush));
    (*shape_).set_selected_index(static_cast<std::size_t>(document.shape));
    (*pattern_).set_selected_index(static_cast<std::size_t>(document.ink.pattern));
    std::optional<std::size_t> size_index;
    for (std::size_t index = 0; index < std::size(sizes); ++index) {
        if (sizes[index] == document.ink.size) {
            size_index = index;
        }
    }
    (*size_).set_selected_index(size_index);
    (*size_).set_placeholder_text(std::to_string(document.ink.size) + " px");
    synchronizing_controls_ = false;
    if (preview_active_) {
        publish_image(preview_, *canvas_);
    } else if (document.selection.active) {
        publish_image(document.visible_image(), *canvas_);
    } else {
        publish_image(document.image, *canvas_);
    }
    for (std::size_t index = 0; index < buttons_.size(); ++index) {
        gf::Button& button = *buttons_[index];
        std::string id(button.stable_id().value());
        bool selected = false;
        if (id.starts_with("tool-")) {
            selected = document.tool == tools[std::stoul(id.substr(5))];
        } else if (id == "outline") {
            selected = document.shape_outline;
        } else if (id == "fill") {
            selected = document.shape_fill;
        } else if (id == "transparent-pattern") {
            selected = document.ink.transparent_pattern;
        } else if (id == "transparent-selection") {
            selected = document.transparent_selection;
        } else if (id == "continuous-path") {
            selected = document.continuous_path;
        }
        button.set_selected(selected);
    }
    update_status();
    invalidate(gf::Rect{0, 31, committed_arranged_bounds().width, 113});
    (*canvas_).invalidate(gf::Dirty::paint);
}
void Editor::update_status() {
    if (status_) {
        (*status_).set_text(
            (document.filename.empty() ? "Untitled"
                                       : std::filesystem::path(document.filename).filename().string()) +
            (document.dirty() ? " *" : "") + "   |   " + std::to_string(document.image.width) + " × " +
            std::to_string(document.image.height) + " px   |   " +
            std::to_string(static_cast<int>(std::round((*canvas_).zoom() * 100))) + "%   |   GUI.Forms port");
    }
}
void Editor::button_clicked(gf::ButtonBase& button) {
    execute(std::string(button.stable_id().value()));
}
void Editor::command_invoked(const gf::CommandInvocation& invocation) {
    execute(invocation.command_id);
}
void Editor::brush_changed(std::optional<std::size_t> index) {
    if (!index || synchronizing_controls_) {
        return;
    }
    document.ink.brush = static_cast<Brush>(*index);
    choose_tool(Tool::Brush);
}
void Editor::shape_changed(std::optional<std::size_t> index) {
    if (!index || synchronizing_controls_) {
        return;
    }
    finish_controls();
    document.shape = static_cast<Shape>(*index);
    choose_tool(Tool::Shape);
}
void Editor::pattern_changed(std::optional<std::size_t> index) {
    if (!index || synchronizing_controls_) {
        return;
    }
    document.ink.pattern = static_cast<Pattern>(*index);
    document.sync_curve();
    document.sync_path();
    refresh();
}
void Editor::size_changed(std::optional<std::size_t> index) {
    if (!index || synchronizing_controls_) {
        return;
    }
    document.ink.size = sizes[*index];
    document.sync_curve();
    document.sync_path();
    refresh();
}
void Editor::release_gesture() {
    dragging_ = false;
    panning_ = false;
    moving_selection_ = false;
    preview_active_ = false;
    curve_handle_ = -1;
    lasso_.clear();
    eraser_.clear();
    material_.clear();
    (*canvas_).set_pointer_capture(false);
}
void Editor::finish_controls() {
    release_gesture();
    document.commit_selection();
    document.commit_path();
    document.commit_curve();
}
void Editor::choose_tool(Tool tool) {
    if (document.tool != tool) {
        finish_controls();
    }
    document.tool = tool;
    refresh();
}
void Editor::pointer(const gf::PointerEvent& event) {
    try {
        shift_ = gf::has_modifier(event.modifiers, gf::Modifier::shift);
        gf::Point client = (*canvas_).point_from_window(event.position);
        gui_drawing::PointF mapped = (*canvas_).client_to_bitmap(client);
        Point point{mapped.x, mapped.y};
        if (event.action == gf::PointerAction::wheel) {
            if (gf::has_modifier(event.modifiers, gf::Modifier::control) ||
                gf::has_modifier(event.modifiers, gf::Modifier::meta)) {
                zoom(event.wheel_delta.y >= 0 ? 1.25 : 0.8, client);
            } else {
                gui_drawing::PointF origin = (*canvas_).view_origin();
                origin.x -= event.wheel_delta.x * 30 / (*canvas_).zoom();
                origin.y -= event.wheel_delta.y * 30 / (*canvas_).zoom();
                (*canvas_).set_view_origin(origin);
                invalidate(gf::Dirty::paint);
            }
            return;
        }
        if (event.action == gf::PointerAction::down) {
            if (window()) {
                static_cast<void>((*window()).request_focus(canvas_));
            }
            if (event.button == gf::PointerButton::middle) {
                panning_ = true;
                pan_start_ = client;
                pan_origin_ = (*canvas_).view_origin();
                (*canvas_).set_pointer_capture(true);
                return;
            }
            if (event.button != gf::PointerButton::primary && event.button != gf::PointerButton::secondary) {
                return;
            }
            if (event.button == gf::PointerButton::secondary &&
                (document.tool == Tool::Path || document.tool == Tool::Stamp)) {
                if (document.tool == Tool::Path) {
                    document.end_path_geometry();
                } else {
                    document.stamp = {};
                }
                release_gesture();
                refresh();
                return;
            }
            begin(point, event.button == gf::PointerButton::secondary);
        } else if (event.action == gf::PointerAction::move) {
            if (panning_) {
                (*canvas_).set_view_origin({pan_origin_.x - (client.x - pan_start_.x) / (*canvas_).zoom(),
                                            pan_origin_.y - (client.y - pan_start_.y) / (*canvas_).zoom()});
                invalidate(gf::Dirty::paint);
            } else if (dragging_) {
                move(point);
            }
        } else if (event.action == gf::PointerAction::up) {
            if (panning_) {
                panning_ = false;
                (*canvas_).set_pointer_capture(false);
            } else if (dragging_) {
                end(point);
            }
        }
    } catch (const std::exception& exception) {
        release_gesture();
        error(exception.what());
    }
}
void Editor::begin(Point point, bool secondary) {
    start_ = last_ = current_ = point;
    gesture_ink_ = document.ink;
    if (secondary) {
        std::swap(gesture_ink_.primary, gesture_ink_.secondary);
    }
    if (document.curve.line_set) {
        for (int index = 0; index < document.curve.geometry.handle_count(); ++index) {
            Point handle = document.curve.geometry.handle(index);
            if (std::hypot(handle.x - point.x, handle.y - point.y) * (*canvas_).zoom() < 11) {
                curve_handle_ = index;
                handle_offset_ = {point.x - handle.x, point.y - handle.y};
                handle_checkpoint_ = false;
                dragging_ = true;
                (*canvas_).set_pointer_capture(true);
                return;
            }
        }
        return;
    }
    if (!document.image.contains(static_cast<int>(std::floor(point.x)),
                                 static_cast<int>(std::floor(point.y)))) {
        return;
    }
    if (document.tool == Tool::Magnifier) {
        zoom(secondary ? 0.5 : 2, screen(point));
        return;
    }
    if (document.tool == Tool::Picker) {
        Color color = document.visible_image().get(static_cast<int>(point.x), static_cast<int>(point.y));
        if (secondary) {
            document.ink.secondary = color;
        } else {
            document.ink.primary = color;
        }
        refresh();
        return;
    }
    if (document.tool == Tool::Shape && document.shape == Shape::Polygon) {
        document.tool = Tool::Path;
        document.continuous_path = false;
    }
    if (document.tool == Tool::Path) {
        for (std::size_t index = 0; index < document.path.nodes.size(); ++index) {
            Point node = document.path.nodes[index];
            if (std::hypot(node.x - point.x, node.y - point.y) * (*canvas_).zoom() < 9) {
                point = node;
                break;
            }
        }
        document.add_path_node(point);
        if (!document.continuous_path && document.path.nodes.size() - document.path.start > 2 &&
            point.x == document.path.nodes[document.path.start].x &&
            point.y == document.path.nodes[document.path.start].y) {
            document.end_path_geometry();
        }
        refresh();
        return;
    }
    if (document.tool == Tool::Stamp) {
        document.commit_selection();
        if (document.stamp.pixels.empty()) {
            document.stamp = make_stamp(
                document.image, {static_cast<int>(point.x - 40), static_cast<int>(point.y - 40), 80, 80},
                document.stamp_shape, document.stamp_transparent, document.ink.secondary);
        } else {
            document.checkpoint();
            composite(document.image, document.stamp, static_cast<int>(point.x - document.stamp.width / 2.0),
                      static_cast<int>(point.y - document.stamp.height / 2.0));
        }
        refresh();
        return;
    }
    if (document.tool == Tool::Select || document.tool == Tool::Lasso) {
        Rect bounds{document.selection.x, document.selection.y, document.selection.image.width,
                    document.selection.image.height};
        moving_selection_ = document.selection.active && point_inside(bounds, point);
        if (moving_selection_) {
            selection_offset_ = {point.x - bounds.x, point.y - bounds.y};
        } else {
            document.commit_selection();
            lasso_ = {point};
        }
    } else if (document.tool == Tool::Shape &&
               (document.shape == Shape::Bezier || document.shape == Shape::Arc)) {
        if (!document.curve.base) {
            document.begin_curve(document.shape == Shape::Arc ? CurveKind::Arc : CurveKind::Bezier, point,
                                 secondary);
        }
    } else {
        document.commit_selection();
        if (document.tool != Tool::Shape &&
            !(document.tool == Tool::Stamp && document.stamp.pixels.empty())) {
            document.checkpoint();
        }
        eraser_.clear();
        material_.clear();
        if (document.tool == Tool::Pencil) {
            gesture_ink_.size = 1;
            gesture_ink_.brush = Brush::Round;
        }
    }
    dragging_ = true;
    (*canvas_).set_pointer_capture(true);
    move(point);
}
void Editor::move(Point point) {
    if (shift_ && document.tool == Tool::Shape && curve_handle_ < 0) {
        Point anchor = document.curve.base ? document.curve.geometry.start : start_;
        double dx = point.x - anchor.x, dy = point.y - anchor.y;
        if (document.shape == Shape::Line || document.shape == Shape::Bezier ||
            document.shape == Shape::Arc) {
            double angle = std::round(std::atan2(dy, dx) / (std::numbers::pi / 4)) * (std::numbers::pi / 4);
            double length = std::hypot(dx, dy);
            point = {anchor.x + length * std::cos(angle), anchor.y + length * std::sin(angle)};
        } else {
            double extent = std::max(std::abs(dx), std::abs(dy));
            point = {anchor.x + std::copysign(extent, dx), anchor.y + std::copysign(extent, dy)};
        }
    }
    current_ = point;
    if (curve_handle_ >= 0) {
        Point next{point.x - handle_offset_.x, point.y - handle_offset_.y};
        Point before = document.curve.geometry.handle(curve_handle_);
        if (std::hypot(next.x - before.x, next.y - before.y) > 1e-9) {
            if (!handle_checkpoint_) {
                document.checkpoint();
                handle_checkpoint_ = true;
            }
            document.curve.geometry.move_handle(curve_handle_, next);
            document.sync_curve();
        }
    } else if (moving_selection_) {
        document.selection.x = static_cast<int>(std::round(point.x - selection_offset_.x));
        document.selection.y = static_cast<int>(std::round(point.y - selection_offset_.y));
    } else if (document.tool == Tool::Select || document.tool == Tool::Lasso) {
        if (document.tool == Tool::Lasso) {
            lasso_.push_back(point);
        }
    } else if (document.tool == Tool::Shape) {
        if (document.curve.base) {
            preview_ = document.curve_image(&point);
        } else {
            preview_ = document.image;
            draw_shape(preview_, document.shape, start_, point, gesture_ink_, document.shape_outline,
                       document.shape_fill, document.shape_fill_brush);
        }
        preview_active_ = true;
    } else if (document.tool == Tool::Pencil) {
        pixel_line(document.image, last_, point, gesture_ink_);
    } else if (document.tool == Tool::Brush) {
        if (gesture_ink_.brush == Brush::Round || textured_brush(gesture_ink_.brush)) {
            material_.segment(document.image, last_, point, gesture_ink_);
        } else {
            stroke(document.image, last_, point, gesture_ink_);
        }
    } else if (document.tool == Tool::Eraser) {
        eraser_.segment(document.image, last_, point, gesture_ink_.size, false);
    } else if (document.tool == Tool::Fill) {
        flood(document.image, static_cast<int>(start_.x), static_cast<int>(start_.y), gesture_ink_);
        dragging_ = false;
        (*canvas_).set_pointer_capture(false);
    }
    if ((document.tool == Tool::Pencil || document.tool == Tool::Brush || document.tool == Tool::Eraser) &&
        curve_handle_ < 0 && !moving_selection_) {
        int margin = gesture_ink_.size + 3;
        Rect damage = rectangle(last_, point);
        damage.x -= margin;
        damage.y -= margin;
        damage.w += margin * 2;
        damage.h += margin * 2;
        publish_image(document.image, *canvas_, damage);
        last_ = point;
        update_status();
    } else {
        last_ = point;
        refresh();
    }
}
void Editor::end(Point point) {
    move(point);
    point = current_;
    if (curve_handle_ < 0 && !moving_selection_) {
        if (document.tool == Tool::Select || document.tool == Tool::Lasso) {
            Rect bounds = rectangle(start_, point);
            if (document.tool == Tool::Lasso) {
                Point low = start_, high = start_;
                for (std::size_t index = 0; index < lasso_.size(); ++index) {
                    low.x = std::min(low.x, lasso_[index].x);
                    low.y = std::min(low.y, lasso_[index].y);
                    high.x = std::max(high.x, lasso_[index].x);
                    high.y = std::max(high.y, lasso_[index].y);
                }
                bounds = rectangle(low, high);
            }
            if (std::hypot(point.x - start_.x, point.y - start_.y) > 0.5 || lasso_.size() > 3) {
                document.select(bounds, document.tool == Tool::Lasso ? lasso_ : std::vector<Point>{});
            }
        } else if (document.tool == Tool::Shape) {
            if (document.curve.base) {
                static_cast<void>(document.establish_curve(point));
            } else {
                document.checkpoint();
                document.image = preview_;
            }
        }
    }
    bool stroke_tool =
        document.tool == Tool::Pencil || document.tool == Tool::Brush || document.tool == Tool::Eraser;
    release_gesture();
    if (stroke_tool) {
        update_status();
    } else {
        refresh();
    }
}
void Editor::zoom(double factor, gf::Point anchor) {
    gui_drawing::PointF before = (*canvas_).client_to_bitmap(anchor);
    double value = std::clamp((*canvas_).zoom() * factor, 0.0625, 32.0);
    (*canvas_).set_view(value, {before.x - anchor.x / value, before.y - anchor.y / value});
    refresh();
}
void Editor::on_key_preview(gf::KeyEvent& event) {
    if (event.action != gf::KeyAction::down || (*menu_).is_open()) {
        return;
    }
    bool command = gf::has_modifier(event.modifiers, gf::Modifier::control) ||
                   gf::has_modifier(event.modifiers, gf::Modifier::meta);
    bool shift = gf::has_modifier(event.modifiers, gf::Modifier::shift);
    std::string action;
    if (command) {
        switch (event.physical_key) {
        case gf::PhysicalKey::n:
            action = "new";
            break;
        case gf::PhysicalKey::o:
            action = "open";
            break;
        case gf::PhysicalKey::s:
            action = shift ? "save-as" : "save";
            break;
        case gf::PhysicalKey::z:
            action = shift ? "redo" : "undo";
            break;
        case gf::PhysicalKey::y:
            action = "redo";
            break;
        case gf::PhysicalKey::c:
            action = "copy";
            break;
        case gf::PhysicalKey::x:
            action = "cut";
            break;
        case gf::PhysicalKey::v:
            action = "paste";
            break;
        case gf::PhysicalKey::a:
            action = "select-all";
            break;
        default:
            break;
        }
    } else if (window() && (*window()).focused_control() == canvas_) {
        if (event.physical_key == gf::PhysicalKey::escape) {
            action = "release";
        } else if (event.physical_key == gf::PhysicalKey::enter) {
            action = "finish-path";
        } else if (event.physical_key == gf::PhysicalKey::delete_forward ||
                   event.physical_key == gf::PhysicalKey::backspace) {
            action = "delete";
        } else if (document.selection.active) {
            int step = shift ? 10 : 1;
            if (event.physical_key == gf::PhysicalKey::left) {
                document.selection.x -= step;
            } else if (event.physical_key == gf::PhysicalKey::right) {
                document.selection.x += step;
            } else if (event.physical_key == gf::PhysicalKey::up) {
                document.selection.y -= step;
            } else if (event.physical_key == gf::PhysicalKey::down) {
                document.selection.y += step;
            } else {
                return;
            }
            event.handled = true;
            refresh();
            return;
        }
    }
    if (!action.empty()) {
        event.handled = true;
        execute(action);
    }
}
gf::HostServices& Editor::services() {
    if (!window() || !(*window()).host_services()) {
        throw std::runtime_error("Native host services are not attached");
    }
    return *(*window()).host_services();
}
gf::HostDialogResult Editor::dialog(gf::HostDialogRequestPayload payload) {
    gf::HostDialogRequest request;
    request.request_id = next_dialog_id_++;
    request.payload = std::move(payload);
    gf::HostDialogResult result = services().show_dialog(request);
    if (!result.status.accepted()) {
        throw std::runtime_error("The native dialog service could not complete the request");
    }
    return result;
}
void Editor::error(const std::string& message) {
    if (status_) {
        (*status_).set_text(message);
    }
    if (window() && (*window()).host_services()) {
        gf::HostMessageDialogRequest request;
        request.title = "Rainstar Paint";
        request.message = message;
        request.icon = gf::HostMessageIcon::error;
        try {
            static_cast<void>(dialog(request));
        } catch (const std::exception&) {
            // The status line remains usable even if native modal services fail.
        }
    }
}

bool Editor::can_replace() {
    if (!document.dirty()) {
        return true;
    }
    gf::HostMessageDialogRequest request;
    request.title = "Save changes?";
    request.message =
        "Save changes to " + (document.filename.empty() ? std::string("Untitled") : document.filename) + "?";
    request.buttons = gf::HostMessageButtons::yes_no_cancel;
    request.default_choice = gf::HostDialogChoice::cancel;
    try {
        gf::HostMessageDialogResult result = std::get<gf::HostMessageDialogResult>(dialog(request).payload);
        if (result.choice == gf::HostDialogChoice::no) {
            return true;
        }
        return result.choice == gf::HostDialogChoice::yes && save(false);
    } catch (const std::exception& exception) {
        error(exception.what());
        return false;
    }
}
void Editor::open_file(const std::string& path) {
    ImageContainer image = load_container(path);
    finish_controls();
    document.replace_container(std::move(image), path);
    (*canvas_).set_view(1, {-16, -16});
    refresh();
}
bool Editor::save(bool save_as) {
    std::string path = document.filename;
    if (save_as || path.empty() || !writable_image_path(path)) {
        gf::HostSaveFileDialogRequest request;
        request.title = "Save image";
        request.suggested_name =
            path.empty() ? "Untitled.png" : std::filesystem::path(path).filename().string();
        request.default_extension = "png";
        request.filters = {
            {"Images", {"png", "bmp", "jpg", "jpeg", "gif", "tif", "tiff", "webp", "ico", "cur"}}};
        gf::HostPathDialogResult result = std::get<gf::HostPathDialogResult>(dialog(request).payload);
        if (result.outcome != gf::HostDialogOutcome::accepted || result.paths.empty()) {
            return false;
        }
        path = result.paths.front();
    }
    document.sync_curve();
    document.sync_path();
    // Saving a selection or retained curve must not release its editing session.
    std::string extension = image_extension(path);
    if (extension == ".ico" || extension == ".cur") {
        save_container(document.output_container(extension == ".cur"), path);
    } else {
        save_image(document.output_image(), path);
    }
    document.filename = path;
    document.saved_revision = document.revision;
    refresh();
    return true;
}
void Editor::copy() {
    Image image = document.selection.active ? document.selection.image : document.visible_image();
    gf::HostImageView view{static_cast<unsigned>(image.width), static_cast<unsigned>(image.height),
                           static_cast<std::uint64_t>(image.width) * 4,
                           std::as_bytes(std::span<const Color>(image.pixels))};
    if (!services().write_clipboard_image(view).accepted()) {
        throw std::runtime_error("Could not copy the image to the clipboard");
    }
}
void Editor::paste() {
    gf::HostClipboardImageResult result = services().read_clipboard_image();
    if (!result.status.accepted()) {
        throw std::runtime_error("Could not read the image clipboard");
    }
    if (!result.has_image) {
        return;
    }
    Image image;
    image.reset(result.image.width, result.image.height);
    for (int y = 0; y < image.height; ++y) {
        std::memcpy(image.pixels.data() + static_cast<std::size_t>(y) * image.width,
                    result.image.pixels.data() + static_cast<std::size_t>(y) * result.image.row_bytes,
                    static_cast<std::size_t>(image.width) * 4);
    }
    release_gesture();
    document.paste(image);
    refresh();
}
void Editor::edit_color(bool secondary) {
    Color color = secondary ? document.ink.secondary : document.ink.primary;
    gf::HostColorDialogRequest request;
    request.title = secondary ? "Color 2" : "Color 1";
    request.initial_rgba = (static_cast<std::uint32_t>(color.r) << 24) |
                           (static_cast<std::uint32_t>(color.g) << 16) |
                           (static_cast<std::uint32_t>(color.b) << 8) | color.a;
    request.allow_alpha = true;
    gf::HostColorDialogResult result = std::get<gf::HostColorDialogResult>(dialog(request).payload);
    if (result.outcome != gf::HostDialogOutcome::accepted) {
        return;
    }
    color = {static_cast<std::uint8_t>(result.rgba >> 24), static_cast<std::uint8_t>(result.rgba >> 16),
             static_cast<std::uint8_t>(result.rgba >> 8), static_cast<std::uint8_t>(result.rgba)};
    if (secondary) {
        document.ink.secondary = color;
    } else {
        document.ink.primary = color;
    }
    document.sync_curve();
    document.sync_path();
}
void Editor::execute(const std::string& command) {
    try {
        if (command.starts_with("tool-")) {
            choose_tool(tools[std::stoul(command.substr(5))]);
            return;
        }
        if (command == "new") {
            if (can_replace()) {
                finish_controls();
                document.new_image();
            }
        } else if (command == "open") {
            if (!can_replace()) {
                return;
            }
            gf::HostOpenFileDialogRequest request;
            request.title = "Open image";
            gf::HostPathDialogResult result = std::get<gf::HostPathDialogResult>(dialog(request).payload);
            if (result.outcome == gf::HostDialogOutcome::accepted && !result.paths.empty()) {
                open_file(result.paths.front());
            }
        } else if (command == "save" || command == "save-as") {
            static_cast<void>(save(command == "save-as"));
        }
#if RAINSTAR_FORMS_NATIVE_PRINT
        else if (command == "print") {
            static_cast<void>(print_image(document.output_image()));
        } else if (command == "page-setup") {
            page_setup();
        }
#endif
        else if (command == "quit") {
            static_cast<void>(handle_.request_close());
        } else if (command == "undo") {
            release_gesture();
            document.undo();
        } else if (command == "redo") {
            release_gesture();
            document.redo();
        } else if (command == "copy") {
            copy();
        } else if (command == "cut") {
            copy();
            if (!document.selection.active) {
                document.select_all();
            }
            document.delete_selection();
        } else if (command == "paste") {
            paste();
        } else if (command == "select-all") {
            finish_controls();
            document.select_all();
            document.tool = Tool::Select;
        } else if (command == "invert-selection") {
            document.invert_selection();
        } else if (command == "delete") {
            document.delete_selection();
        } else if (command == "crop") {
            release_gesture();
            document.crop();
        } else if (command == "rotate-right" || command == "rotate-left") {
            finish_controls();
            document.rotate(command == "rotate-right" ? 1 : 3);
        } else if (command == "flip-horizontal" || command == "flip-vertical") {
            finish_controls();
            document.flip(command == "flip-horizontal");
        } else if (command == "invert") {
            finish_controls();
            document.invert_colors();
        } else if (command == "release") {
            finish_controls();
            document.stamp = {};
        } else if (command == "finish-path") {
            if (document.tool == Tool::Path) {
                document.end_path_geometry();
            } else {
                finish_controls();
            }
        } else if (command == "outline") {
            document.shape_outline = !document.shape_outline;
        } else if (command == "fill") {
            document.shape_fill = !document.shape_fill;
        } else if (command == "continuous-path") {
            document.continuous_path = !document.continuous_path;
        } else if (command == "transparent-pattern") {
            document.ink.transparent_pattern = !document.ink.transparent_pattern;
        } else if (command == "transparent-selection") {
            document.transparent_selection = !document.transparent_selection;
        } else if (command == "primary" || command == "secondary") {
            edit_color(command == "secondary");
        } else if (command == "zoom-in" || command == "zoom-out") {
            zoom(command == "zoom-in" ? 2 : 0.5, {0, 0});
        } else if (command == "actual-size") {
            (*canvas_).set_view(1, {-16, -16});
        } else if (command == "fit") {
            gf::Rect bounds = (*canvas_).committed_arranged_bounds();
            double scale = std::clamp(std::min((bounds.width - 32) / document.image.width,
                                               (bounds.height - 32) / document.image.height),
                                      0.0625, 32.0);
            (*canvas_).set_view(scale, {-16 / scale, -16 / scale});
        } else if (command == "help" || command == "about") {
            gf::HostMessageDialogRequest request;
            request.title = "Rainstar Paint";
            request.message =
                command == "about"
                    ? "For the people who keep making things.\n\nAuthor: Astra\nSponsor: "
                      "Rainstar\n\nGUI.Forms integration build"
                    : "Draw with the left button; the right button uses Color 2. Drag selections to move "
                      "them. Drag Bézier and arc handles after setting their line. Escape releases editing "
                      "controls. Enter or right-click ends a path run. Middle-drag pans; Ctrl/Command + "
                      "wheel zooms.\n\nThis integration build is still being ported. Text editing, CONV "
                      "reshape and rotation controls, atlas UI and desktop commands remain in the comparison "
                      "frontend.";
            static_cast<void>(dialog(request));
        }
        document.sync_curve();
        document.sync_path();
        refresh();
    } catch (const std::exception& exception) {
        error(exception.what());
    }
}
} // namespace paint::forms
