#include "forms/ribbon.hpp"
#include "codecs.hpp"
#include "forms/editor.hpp"
#include "forms/ribbon_icons.hpp"
#include "material.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <gui_forms/delegate.hpp>
namespace paint::forms {
namespace gf = gui_forms;
namespace {
const Tool ribbon_tools[] = {Tool::Select, Tool::Lasso,  Tool::Pencil,    Tool::Fill,
                             Tool::Eraser, Tool::Picker, Tool::Magnifier, Tool::Brush,
                             Tool::Shape,  Tool::Path,   Tool::Stamp};
const Shape home_shapes[] = {
    Shape::Line,    Shape::Bezier,     Shape::Oval,          Shape::Rectangle,      Shape::RoundedRectangle,
    Shape::Polygon, Shape::Triangle,   Shape::RightTriangle, Shape::Diamond,        Shape::Pentagon,
    Shape::Hexagon, Shape::RightArrow, Shape::LeftArrow,     Shape::UpArrow,        Shape::DownArrow,
    Shape::Star4,   Shape::Star5,      Shape::Star6,         Shape::RoundedCallout, Shape::Heart,
    Shape::Arc};
class WeightButton final : public gf::Button {
  public:
    WeightButton(gf::StableId id, std::string text, int weight)
        : Button(std::move(id), std::move(text)), weight_(weight) {}
    void on_paint(gf::Painter& painter, gf::Rect damage) override {
        Button::on_paint(painter, damage);
        double height = committed_arranged_bounds().height;
        painter.draw_line({68, height / 2}, {145, height / 2}, gf::Color::rgba(36, 60, 85),
                          std::min(12, weight_));
    }

