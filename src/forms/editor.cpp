#include "forms/editor.hpp"
#include "codecs.hpp"
#include "forms/display.hpp"
#include "platform.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <gui_forms/host.hpp>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <stdexcept>
namespace paint::forms {
namespace gf = gui_forms;
namespace {
const Tool tools[] = {Tool::Select,    Tool::Lasso, Tool::Pencil, Tool::Fill, Tool::Eraser, Tool::Picker,
                      Tool::Magnifier, Tool::Brush, Tool::Shape,  Tool::Path, Tool::Stamp};
class ZoomTrackBar final : public gf::TrackBar {
  public:
    explicit ZoomTrackBar(gf::StableId id) : TrackBar(std::move(id)) {}
    gf::SemanticDescriptor semantic_descriptor() const override {
        gf::SemanticDescriptor descriptor = TrackBar::semantic_descriptor();
        descriptor.numeric_value = std::exp2(value()) * 100;
        descriptor.minimum_value = 6.25;
        descriptor.maximum_value = 3200;
        descriptor.value = std::to_string(static_cast<int>(std::lround(std::exp2(value()) * 100))) + "%";
        return descriptor;
    }
    bool on_semantic_action(gf::SemanticAction action, std::string_view text) override {
        if (action == gf::SemanticAction::set_value) {
            try {
                std::size_t consumed = 0;
                double percentage = std::stod(std::string(text), &consumed);
                if (consumed != text.size() || !std::isfinite(percentage) || percentage < 6.25 ||
                    percentage > 3200) {
                    return false;
                }
                set_value(std::log2(percentage / 100));
                return true;
            } catch (const std::exception&) {
                return false;
            }
        }
        return TrackBar::on_semantic_action(action, text);
    }
};
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
    ribbon_ = gf::make_control<Ribbon>(gf::StableId("ribbon"),
                                       std::static_pointer_cast<Editor>(shared_from_this()));
    add_child(ribbon_);
    menu_ = gf::make_control<gf::MenuStrip>(gf::StableId("menus"));
    gf::ThemeDefinition file_theme = gf::windows_professional_theme_definition();
    file_theme.id = "rainstar-file-tab";
    file_theme.compatibility.border = gf::Color::rgba(45, 105, 172);
    for (std::size_t state = 0; state < gf::control_surface_state_count; ++state) {
        gf::SurfaceMaterial surface;
        surface.fills = {gf::MaterialFillLayer::linear(
            {0, 0}, {0, 1}, {{0, gf::Color::rgba(70, 139, 210)}, {1, gf::Color::rgba(38, 105, 177)}})};
        file_theme.roles[static_cast<std::size_t>(gf::ControlVisualRole::panel)].ordinary[state].material =
            surface;
        gf::ControlRoleRecipes& item =
            file_theme.roles[static_cast<std::size_t>(gf::ControlVisualRole::menu_item)];
        item.ordinary[state].material = surface;
        item.ordinary[state].text = gf::Color::rgba(255, 255, 255);
        item.selected[state].material = surface;
        item.selected[state].text = gf::Color::rgba(255, 255, 255);
    }
    (*menu_).set_theme_override(gf::Theme::create(std::move(file_theme)));
    (*menu_).set_item_padding(17);
    (*menu_).set_items({{"file",
                         "File",
                         {menu_item("new", "New"), menu_item("open", "Open…"), menu_item("save", "Save"),
                          menu_item("save-as", "Save as…"),
#if RAINSTAR_FORMS_NATIVE_PRINT
                          menu_item("print", "Print…"), menu_item("page-setup", "Page setup…"),
#endif
                          menu_item("quit", "Exit")}}});
    add_child(menu_);
    status_ = gf::make_control<gf::Label>(gf::StableId("status"));
    cursor_status_ = gf::make_control<gf::Label>(gf::StableId("cursor-status"));
    dimensions_status_ = gf::make_control<gf::Label>(gf::StableId("dimensions-status"));
    selection_status_ = gf::make_control<gf::Label>(gf::StableId("selection-status"));
    for (const std::shared_ptr<gf::Label>& label :
         {status_, cursor_status_, dimensions_status_, selection_status_}) {
        (*label).set_font({gf::FontRole::control, 12, 400, false, 0.08});
        add_child(label);
    }
    zoom_out_ = gf::make_control<gf::Button>(gf::StableId("status-zoom-out"), "−");
    zoom_in_ = gf::make_control<gf::Button>(gf::StableId("status-zoom-in"), "+");
    zoom_reset_ = gf::make_control<gf::Button>(gf::StableId("status-zoom-reset"), "100%");
    for (const std::shared_ptr<gf::Button>& button : {zoom_out_, zoom_in_, zoom_reset_}) {
        (*button).set_theme_override(ribbon_theme());
        (*button).set_content_padding({2, 1, 2, 1});
        (*button).set_font({gf::FontRole::control, button == zoom_reset_ ? 12.0 : 18.0, 400, false});
        subscriptions_.push_back((*button).clicked().subscribe(
            *this, gf::Delegate<gf::ButtonBase&>::bind<Editor, &Editor::status_clicked>(*this)));
        add_child(button);
    }
    (*zoom_out_).set_accessible_name("Zoom out");
    (*zoom_in_).set_accessible_name("Zoom in");
    (*zoom_reset_).set_accessible_name("Zoom percentage; reset to 100%");
    zoom_slider_ = gf::make_control<ZoomTrackBar>(gf::StableId("zoom-slider"));
    (*zoom_slider_).set_range(-4, 5);
    (*zoom_slider_).set_value(0);
    (*zoom_slider_).set_small_change(0.25);
    (*zoom_slider_).set_large_change(1);
    (*zoom_slider_).set_show_ticks(false);
    (*zoom_slider_).set_visual_style(gf::TrackBarVisualStyle::compact);
    (*zoom_slider_).set_accessible_name("Zoom percentage");
    subscriptions_.push_back(
        (*zoom_slider_)
            .value_changed()
            .subscribe(*this, gf::Delegate<double>::bind<Editor, &Editor::zoom_slider_changed>(*this)));
    add_child(zoom_slider_);
    refresh();
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
    set_child_layout(ribbon_, {0, 0, bounds.width, 143});
    set_child_layout(menu_, {0, 0, 56, 27});
    double ruler = show_rulers ? 20 : 0;
    double footer = show_status ? 30 : 0;
    set_child_layout(canvas_, {ruler, 143 + ruler, bounds.width - ruler,
                               std::max(1.0, bounds.height - 143 - ruler - footer)});
    for (const std::shared_ptr<gf::Control>& control : std::vector<std::shared_ptr<gf::Control>>{
             status_, cursor_status_, selection_status_, dimensions_status_, zoom_reset_, zoom_out_,
             zoom_slider_, zoom_in_}) {
        (*control).set_visible(show_status);
    }
    double y = bounds.height - 28;
    set_child_layout(status_, {12, y, 230, 26});
    set_child_layout(cursor_status_, {260, y, 165, 26});
    set_child_layout(selection_status_, {445, y, 164, 26});
    set_child_layout(dimensions_status_, {632, y, 210, 26});
    set_child_layout(zoom_reset_, {bounds.width - 292, y + 1, 65, 25});
    set_child_layout(zoom_out_, {bounds.width - 221, y + 1, 26, 25});
    set_child_layout(zoom_slider_, {bounds.width - 189, y + 1, 148, 25});
    set_child_layout(zoom_in_, {bounds.width - 34, y + 1, 26, 25});
}
void Editor::on_paint(gf::Painter& painter, gf::Rect) {
    gf::Rect bounds = committed_arranged_bounds();
    painter.fill_rect({0, 0, bounds.width, bounds.height}, gf::Color::rgba(232, 240, 249));
    if (show_rulers) {
        gf::Rect area = (*canvas_).committed_arranged_bounds();
        gui_drawing::PointF origin = (*canvas_).view_origin();
        double scale = (*canvas_).zoom();
        double step = 1;
        while (step * scale < 50) {
            step *= 2;
        }
        const gf::FontSpec font{gf::FontRole::control, 10, 400, false};
        const gf::Color color = gf::Color::rgba(81, 103, 127);
        painter.fill_rect({20, 143, area.width, 20}, gf::Color::rgba(245, 248, 252));
        painter.fill_rect({0, 163, 20, area.height}, gf::Color::rgba(245, 248, 252));
        for (int axis = 0; axis < 2; ++axis) {
            double start = axis == 0 ? origin.x : origin.y;
            double length = axis == 0 ? area.width : area.height;
            double end = start + length / scale;
            for (double value = std::ceil(start / (step / 5)) * (step / 5); value <= end; value += step / 5) {
                double position = (value - start) * scale;
                bool major = std::abs(value / step - std::round(value / step)) < 0.001;
                if (axis == 0) {
                    painter.draw_line({20 + position, 163}, {20 + position, major ? 155.0 : 159.0}, color, 1);
                    if (major) {
                        painter.draw_text_utf8({23 + position, 153},
                                               std::to_string(static_cast<int>(std::round(value))), font,
                                               color);
                    }
                } else {
                    painter.draw_line({20, 163 + position}, {major ? 12.0 : 16.0, 163 + position}, color, 1);
                    if (major) {
                        painter.draw_text_utf8({1, 160 + position},
                                               std::to_string(static_cast<int>(std::round(value))), font,
                                               color);
                    }
                }
            }
        }
        painter.draw_line({20, 143}, {20, 163 + area.height}, color, 1);
        painter.draw_line({0, 163}, {20 + area.width, 163}, color, 1);
    }
    if (!show_status) {
        return;
    }
    double y = bounds.height - 30;
    painter.draw_line({0, y}, {bounds.width, y}, gf::Color::rgba(172, 193, 214), 1);
    for (double x : {249.0, 434.0, 621.0, bounds.width - 303}) {
        painter.draw_line({x, y + 5}, {x, bounds.height - 5}, gf::Color::rgba(193, 208, 224), 1);
    }
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
    if (show_grid && (*canvas_).zoom() >= 4) {
        double scale = (*canvas_).zoom();
        gui_drawing::PointF origin = (*canvas_).view_origin();
        gf::Rect area = (*canvas_).committed_arranged_bounds();
        int left = std::max(0, static_cast<int>(std::floor(origin.x)));
        int top = std::max(0, static_cast<int>(std::floor(origin.y)));
        int right =
            std::min(document.image.width, static_cast<int>(std::ceil(origin.x + area.width / scale)));
        int bottom =
            std::min(document.image.height, static_cast<int>(std::ceil(origin.y + area.height / scale)));
        gf::Color color = gf::Color::rgba(90, 110, 135, 85);
        for (int x = left; x <= right; ++x) {
            painter.draw_line(screen({static_cast<double>(x), static_cast<double>(top)}),
                              screen({static_cast<double>(x), static_cast<double>(bottom)}), color, 1);
        }
        for (int y = top; y <= bottom; ++y) {
            painter.draw_line(screen({static_cast<double>(left), static_cast<double>(y)}),
                              screen({static_cast<double>(right), static_cast<double>(y)}), color, 1);
        }
    }
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
    if (preview_active_) {
        publish_image(preview_, *canvas_);
    } else if (document.selection.active) {
        publish_image(document.visible_image(), *canvas_);
    } else {
        publish_image(document.image, *canvas_);
    }
    if (ribbon_) {
        (*ribbon_).synchronize();
    }
    update_status();
    invalidate(gf::Dirty::paint);
    (*canvas_).invalidate(gf::Dirty::paint);
}
void Editor::update_cursor_status() {
    if (!cursor_status_) {
        return;
    }
    std::string text = "X: —   Y: —";
    if (cursor_client_) {
        gui_drawing::PointF point = (*canvas_).client_to_bitmap(*cursor_client_);
        text = "X: " + std::to_string(static_cast<int>(std::floor(point.x))) +
               "   Y: " + std::to_string(static_cast<int>(std::floor(point.y))) + " px";
    }
    (*cursor_status_).set_text(text);
}
void Editor::update_status() {
    if (!status_) {
        return;
    }
    (*status_).set_text((document.filename.empty()
                             ? "Untitled"
                             : std::filesystem::path(document.filename).filename().string()) +
                        (document.dirty() ? " *" : ""));
    (*dimensions_status_)
        .set_text(std::to_string(document.image.width) + " × " + std::to_string(document.image.height) +
                  " px");
    std::string selection;
    if (document.selection.active) {
        selection = std::to_string(document.selection.image.width) + " × " +
                    std::to_string(document.selection.image.height) + " px selected";
    } else if (dragging_ && (document.tool == Tool::Select || document.tool == Tool::Lasso)) {
        Rect bounds = rectangle(start_, current_);
        selection = std::to_string(bounds.w) + " × " + std::to_string(bounds.h) + " px selected";
    }
    (*selection_status_).set_text(selection);
    double percent = (*canvas_).zoom() * 100;
    std::ostringstream label;
    label << std::fixed << std::setprecision(percent < 10 ? 2 : 0) << percent << "%";
    (*zoom_reset_).set_text(label.str());
    synchronizing_zoom_ = true;
    (*zoom_slider_).set_value(std::log2((*canvas_).zoom()));
    synchronizing_zoom_ = false;
    update_cursor_status();
}
void Editor::zoom_slider_changed(double value) {
    if (synchronizing_zoom_) {
        return;
    }
    gf::Rect bounds = (*canvas_).committed_arranged_bounds();
    zoom(std::exp2(value) / (*canvas_).zoom(), {bounds.width / 2, bounds.height / 2});
}
void Editor::status_clicked(gf::ButtonBase& button) {
    std::string_view id = button.stable_id().value();
    if (id == "status-zoom-reset") {
        execute("actual-size");
        return;
    }
    gf::Rect bounds = (*canvas_).committed_arranged_bounds();
    zoom(id == "status-zoom-in" ? 2 : 0.5, {bounds.width / 2, bounds.height / 2});
}
void Editor::command_invoked(const gf::CommandInvocation& invocation) {
    execute(invocation.command_id);
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
void Editor::choose_shape(Shape shape) {
    finish_controls();
    document.shape = shape;
    document.tool = Tool::Shape;
    refresh();
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
        if (event.action == gf::PointerAction::leave && !(*canvas_).has_pointer_capture()) {
            cursor_client_.reset();
        } else {
            cursor_client_ = client;
        }
        update_cursor_status();
        if (event.action == gf::PointerAction::wheel) {
            if (gf::has_modifier(event.modifiers, gf::Modifier::control) ||
                gf::has_modifier(event.modifiers, gf::Modifier::meta)) {
                zoom(event.wheel_delta.y >= 0 ? 1.25 : 0.8, client);
            } else {
                gui_drawing::PointF origin = (*canvas_).view_origin();
                origin.x -= event.wheel_delta.x * 30 / (*canvas_).zoom();
                origin.y -= event.wheel_delta.y * 30 / (*canvas_).zoom();
                (*canvas_).set_view_origin(origin);
                update_cursor_status();
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
                update_cursor_status();
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
void Editor::open_editor_dialog(EditorDialogKind kind, bool secondary) {
    if (!attached_window()) {
        return;
    }
    (*ribbon_).close_popup();
    close_editor_dialog();
    editor_dialog_ = gf::make_control<EditorDialog>(
        gf::StableId("editor-dialog"), std::static_pointer_cast<Editor>(shared_from_this()), kind, secondary);
    editor_dialog_popup_ = (*attached_window()).open_popup(shared_from_this(), editor_dialog_);
    editor_dialog_focus_ = (*attached_window()).begin_focus_scope(editor_dialog_);
}
void Editor::close_editor_dialog() {
    if (editor_dialog_focus_ && attached_window()) {
        static_cast<void>((*attached_window()).end_focus_scope(editor_dialog_focus_));
        editor_dialog_focus_ = {};
    }
    editor_dialog_popup_.disconnect();
    editor_dialog_.reset();
}
void Editor::edit_color(bool secondary) {
    open_editor_dialog(EditorDialogKind::color, secondary);
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
        } else if (command == "resize") {
            open_editor_dialog(EditorDialogKind::resize);
        } else if (command == "rotate-right" || command == "rotate-left" || command == "rotate-180") {
            finish_controls();
            document.rotate(command == "rotate-right" ? 1 : command == "rotate-left" ? 3 : 2);
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
        } else if (command == "show-rulers" || command == "show-grid" || command == "show-status") {
            if (command == "show-rulers") {
                show_rulers = !show_rulers;
            }
            if (command == "show-grid") {
                show_grid = !show_grid;
            }
            if (command == "show-status") {
                show_status = !show_status;
            }
            invalidate(gf::Dirty::layout | gf::Dirty::paint);
        } else if (command == "full-screen") {
            gf::HostServiceStatus result = handle_.toggle_full_screen();
            if (!result.accepted()) {
                error("Full screen is unavailable from this window host.");
            }
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
                    : "Draw with the left button; the right button uses Alt. Drag selections to move "
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