  private:
    int weight_;
};
gf::SurfaceMaterial ribbon_material(bool selected, bool hot, bool pressed) {
    gf::SurfaceMaterial material;
    material.fills = {gf::MaterialFillLayer::solid(gf::Color::rgba(0, 0, 0, 0))};
    if (selected || hot || pressed) {
        gf::Color top = selected ? gf::Color::rgba(255, 237, 186) : gf::Color::rgba(255, 249, 228);
        gf::Color bottom = selected ? gf::Color::rgba(255, 207, 102) : gf::Color::rgba(255, 228, 157);
        if (pressed) {
            top = gf::Color::rgba(246, 193, 91);
            bottom = gf::Color::rgba(255, 226, 151);
        }
        material.fills.push_back(gf::MaterialFillLayer::linear({0, 0}, {0, 1}, {{0, top}, {1, bottom}}));
        material.border =
            gf::MaterialBorder{selected ? gf::Color::rgba(196, 147, 50) : gf::Color::rgba(219, 182, 109), 1};
        material.corner_radius = 2;
        material.keylines.push_back({gf::MaterialEdge::top, gf::Color::rgba(255, 255, 240), 1, 1});
    }
    return material;
}
} // namespace
Color ribbon_color(int index) {
    return office_color(std::clamp(index, 0, 29));
}
std::shared_ptr<const gf::Theme> ribbon_theme() {
    gf::ThemeDefinition definition = gf::windows_professional_theme_definition();
    definition.id = "rainstar-classic-ribbon";
    for (gf::ControlVisualRole role :
         {gf::ControlVisualRole::button, gf::ControlVisualRole::command_button}) {
        gf::ControlRoleRecipes& recipes = definition.roles[static_cast<std::size_t>(role)];
        for (std::size_t state = 0; state < gf::control_surface_state_count; ++state) {
            bool hot = state == static_cast<std::size_t>(gf::ControlSurfaceState::hot);
            bool pressed = state == static_cast<std::size_t>(gf::ControlSurfaceState::pressed);
            recipes.ordinary[state].material = ribbon_material(false, hot, pressed);
            recipes.selected[state].material = ribbon_material(true, hot, pressed);
            recipes.ordinary[state].pressed_content_offset = {};
            recipes.selected[state].pressed_content_offset = {};
        }
    }
    return gf::Theme::create(std::move(definition));
}
SwatchButton::SwatchButton(gf::StableId id, std::string text, Color color)
    : Button(std::move(id), std::move(text)), color_(color) {}
void SwatchButton::set_color(Color color) {
    color_ = color;
    invalidate(gf::Dirty::paint);
}
void SwatchButton::on_paint(gf::Painter& painter, gf::Rect damage) {
    Button::on_paint(painter, damage);
    gf::Rect bounds = committed_arranged_bounds();
    gf::Rect swatch = text().empty() ? gf::Rect{3, 3, bounds.width - 6, bounds.height - 6}
                                     : gf::Rect{9, 7, bounds.width - 18, 30};
    painter.fill_rect(swatch, gf::Color::rgba(color_.r, color_.g, color_.b, color_.a));
    painter.stroke_rect(swatch, gf::Color::rgba(127, 145, 163), 1);
}
Ribbon::Ribbon(gf::StableId id, std::weak_ptr<Editor> editor)
    : Control(std::move(id)), editor_(std::move(editor)) {
    set_theme_override(ribbon_theme());
}
std::shared_ptr<gf::Button> Ribbon::button(const std::string& id, const std::string& text, int icon,
                                           gf::Rect bounds, bool tall, bool disclosure, bool split) {
    std::shared_ptr<gf::Button> result;
    if (disclosure) {
        std::shared_ptr<gf::DropDownButton> dropdown = gf::make_control<gf::DropDownButton>(
            gf::StableId(id), text, split ? gf::DropDownButtonMode::split : gf::DropDownButtonMode::menu);
        if (tall) {
            (*dropdown).set_drop_down_edge(gf::DropDownButtonEdge::bottom);
        }
        (*dropdown).set_drop_down_width(tall ? 14 : 16);
        subscriptions_.push_back((*dropdown).drop_down_requested().subscribe(
            *this, gf::Delegate<gf::DropDownButton&>::bind<Ribbon, &Ribbon::dropdown>(*this)));
        subscriptions_.push_back((*dropdown).drop_down_close_requested().subscribe(
            *this, gf::Delegate<gf::DropDownButton&>::bind<Ribbon, &Ribbon::dropdown>(*this)));
        result = dropdown;
    } else {
        result = gf::make_control<gf::Button>(gf::StableId(id), text);
    }
    if (id.ends_with("-tab")) {
        gf::ThemeDefinition tab = (*ribbon_theme()).definition();
        tab.id = "rainstar-home-tab";
        for (std::size_t state = 0; state < gf::control_surface_state_count; ++state) {
            gf::SurfaceMaterial& material =
                tab.roles[static_cast<std::size_t>(gf::ControlVisualRole::button)].selected[state].material;
            material.fills = {gf::MaterialFillLayer::solid(gf::Color::rgba(250, 253, 255))};
            material.border = gf::MaterialBorder{gf::Color::rgba(178, 198, 219), 1};
            material.corner_radius = 2;
        }
        (*result).set_theme_override(gf::Theme::create(std::move(tab)));
    }
    (*result).set_requested_bounds(bounds);
    (*result).set_accessible_name(text.empty() ? id : text);
    (*result).set_use_mnemonic(false);
    (*result).set_font({gf::FontRole::control, 13, 400, false, 0.05});
    (*result).set_content_padding(tall ? gf::Insets{3, 4, 3, disclosure ? 16.0 : 4.0}
                                       : gf::Insets{4, 2, disclosure ? 18.0 : 4.0, 2});
    (*result).set_text_image_relation(tall ? gf::TextImageRelation::image_above_text
                                           : gf::TextImageRelation::image_before_text);
    (*result).set_image_gap(id == "paste" ? 1 : 4);
    (*result).set_text_line_spacing(1.1);
    (*result).set_text_alignment(tall || text.empty() ? gf::ContentAlignment::middle_center
                                                      : gf::ContentAlignment::middle_left);
    (*result).set_image_alignment(tall || text.empty() ? gf::ContentAlignment::middle_center
                                                       : gf::ContentAlignment::middle_left);
    if (icon >= 0) {
        (*result).set_image_key(std::to_string(icon));
    }
    subscriptions_.push_back((*result).clicked().subscribe(
        *this, gf::Delegate<gf::ButtonBase&>::bind<Ribbon, &Ribbon::clicked>(*this)));
    button_pages_.push_back(building_page_);
    buttons_.push_back(result);
    add_child(result);
    return result;
}
void Ribbon::initialize_control_tree() {
    building_page_ = 0;
    button("home-tab", "Home", -1, {57, 0, 61, 27});
    button("view-tab", "View", -1, {118, 0, 63, 27});
    button("patterns-tab", "Patterns & tools", -1, {184, 0, 144, 27});
    button("tool-tab", "Pencil", -1, {330, 0, 142, 27});
    button("help", "?", -1, {1240, 0, 32, 27});
    building_page_ = 1;
    button("save", "Save", 14, {5, 35, 64, 34});
    button("undo", "", 15, {5, 81, 32, 32});
    button("redo", "", 16, {39, 81, 32, 32});
    button("paste", "Paste", 0, {80, 34, 56, 83}, true, true, true);
    button("tool-0", "", 3, {142, 37, 36, 30}, false, true, true);
    button("crop", "", 4, {180, 37, 28, 30});
    button("resize", "", 5, {210, 37, 28, 30});
    button("rotate-menu", "", 6, {240, 37, 36, 30}, false, true);
    button("cut", "", 1, {278, 37, 28, 30});
    button("copy", "", 2, {308, 37, 28, 30});
    button("tool-2", "", 7, {144, 80, 28, 30});
    button("tool-3", "", 8, {176, 80, 28, 30});
    std::shared_ptr<gf::Button> text = button("text", "", 9, {208, 80, 28, 30});
    (*text).set_enabled(false);
    (*text).set_accessible_name("Text — port in progress");
    button("tool-4", "", 10, {240, 80, 28, 30});
    button("tool-5", "", 11, {272, 80, 28, 30});
    button("tool-6", "", 12, {304, 80, 28, 30});
    button("brush-menu", "Brushes", 13, {344, 34, 61, 83}, true, true);
    button("tool-10", "Stamp", 20, {410, 34, 59, 83}, true, true, true);
    button("tool-9", "Path", 21, {475, 34, 58, 83}, true, true, true);
    for (std::size_t i = 0; i < std::size(home_shapes); ++i) {
        button("shape-" + std::to_string(static_cast<int>(home_shapes[i])), "",
               100 + static_cast<int>(home_shapes[i]), {546 + 25.0 * (i % 7), 36 + 25.0 * (i / 7), 25, 25});
    }
    button("shapes-menu", "", -1, {722, 36, 18, 75}, false, true);
    button("outline-menu", "Edge", -1, {747, 39, 64, 28}, false, true);
    button("fill-menu", "Fill", -1, {747, 79, 64, 28}, false, true);
    button("size-menu", "", 23, {824, 34, 48, 83}, true, true);
    primary_ = gf::make_control<SwatchButton>(gf::StableId("primary"), "Primary", Color{0, 0, 0, 255});
    secondary_ = gf::make_control<SwatchButton>(gf::StableId("secondary"), "Alt", Color{255, 255, 255, 255});
    for (int i = 0; i < 2; ++i) {
        std::shared_ptr<SwatchButton> swatch = i == 0 ? primary_ : secondary_;
        (*swatch).set_requested_bounds({885 + i * 58.0, 36, 54, 80});
        (*swatch).set_font({gf::FontRole::control, 13, 400, false});
        (*swatch).set_content_padding({0, 40, 0, 0});
        subscriptions_.push_back((*swatch).clicked().subscribe(
            *this, gf::Delegate<gf::ButtonBase&>::bind<Ribbon, &Ribbon::clicked>(*this)));
        button_pages_.push_back(building_page_);
        buttons_.push_back(swatch);
        add_child(swatch);
    }
    for (int i = 0; i < 30; ++i) {
        std::shared_ptr<SwatchButton> swatch =
            gf::make_control<SwatchButton>(gf::StableId("swatch-" + std::to_string(i)), "", ribbon_color(i));
        (*swatch).set_requested_bounds({1005.0 + (i % 10) * 20, 36.0 + (i / 10) * 24, 20, 23});
        (*swatch).set_accessible_name("Palette color " + std::to_string(i + 1));
        subscriptions_.push_back((*swatch).clicked().subscribe(
            *this, gf::Delegate<gf::ButtonBase&>::bind<Ribbon, &Ribbon::clicked>(*this)));
        button_pages_.push_back(building_page_);
        buttons_.push_back(swatch);
        add_child(swatch);
    }
    button("edit-colors", "Edit\ncolors", 24, {1210, 34, 60, 83}, true);
    add_options();
    show_page();
}
namespace {
class PatternButton final : public gf::Button {
  public:
    PatternButton(gf::StableId id, int pattern) : Button(std::move(id)), pattern_(pattern) {}
    void on_paint(gf::Painter& painter, gf::Rect damage) override {
        Button::on_paint(painter, damage);
        Ink ink;
        ink.pattern = static_cast<Pattern>(pattern_);
        ink.primary = {40, 80, 120, 255};
        ink.secondary = {245, 248, 252, 255};
        for (int y = 0; y < 24; ++y) {
            for (int x = 0; x < 25; ++x) {
                Color color = patterned(ink, x, y);
                painter.fill_rect({2.0 + x, 2.0 + y, 1, 1}, gf::Color::rgba(color.r, color.g, color.b));
            }
        }
    }

  private:
    int pattern_;
};
} // namespace
void Ribbon::option(std::shared_ptr<gf::Control> control, gf::Rect bounds) {
    (*control).set_requested_bounds(bounds);
    option_controls_.push_back(control);
    option_pages_.push_back(building_page_);
    add_child(control);
}
void Ribbon::check(const std::string& id, const std::string& text, gf::Rect bounds) {
    std::shared_ptr<gf::CheckBox> control = gf::make_control<gf::CheckBox>(gf::StableId(id), text);
    (*control).set_auto_check(false);
    (*control).set_font({gf::FontRole::control, 13, 400, false});
    subscriptions_.push_back((*control).clicked().subscribe(
        *this, gf::Delegate<gf::ButtonBase&>::bind<Ribbon, &Ribbon::clicked>(*this)));
    checks_.push_back(control);
    option(control, bounds);
}
std::shared_ptr<gf::NumericUpDown> Ribbon::number(const std::string& id, const std::string& text,
                                                  gf::Rect bounds, double minimum, double maximum,
                                                  double value, int decimals) {
    std::shared_ptr<gf::Label> label = gf::make_control<gf::Label>(gf::StableId(id + "-label"), text);
    (*label).set_font({gf::FontRole::control, 13, 400, false});
    option(label, {bounds.x, bounds.y, 113, bounds.height});
    std::shared_ptr<gf::NumericUpDown> control = gf::make_control<gf::NumericUpDown>(gf::StableId(id));
    (*control).set_range(minimum, maximum);
    (*control).set_decimal_places(static_cast<std::uint8_t>(decimals));
    (*control).set_increment(decimals ? 0.05 : 1);
    (*control).set_value(value);
    (*control).set_accessible_name(text);
    option(control, {bounds.x + 113, bounds.y, bounds.width - 113, bounds.height});
    subscriptions_.push_back((*control).value_changed().subscribe(
        *this, gf::Delegate<double>::bind<Ribbon, &Ribbon::options_changed>(*this)));
    return control;
}
void Ribbon::add_options() {
    building_page_ = 2;
    button("zoom-in", "Zoom in", 12, {8, 35, 65, 81}, true);
    button("zoom-out", "Zoom out", 12, {76, 35, 65, 81}, true);
    button("actual-size", "100%", 5, {144, 35, 65, 81}, true);
    check("show-rulers", "Rulers", {234, 36, 144, 25});
    check("show-grid", "Gridlines", {234, 64, 144, 25});
    check("show-status", "Status bar", {234, 92, 144, 25});
    button("full-screen", "Full screen", 5, {400, 35, 98, 81}, true);
    button("fit", "Fit window", 5, {507, 35, 98, 81}, true);
    building_page_ = 4;
    button("patterns-stamp", "Stamp", 20, {8, 35, 66, 81}, true);
    std::shared_ptr<gf::Button> mesh = button("mesh", "Mesh", 21, {82, 35, 66, 81}, true);
    (*mesh).set_enabled(false);
    (*mesh).set_accessible_name("Mesh — port in progress");
    button("patterns-path", "Path", 21, {156, 35, 66, 81}, true);
    building_page_ = 12;
    for (int i = 0; i < 18; ++i) {
        std::shared_ptr<gf::Button> swatch =
            gf::make_control<PatternButton>(gf::StableId("r-pattern-" + std::to_string(i)), i);
        (*swatch).set_requested_bounds({242.0 + (i % 9) * 33, 36.0 + (i / 9) * 32, 29, 28});
        (*swatch).set_accessible_name(pattern_names[i]);
        subscriptions_.push_back((*swatch).clicked().subscribe(
            *this, gf::Delegate<gf::ButtonBase&>::bind<Ribbon, &Ribbon::clicked>(*this)));
        button_pages_.push_back(building_page_);
        buttons_.push_back(swatch);
        add_child(swatch);
    }
    check("transparent-pattern", "Transparent second color", {242, 96, 300, 25});
    grain_ = number("grain-scale", "Grain scale", {560, 35, 273, 25}, 0.3, 4, 1, 2);
    tooth_ = number("paper-tooth", "Paper tooth", {560, 64, 273, 25}, 0, 1, 0.65, 2);
    load_ = number("paint-load", "Paint load", {560, 93, 273, 25}, 0, 1, 0.65, 2);
    button("edge-medium", "Edge medium", 13, {858, 35, 185, 26}, false, true);
    button("fill-medium", "Fill medium", 8, {1055, 35, 185, 26}, false, true);
    angle_ = number("grain-angle", "Grain angle", {858, 65, 240, 25}, -180, 180, -20);
    button("new-grain", "New grain", -1, {1110, 65, 130, 25});
    building_page_ = 8;
    tool_size_ = number("tool-size", "Size (px)", {12, 36, 204, 25}, 1, 64, 3);
    check("continuous-path", "Continuous path", {12, 65, 210, 25});
    check("outline", "Edge", {12, 94, 90, 25});
    check("fill", "Fill", {116, 94, 100, 25});
    check("transparent-selection", "Transparent selection", {12, 36, 210, 25});
    button("context-select-all", "Select all", -1, {12, 65, 95, 25});
    button("context-invert-selection", "Invert", -1, {116, 65, 100, 25});
    button("context-crop", "Crop", 4, {12, 94, 95, 25});
    button("context-delete", "Delete", -1, {116, 94, 100, 25});
    for (int i = 0; i < 4; ++i) {
        const char* names[] = {"Circle", "Pill", "Square", "Rectangle"};
        button("stamp-shape-" + std::to_string(i), names[i], -1,
               {12.0 + 105 * (i % 2), 36.0 + 28 * (i / 2), 100, 25});
    }
    check("stamp-transparent", "Transparent stamp", {12, 94, 210, 25});
    button("context-stamp-clear", "Clear stamp", -1, {242, 36, 135, 30});
    button("context-zoom-in", "Zoom in", 12, {12, 36, 95, 78}, true);
    button("context-zoom-out", "Zoom out", 12, {117, 36, 99, 78}, true);
    std::shared_ptr<gf::Label> picker =
        gf::make_control<gf::Label>(gf::StableId("picker-hint"), "Left: Primary\nRight: Alt");
    option(picker, {12, 36, 204, 74});
    building_page_ = 1;
}
void Ribbon::show_page() {
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    Tool tool = (*editor).document.tool;
    bool selection = tool == Tool::Select || tool == Tool::Lasso;
    bool ink = tool == Tool::Pencil || tool == Tool::Fill || tool == Tool::Brush || tool == Tool::Shape ||
               tool == Tool::Path;
    bool material = tool == Tool::Brush || tool == Tool::Shape || tool == Tool::Path;
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        gf::Button& control = *buttons_[i];
        std::string id(control.stable_id().value());
        bool visible = button_pages_[i] == 0 || (button_pages_[i] & page_) != 0;
        if (page_ == 8 && button_pages_[i] != 0) {
            if (id.starts_with("r-pattern-")) {
                visible = ink;
            }
            if (id == "edge-medium" || id == "fill-medium" || id == "new-grain") {
                visible = material;
            }
            if (id.starts_with("context-")) {
                visible = id == "context-stamp-clear"       ? tool == Tool::Stamp
                          : id.starts_with("context-zoom-") ? tool == Tool::Magnifier
                                                            : selection;
            }
            if (id.starts_with("stamp-shape-")) {
                visible = tool == Tool::Stamp;
            }
        }
        control.set_visible(visible);
    }
    for (std::size_t i = 0; i < option_controls_.size(); ++i) {
        gf::Control& control = *option_controls_[i];
        std::string id(control.stable_id().value());
        bool visible = (option_pages_[i] & page_) != 0;
        if (page_ == 8 && visible) {
            if (id == "transparent-pattern") {
                visible = ink;
            }
            if (id.starts_with("grain-") || id.starts_with("paper-") || id.starts_with("paint-load")) {
                visible = material;
            }
            if (id.starts_with("tool-size")) {
                visible = tool == Tool::Pencil || tool == Tool::Eraser || material;
            }
            if (id == "continuous-path") {
                visible = tool == Tool::Path;
            }
            if (id == "outline" || id == "fill") {
                visible = tool == Tool::Shape || tool == Tool::Path;
            }
            if (id == "transparent-selection") {
                visible = selection;
            }
            if (id == "stamp-transparent") {
                visible = tool == Tool::Stamp;
            }
            if (id == "picker-hint") {
                visible = tool == Tool::Picker;
            }
        }
        control.set_visible(visible);
    }
    invalidate(gf::Dirty::paint);
}
void Ribbon::options_changed(double) {
    if (synchronizing_) {
        return;
    }
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    Ink& ink = (*editor).document.ink;
    ink.grain_scale = (*grain_).value();
    ink.paper_roughness = (*tooth_).value();
    ink.pigment_load = (*load_).value();
    ink.material_angle = (*angle_).value();
    ink.size = static_cast<int>((*tool_size_).value());
    (*editor).document.sync_curve();
    (*editor).document.sync_path();
    (*editor).refresh();
}
void Ribbon::apply_choice(const std::string& id) {
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    Document& document = (*editor).document;
    if (id.starts_with("r-pattern-")) {
        document.ink.pattern = static_cast<Pattern>(std::stoi(id.substr(10)));
    } else if (id.starts_with("stamp-shape-")) {
        document.stamp_shape = static_cast<StampShape>(std::stoi(id.substr(12)));
    } else if (id == "stamp-transparent") {
        document.stamp_transparent = !document.stamp_transparent;
    } else if (id == "new-grain") {
        ++document.ink.noise;
    } else if (id == "patterns-stamp") {
        (*editor).choose_tool(Tool::Stamp);
    } else if (id == "patterns-path") {
        if (!document.continuous_path) {
            document.commit_path();
        }
        (*editor).choose_tool(Tool::Path);
        document.continuous_path = true;
    } else if (id == "context-stamp-clear") {
        document.stamp = {};
    } else {
        (*editor).execute(id.starts_with("context-") ? id.substr(8) : id);
    }
    document.sync_curve();
    document.sync_path();
    (*editor).refresh();
}

void Ribbon::on_attached_to_window() {
    tooltips_ = std::make_shared<gf::ToolTip>(*attached_window());
    const char* tool_names[] = {"Select",       "Free-form selection", "Pencil", "Fill with color", "Eraser",
                                "Color picker", "Magnifier",           "Brush",  "Shape",           "Path",
                                "Stamp"};
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        gf::Button& control = *buttons_[i];
        std::string id(control.stable_id().value());
        std::string name = control.text().empty() ? control.accessible_name() : control.text();
        if (id.starts_with("tool-") && id != "tool-tab") {
            name = tool_names[std::stoi(id.substr(5))];
        } else if (id.starts_with("shape-")) {
            name = shape_names[std::stoi(id.substr(6))];
        } else if (id == "undo") {
            name = "Undo (Ctrl/Command+Z)";
        } else if (id == "redo") {
            name = "Redo (Ctrl/Command+Y)";
        } else if (id == "size-menu") {
            name = "Brush size";
        } else if (id == "crop") {
            name = "Crop";
        } else if (id == "resize") {
            name = "Resize";
        } else if (id == "rotate-menu") {
            name = "Rotate and flip";
        } else if (id == "cut") {
            name = "Cut (Ctrl/Command+X)";
        } else if (id == "copy") {
            name = "Copy (Ctrl/Command+C)";
        } else if (id == "shapes-menu") {
            name = "More shapes";
        } else if (id.starts_with("swatch-")) {
            name = to_hex(ribbon_color(std::stoi(id.substr(7))));
        }
        if (!name.empty()) {
            control.set_accessible_name(name);
            (*tooltips_).set_tool_tip(buttons_[i], name);
        }
    }
    small_icons_ = std::make_shared<gf::ImageList>(*attached_window(), gf::Size{16, 16});
    medium_icons_ = std::make_shared<gf::ImageList>(*attached_window(), gf::Size{24, 24});
    large_icons_ = std::make_shared<gf::ImageList>(*attached_window(), gf::Size{32, 32});
    for (int i = 0; i < 25 + shape_count; ++i) {
        int icon = i < 25 ? i : 100 + i - 25;
        static_cast<void>((*small_icons_).add_png(std::to_string(icon), ribbon_icon_png(icon, 16), 2));
        static_cast<void>((*medium_icons_).add_png(std::to_string(icon), ribbon_icon_png(icon, 24), 2));
        static_cast<void>((*large_icons_).add_png(std::to_string(icon), ribbon_icon_png(icon, 32), 2));
    }
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        gf::Button& control = *buttons_[i];
        if (control.stable_id().value() == "undo" || control.stable_id().value() == "redo" ||
            control.stable_id().value() == "save") {
            control.set_image_list(medium_icons_);
            continue;
        }
        if (!control.image_key().empty()) {
            control.set_image_list(control.requested_bounds().height > 45 &&
                                           control.requested_bounds().width > 25
                                       ? large_icons_
                                       : small_icons_);
        }
    }
}
void Ribbon::bind_icon(gf::Button& button, int icon, bool large) {
    if (icon < 0) {
        return;
    }
    button.set_image_list(large ? large_icons_ : small_icons_);
    button.set_image_key(std::to_string(icon));
}
void Ribbon::arrange(gf::Rect bounds) {
    arrange_self(bounds);
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        set_child_layout(buttons_[i], (*buttons_[i]).requested_bounds());
    }
    for (const std::shared_ptr<gf::Control>& control : option_controls_) {
        set_child_layout(control, (*control).requested_bounds());
    }
}
void Ribbon::on_paint(gf::Painter& painter, gf::Rect) {
    double width = committed_arranged_bounds().width;
    painter.fill_rect({0, 0, width, 27}, gf::Color::rgba(235, 242, 250));
    const gf::GradientStop stops[] = {{0, gf::Color::rgba(253, 254, 255)},
                                      {0.48, gf::Color::rgba(241, 246, 251)},
                                      {1, gf::Color::rgba(222, 234, 247)}};
    painter.fill_linear_gradient({0, 27, width, 116}, {0, 27}, {0, 143}, stops);
    painter.draw_line({0, 27}, {width, 27}, gf::Color::rgba(182, 199, 218), 1);
    painter.draw_line({0, 142}, {width, 142}, gf::Color::rgba(160, 182, 207), 1);
    if (page_ != 1) {
        const gf::FontSpec font{gf::FontRole::control, 12, 400, false, 0.08};
        const std::vector<std::string> labels =
            page_ == 2 ? std::vector<std::string>{"Zoom", "Show or hide", "Display"}
                       : std::vector<std::string>{page_ == 4 ? "Tools" : "Tool settings", "Patterns",
                                                  "Material", "Media"};
        const std::vector<double> edges =
            page_ == 2 ? std::vector<double>{0, 220, 390, 620} : std::vector<double>{0, 232, 548, 846, width};
        std::shared_ptr<Editor> editor = editor_.lock();
        Tool tool = editor ? (*editor).document.tool : Tool::Pencil;
        for (std::size_t i = 0; i < labels.size(); ++i) {
            if (page_ == 8 && i > 0) {
                bool ink = tool == Tool::Pencil || tool == Tool::Fill || tool == Tool::Brush ||
                           tool == Tool::Shape || tool == Tool::Path;
                bool material = tool == Tool::Brush || tool == Tool::Shape || tool == Tool::Path;
                if ((i == 1 && !ink) || (i > 1 && !material)) {
                    continue;
                }
            }
            double x = edges[i + 1];
            painter.draw_line({x, 32}, {x, 138}, gf::Color::rgba(188, 204, 222), 1);
            gf::Size size = painter.measure_text_utf8(labels[i], font);
            painter.draw_text_utf8({(edges[i] + x - size.width) / 2, 132}, labels[i], font,
                                   gf::Color::rgba(72, 91, 112));
        }
        return;
    }
    for (double x : {74.0, 338.0, 540.0, 818.0, 879.0}) {
        painter.draw_line({x, 32}, {x, 138}, gf::Color::rgba(188, 204, 222), 1);
        painter.draw_line({x + 1, 32}, {x + 1, 138}, gf::Color::rgba(255, 255, 255), 1);
    }
    painter.fill_rect({546, 36, 176, 75}, gf::Color::rgba(255, 255, 255));
    painter.stroke_rect({545.5, 35.5, 195, 76}, gf::Color::rgba(160, 183, 207), 1);
    const char* captions[] = {"Clipboard", "Tools", "Brushes", "Shapes", "Size", "Colors"};
    const double centers[] = {108, 239, 439, 679, 848, 1076};
    const gf::FontSpec font{gf::FontRole::control, 12, 400, false, 0.08};
    for (int i = 0; i < 6; ++i) {
        gf::Size size = painter.measure_text_utf8(captions[i], font);
        painter.draw_text_utf8({centers[i] - size.width / 2, 132}, captions[i], font,
                               gf::Color::rgba(72, 91, 112));
    }
}
void Ribbon::synchronize() {
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    Document& document = (*editor).document;
    (*primary_).set_color(document.ink.primary);
    (*secondary_).set_color(document.ink.secondary);
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        gf::Button& control = *buttons_[i];
        std::string id(control.stable_id().value());
        bool selected = false;
        if (id.starts_with("tool-") && id != "tool-tab") {
            selected = document.tool == ribbon_tools[std::stoi(id.substr(5))];
        }
        if (id.starts_with("shape-")) {
            selected =
                document.tool == Tool::Shape && static_cast<int>(document.shape) == std::stoi(id.substr(6));
        }
        if (id == "brush-menu") {
            selected = document.tool == Tool::Brush;
        }
        if (id == "primary") {
            selected = !secondary_color_;
        }
        if (id == "secondary") {
            selected = secondary_color_;
        }
        if (id.ends_with("-tab")) {
            selected = id == (page_ == 1   ? "home-tab"
                              : page_ == 2 ? "view-tab"
                              : page_ == 4 ? "patterns-tab"
                                           : "tool-tab");
        }
        if (id.starts_with("r-pattern-")) {
            selected = static_cast<int>(document.ink.pattern) == std::stoi(id.substr(10));
        }
        if (id.starts_with("stamp-shape-")) {
            selected = static_cast<int>(document.stamp_shape) == std::stoi(id.substr(12));
        }
        if (id == "tool-tab") {
            const char* names[] = {"Selection", "Selection",    "Pencil",    "Fill",  "Text",
                                   "Eraser",    "Color picker", "Magnifier", "Brush", "Shape",
                                   "Path",      "Stamp",        "Mesh"};
            control.set_text(names[static_cast<int>(document.tool)]);
            control.set_accessible_name(std::string(names[static_cast<int>(document.tool)]) + " tools");
        }
        control.set_selected(selected);
    }
    synchronizing_ = true;
    (*grain_).set_value(document.ink.grain_scale);
    (*tooth_).set_value(document.ink.paper_roughness);
    (*load_).set_value(document.ink.pigment_load);
    (*angle_).set_value(document.ink.material_angle);
    (*tool_size_).set_value(document.ink.size);
    for (const std::shared_ptr<gf::CheckBox>& check : checks_) {
        std::string id((*check).stable_id().value());
        (*check).set_checked(id == "show-rulers"             ? (*editor).show_rulers
                             : id == "show-grid"             ? (*editor).show_grid
                             : id == "show-status"           ? (*editor).show_status
                             : id == "transparent-pattern"   ? document.ink.transparent_pattern
                             : id == "transparent-selection" ? document.transparent_selection
                             : id == "stamp-transparent"     ? document.stamp_transparent
                             : id == "continuous-path"       ? document.continuous_path
                             : id == "outline"               ? document.shape_outline
                                                             : document.shape_fill);
    }
    synchronizing_ = false;
    show_page();
}
void Ribbon::clicked(gf::ButtonBase& button) {
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    std::string id(button.stable_id().value());
    if (id.ends_with("-tab")) {
        close_popup();
        page_ = id == "home-tab" ? 1 : id == "view-tab" ? 2 : id == "patterns-tab" ? 4 : 8;
        synchronize();
        invalidate(gf::Dirty::paint);
        return;
    }
    if (id == "primary" || id == "secondary") {
        secondary_color_ = id == "secondary";
        synchronize();
        return;
    }
    if (id == "edit-colors") {
        (*editor).execute(secondary_color_ ? "secondary" : "primary");
        return;
    }
    if (id.starts_with("swatch-")) {
        Color& color = secondary_color_ ? (*editor).document.ink.secondary : (*editor).document.ink.primary;
        color = ribbon_color(std::stoi(id.substr(7)));
        (*editor).document.sync_curve();
        (*editor).document.sync_path();
        (*editor).refresh();
        return;
    }
    if (id.starts_with("shape-")) {
        (*editor).choose_shape(static_cast<Shape>(std::stoi(id.substr(6))));
        return;
    }
    apply_choice(id);
}
std::shared_ptr<gf::Button> Ribbon::add_popup_button(gf::Panel& panel, const std::string& id,
                                                     const std::string& text, int icon, gf::Rect bounds,
                                                     bool selected) {
    std::shared_ptr<gf::Button> item;
    if (id.starts_with("size-")) {
        item = gf::make_control<WeightButton>(gf::StableId("popup-" + id), text, std::stoi(id.substr(5)));
    } else {
        item = gf::make_control<gf::Button>(gf::StableId("popup-" + id), text);
    }
    (*item).set_requested_bounds(bounds);
    (*item).set_font({gf::FontRole::control, 13, 400, false, 0.05});
    (*item).set_text_image_relation(gf::TextImageRelation::image_before_text);
    (*item).set_text_alignment(gf::ContentAlignment::middle_left);
    (*item).set_image_alignment(gf::ContentAlignment::middle_left);
    (*item).set_content_padding({8, 4, 8, 4});
    (*item).set_selected(selected);
    bind_icon(*item, icon, false);
    popup_subscriptions_.push_back((*item).clicked().subscribe(
        *this, gf::Delegate<gf::ButtonBase&>::bind<Ribbon, &Ribbon::popup_clicked>(*this)));
    if (id.starts_with("shape-")) {
        std::string name = shape_names[std::stoi(id.substr(6))];
        (*item).set_accessible_name(name);
        (*item).set_image_list(medium_icons_);
        (*item).set_content_padding({4, 4, 4, 4});
    }
    panel.add_child(item);
    return item;
}
void Ribbon::dropdown(gf::DropDownButton& button) {
    if (popup_owner_.get() == &button) {
        close_popup();
        return;
    }
    close_popup();
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    Document& document = (*editor).document;
    std::string id(button.stable_id().value());
    std::shared_ptr<gf::Panel> panel = gf::make_control<gf::Panel>(gf::StableId("ribbon-popup-content"));
    (*panel).set_theme_override(ribbon_theme());
    gf::SurfaceMaterial material;
    material.fills = {gf::MaterialFillLayer::solid(gf::Color::rgba(250, 252, 255))};
    material.border = gf::MaterialBorder{gf::Color::rgba(141, 164, 190), 1};
    material.shadows.push_back({{0, 3}, 7, 0, gf::Color::rgba(30, 50, 75, 60), false});
    (*panel).set_authored_surface_material(material);
    double width = 232, height = 10;
    if (id == "shapes-menu") {
        width = 280;
        height = 8 + 34 * ((shape_count + 6) / 7);
        for (int i = 0; i < shape_count; ++i) {
            add_popup_button(*panel, "shape-" + std::to_string(i), "", 100 + i,
                             {6.0 + (i % 7) * 38, 5.0 + (i / 7) * 34, 37, 33},
                             document.shape == static_cast<Shape>(i));
        }
    } else if (id == "brush-menu" || id == "edge-medium" || id == "fill-medium") {
        if (!brush_previews_) {
            brush_previews_ = std::make_shared<gf::ImageList>(*attached_window(), gf::Size{104, 42});
            for (int i = 0; i < brush_count; ++i) {
                Image sample;
                sample.reset(208, 84, {255, 255, 255, 255});
                Ink ink;
                ink.primary = {48, 83, 123, 255};
                ink.size = 25;
                ink.brush = static_cast<Brush>(i);
                MaterialStroke material;
                Point previous{18, 51};
                for (int step = 1; step <= 18; ++step) {
                    Point next{18 + step * 9.4, 44 + 16 * std::sin(step * 0.33)};
                    if (textured_brush(ink.brush)) {
                        material.segment(sample, previous, next, ink);
                    } else {
                        stroke(sample, previous, next, ink);
                    }
                    previous = next;
                }
                std::vector<std::uint8_t> png = encode_png(sample);
                static_cast<void>(
                    (*brush_previews_).add_png(std::to_string(i), std::as_bytes(std::span(png)), 2));
            }
        }
        width = 382;
        height = 8 + 78 * 4;
        const char* labels[] = {"Brush",      "Calligraphy 1", "Calligraphy 2", "Airbrush",
                                "Oil brush",  "Crayon",        "Marker",        "Natural pencil",
                                "Watercolor", "Bristle brush", "Soft pastel",   "Charcoal"};
        for (int i = 0; i < brush_count; ++i) {
            std::shared_ptr<gf::Button> item =
                add_popup_button(*panel,
                                 (id == "fill-medium"   ? "fill-brush-"
                                  : id == "edge-medium" ? "edge-brush-"
                                                        : "brush-") +
                                     std::to_string(i),
                                 labels[i], -1, {6.0 + (i % 3) * 124, 4.0 + (i / 3) * 78, 122, 76},
                                 (id == "fill-medium" ? document.shape_fill_brush : document.ink.brush) ==
                                     static_cast<Brush>(i));
            (*item).set_image_list(brush_previews_);
            (*item).set_image_key(std::to_string(i));
            (*item).set_text_image_relation(gf::TextImageRelation::image_above_text);
            (*item).set_text_alignment(gf::ContentAlignment::middle_center);
            (*item).set_image_alignment(gf::ContentAlignment::middle_center);
        }
    } else if (id == "size-menu") {
        const int values[] = {1, 2, 3, 4, 5, 7, 8, 12, 16, 24, 32, 48, 64};
        width = 172;
        height = 8 + 28 * std::size(values);
        for (std::size_t i = 0; i < std::size(values); ++i) {
            add_popup_button(*panel, "size-" + std::to_string(values[i]), std::to_string(values[i]) + " px",
                             -1, {4, 4 + i * 28.0, width - 8, 27}, document.ink.size == values[i]);
        }
    } else if (id == "fill-menu" || id == "tool-pattern-menu") {
        if (!pattern_previews_) {
            pattern_previews_ = std::make_shared<gf::ImageList>(*attached_window(), gf::Size{82, 28});
            for (int i = 0; i < 18; ++i) {
                Image sample;
                sample.reset(164, 56, {255, 255, 255, 255});
                Ink ink;
                ink.primary = {47, 73, 103, 255};
                ink.pattern = static_cast<Pattern>(i);
                for (int y = 0; y < sample.height; ++y) {
                    for (int x = 0; x < sample.width; ++x) {
                        sample.set(x, y, patterned(ink, x / 2, y / 2));
                    }
                }
                std::vector<std::uint8_t> png = encode_png(sample);
                static_cast<void>(
                    (*pattern_previews_).add_png(std::to_string(i), std::as_bytes(std::span(png)), 2));
            }
        }
        width = 330;
        height = 8 + 62 * 6 + 62;
        for (int i = 0; i < 18; ++i) {
            std::shared_ptr<gf::Button> item =
                add_popup_button(*panel, "pattern-" + std::to_string(i), pattern_names[i], -1,
                                 {5.0 + (i % 3) * 107, 4.0 + (i / 3) * 62, 105, 60},
                                 static_cast<int>(document.ink.pattern) == i);
            (*item).set_font({gf::FontRole::control, 12, 400, false});
            (*item).set_image_list(pattern_previews_);
            (*item).set_image_key(std::to_string(i));
            (*item).set_text_image_relation(gf::TextImageRelation::image_above_text);
            (*item).set_text_alignment(gf::ContentAlignment::middle_center);
        }
        add_popup_button(*panel, "fill-off", "No fill", -1, {5, 380, width - 10, 27}, !document.shape_fill);
        add_popup_button(*panel, "transparent-pattern", "Transparent second color", -1,
                         {5, 410, width - 10, 27}, document.ink.transparent_pattern);
    } else {
        std::vector<std::string> ids, texts;
        if (id == "tool-0") {
            ids = {"tool-0", "tool-1", "select-all", "invert-selection", "transparent-selection"};
            texts = {"Rectangular selection", "Free-form selection", "Select all", "Invert selection",
                     "Transparent selection"};
        }
        if (id == "paste") {
            ids = {"paste", "open"};
            texts = {"Paste", "Open image…"};
        }
        if (id == "rotate-menu") {
            ids = {"rotate-right", "rotate-left", "rotate-180", "flip-horizontal", "flip-vertical"};
            texts = {"Rotate right 90°", "Rotate left 90°", "Rotate 180°", "Flip horizontal",
                     "Flip vertical"};
        }
        if (id == "outline-menu") {
            ids = {"outline-on", "outline-off"};
            texts = {"Solid outline", "No outline"};
        }
        if (id == "fill-menu") {
            ids = {"fill-on", "fill-off"};
            texts = {"Solid fill", "No fill"};
        }
        if (id == "view-menu") {
            ids = {"zoom-in", "zoom-out", "actual-size", "fit"};
            texts = {"Zoom in", "Zoom out", "100%", "Fit canvas"};
        }
        if (id == "tool-9") {
            ids = {"tool-9", "continuous-path"};
            texts = {"Draw path", "Continuous path"};
        }
        if (id == "tool-10") {
            ids = {"tool-10", "stamp-clear"};
            texts = {"Capture / place stamp", "Clear captured stamp"};
        }
        height = 8 + 30 * ids.size();
        for (std::size_t i = 0; i < ids.size(); ++i) {
            add_popup_button(*panel, ids[i], texts[i], -1, {4, 4 + i * 30.0, width - 8, 29});
        }
    }
    popup_owner_ = std::dynamic_pointer_cast<gf::DropDownButton>(button.shared_from_this());
    gf::AnchoredPopupPlacement placement;
    placement.preferred_size = {width, height};
    placement.gap = 2;
    popup_layer_ =
        gf::make_control<gf::AnchoredPopupLayer>(gf::StableId("ribbon-popup"), popup_owner_, placement);
    (*popup_layer_).set_content(panel);
    popup_subscriptions_.push_back(
        (*popup_layer_)
            .dismiss_requested()
            .subscribe(*this, gf::Delegate<gf::PopupDismissReason>::bind<Ribbon, &Ribbon::dismissed>(*this)));
    popup_ = (*attached_window()).open_popup(popup_owner_, popup_layer_);
    if (id == "shapes-menu") {
        for (int i = 0; i < shape_count; ++i) {
            gf::Control::Ptr shape = (*attached_window()).find("popup-shape-" + std::to_string(i));
            (*tooltips_).set_tool_tip(shape, shape_names[i]);
        }
    }
    focus_scope_ = (*attached_window()).begin_focus_scope(popup_layer_);
    (*popup_owner_).set_drop_down_open(true);
}
void Ribbon::dismissed(gf::PopupDismissReason) {
    close_popup();
}
void Ribbon::close_popup() {
    if (popup_owner_ && (*popup_owner_).stable_id().value() == "shapes-menu" && attached_window()) {
        for (int i = 0; i < shape_count; ++i) {
            gf::Control::Ptr shape = (*attached_window()).find("popup-shape-" + std::to_string(i));
            if (shape) {
                static_cast<void>((*tooltips_).remove_tool_tip(*shape));
            }
        }
    }
    if (focus_scope_ && attached_window()) {
        static_cast<void>((*attached_window()).end_focus_scope(focus_scope_));
        focus_scope_ = {};
    }
    popup_.disconnect();
    if (popup_owner_) {
        (*popup_owner_).set_drop_down_open(false);
    }
    popup_owner_.reset();
    popup_layer_.reset();
    popup_subscriptions_.clear();
}
void Ribbon::popup_clicked(gf::ButtonBase& button) {
    std::string id(button.stable_id().value().substr(6));
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    close_popup();
    Document& document = (*editor).document;
    if (id.starts_with("shape-")) {
        (*editor).choose_shape(static_cast<Shape>(std::stoi(id.substr(6))));
    } else if (id.starts_with("fill-brush-") || id.starts_with("edge-brush-")) {
        if (id.starts_with("fill-brush-")) {
            document.shape_fill_brush = static_cast<Brush>(std::stoi(id.substr(11)));
            document.shape_fill = true;
        } else {
            document.ink.brush = static_cast<Brush>(std::stoi(id.substr(11)));
            document.shape_outline = true;
        }
    } else if (id.starts_with("brush-")) {
        document.ink.brush = static_cast<Brush>(std::stoi(id.substr(6)));
        (*editor).choose_tool(Tool::Brush);
    } else if (id.starts_with("size-")) {
        document.ink.size = std::stoi(id.substr(5));
    } else if (id.starts_with("pattern-")) {
        document.ink.pattern = static_cast<Pattern>(std::stoi(id.substr(8)));
        document.shape_fill = true;
    } else if (id == "outline-on" || id == "outline-off") {
        document.shape_outline = id == "outline-on";
    } else if (id == "fill-on" || id == "fill-off") {
        document.shape_fill = id == "fill-on";
    } else if (id == "stamp-clear") {
        document.stamp = {};
    } else {
        (*editor).execute(id);
    }
    document.sync_curve();
    document.sync_path();
    (*editor).refresh();
}
} // namespace paint::forms
