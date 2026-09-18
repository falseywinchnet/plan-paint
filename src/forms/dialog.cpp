#include "forms/dialog.hpp"
#include "forms/atlas.hpp"
#include "forms/display.hpp"
#include "forms/editor.hpp"
#include "forms/ribbon.hpp"
#include <algorithm>
#include <cmath>
#include <gui_forms/delegate.hpp>
namespace paint::forms {
namespace gf = gui_forms;
namespace {
Color hsv_color(double hue, double saturation, double value) {
    double h = std::fmod(hue, 1.0) * 6, c = value * saturation, x = c * (1 - std::abs(std::fmod(h, 2) - 1)),
           m = value - c;
    double r = 0, g = 0, b = 0;
    if (h < 1) {
        r = c;
        g = x;
    } else if (h < 2) {
        r = x;
        g = c;
    } else if (h < 3) {
        g = c;
        b = x;
    } else if (h < 4) {
        g = x;
        b = c;
    } else if (h < 5) {
        r = x;
        b = c;
    } else {
        r = c;
        b = x;
    }
    return {static_cast<std::uint8_t>(std::lround((r + m) * 255)),
            static_cast<std::uint8_t>(std::lround((g + m) * 255)),
            static_cast<std::uint8_t>(std::lround((b + m) * 255)), 255};
}
gf::Color ui_color(Color color) {
    return gf::Color::rgba(color.r, color.g, color.b, color.a);
}
} // namespace
ColorPlane::ColorPlane(gf::StableId id, std::weak_ptr<EditorDialog> dialog, bool hue_strip)
    : Control(std::move(id)), dialog_(std::move(dialog)), hue_strip_(hue_strip) {
    set_cursor(gf::CursorKind::crosshair);
    set_accessible_name(hue_strip ? "Hue" : "Saturation and value");
}
void ColorPlane::on_paint(gf::Painter& painter, gf::Rect) {
    std::shared_ptr<EditorDialog> dialog = dialog_.lock();
    if (!dialog) {
        return;
    }
    gf::Rect bounds = {0, 0, committed_arranged_bounds().width, committed_arranged_bounds().height};
    if (hue_strip_) {
        std::vector<gf::GradientStop> stops;
        for (int i = 0; i <= 6; ++i) {
            stops.push_back({i / 6.0, ui_color(hsv_color(i / 6.0, 1, 1))});
        }
        painter.fill_linear_gradient(bounds, {0, 0}, {0, bounds.height}, stops);
        double y = (*dialog).hue * bounds.height;
        painter.stroke_rect({0, y - 2, bounds.width, 4}, gf::Color::rgba(255, 255, 255), 2);
        painter.stroke_rect({0, y - 3, bounds.width, 6}, gf::Color::rgba(35, 50, 65), 1);
    } else {
        const gf::GradientStop horizontal[] = {{0, gf::Color::rgba(255, 255, 255)},
                                               {1, ui_color(hsv_color((*dialog).hue, 1, 1))}};
        const gf::GradientStop vertical[] = {{0, gf::Color::rgba(0, 0, 0, 0)}, {1, gf::Color::rgba(0, 0, 0)}};
        painter.fill_linear_gradient(bounds, {0, 0}, {bounds.width, 0}, horizontal);
        painter.fill_linear_gradient(bounds, {0, 0}, {0, bounds.height}, vertical);
        double x = (*dialog).saturation * bounds.width, y = (1 - (*dialog).value) * bounds.height;
        painter.stroke_rect({x - 4, y - 4, 8, 8}, gf::Color::rgba(255, 255, 255), 2);
        painter.stroke_rect({x - 5, y - 5, 10, 10}, gf::Color::rgba(40, 50, 60), 1);
    }
    painter.stroke_rect({0.5, 0.5, bounds.width - 1, bounds.height - 1}, gf::Color::rgba(132, 153, 176), 1);
}
void ColorPlane::on_pointer(gf::PointerEvent& event) {
    std::shared_ptr<EditorDialog> dialog = dialog_.lock();
    if (!dialog) {
        return;
    }
    if (event.action == gf::PointerAction::down && event.button == gf::PointerButton::primary) {
        set_pointer_capture(true);
    }
    if (has_pointer_capture() &&
        (event.action == gf::PointerAction::down || event.action == gf::PointerAction::move ||
         event.action == gf::PointerAction::up)) {
        gf::Point local = point_from_window(event.position);
        gf::Rect bounds = committed_arranged_bounds();
        double x = std::clamp(local.x / bounds.width, 0.0, 1.0),
               y = std::clamp(local.y / bounds.height, 0.0, 1.0);
        (*dialog).choose_hsv(hue_strip_ ? y : (*dialog).hue, hue_strip_ ? (*dialog).saturation : x,
                             hue_strip_ ? (*dialog).value : 1 - y);
        if (event.action == gf::PointerAction::up) {
            set_pointer_capture(false);
        }
        event.handled = true;
    }
}
EditorDialog::EditorDialog(gf::StableId id, std::weak_ptr<Editor> editor, EditorDialogKind kind,
                           bool secondary)
    : Control(std::move(id)), editor_(std::move(editor)), kind_(kind), secondary_(secondary) {}
void EditorDialog::put(gf::Control::Ptr control, gf::Rect bounds) {
    (*control).set_requested_bounds(bounds);
    controls_.push_back(control);
    add_child(control);
}
void EditorDialog::label(const std::string& id, const std::string& text, gf::Rect bounds, bool heading) {
    std::shared_ptr<gf::Label> control = gf::make_control<gf::Label>(gf::StableId(id), text);
    (*control).set_font({gf::FontRole::control, heading ? 14.0 : 13.0,
                         static_cast<std::uint16_t>(heading ? 600 : 400), false, 0.05});
    (*control).set_foreground(gf::Color::rgba(45, 66, 88));
    put(control, bounds);
}
std::shared_ptr<gf::Button> EditorDialog::button(const std::string& id, const std::string& text,
                                                 gf::Rect bounds) {
    std::shared_ptr<gf::Button> control = gf::make_control<gf::Button>(gf::StableId(id), text);
    (*control).set_font({gf::FontRole::control, 13, 400, false, 0.05});
    (*control).set_use_mnemonic(false);
    subscriptions_.push_back((*control).clicked().subscribe(
        *this, gf::Delegate<gf::ButtonBase&>::bind<EditorDialog, &EditorDialog::clicked>(*this)));
    put(control, bounds);
    return control;
}
std::shared_ptr<gf::NumericUpDown> EditorDialog::number(const std::string& id, gf::Rect bounds,
                                                        double minimum, double maximum, double value,
                                                        int decimals) {
    std::shared_ptr<gf::NumericUpDown> control = gf::make_control<gf::NumericUpDown>(gf::StableId(id));
    (*control).set_range(minimum, maximum);
    (*control).set_decimal_places(static_cast<std::uint8_t>(decimals));
    (*control).set_increment(decimals ? 0.01 : 1);
    (*control).set_value(value);
    (*control).set_accessible_name(id);
    put(control, bounds);
    return control;
}
void EditorDialog::initialize_control_tree() {
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    if (kind_ == EditorDialogKind::color) {
        panel_ = {0, 0, 660, 540};
        original_ = secondary_ ? (*editor).document.ink.secondary : (*editor).document.ink.primary;
        primary_tab_ = button("dialog-color1", "Primary", {18, 48, 90, 28});
        secondary_tab_ = button("dialog-color2", "Alt", {112, 48, 90, 28});
        (*primary_tab_).set_theme_override(ribbon_theme());
        (*secondary_tab_).set_theme_override(ribbon_theme());
        label("basic-colors-label", "Basic colors", {20, 91, 245, 22}, true);
        for (int i = 0; i < 30; ++i) {
            std::shared_ptr<SwatchButton> swatch = gf::make_control<SwatchButton>(
                gf::StableId("dialog-basic-" + std::to_string(i)), "", ribbon_color(i));
            (*swatch).set_theme_override(ribbon_theme());
            (*swatch).set_accessible_name("Basic color " + std::to_string(i + 1));
            subscriptions_.push_back((*swatch).clicked().subscribe(
                *this, gf::Delegate<gf::ButtonBase&>::bind<EditorDialog, &EditorDialog::clicked>(*this)));
            put(swatch, {18 + (i % 10) * 25.0, 116 + (i / 10) * 28.0, 25, 28});
        }
        label("custom-colors-label", "Custom colors", {20, 216, 245, 22}, true);
        for (int i = 0; i < 16; ++i) {
            std::shared_ptr<SwatchButton> swatch =
                gf::make_control<SwatchButton>(gf::StableId("dialog-custom-" + std::to_string(i)), "",
                                               (*editor).custom_colors.colors[static_cast<std::size_t>(i)]);
            (*swatch).set_theme_override(ribbon_theme());
            (*swatch).set_accessible_name("Custom color " + std::to_string(i + 1));
            subscriptions_.push_back((*swatch).clicked().subscribe(
                *this, gf::Delegate<gf::ButtonBase&>::bind<EditorDialog, &EditorDialog::clicked>(*this)));
            put(swatch, {18 + (i % 8) * 31.0, 242 + (i / 8) * 31.0, 29, 29});
            custom_.push_back(swatch);
        }
        button("dialog-add-custom", "Add to custom colors", {20, 315, 245, 30});
        label("original-label", "Original", {20, 368, 118, 23});
        label("new-label", "New", {150, 368, 118, 23});
        old_ = gf::make_control<SwatchButton>(gf::StableId("dialog-original"), "", original_);
        preview_ = gf::make_control<SwatchButton>(gf::StableId("dialog-preview"), "", original_);
        put(old_, {20, 397, 115, 50});
        put(preview_, {150, 397, 115, 50});
        subscriptions_.push_back((*old_).clicked().subscribe(
            *this, gf::Delegate<gf::ButtonBase&>::bind<EditorDialog, &EditorDialog::clicked>(*this)));
        plane_ = gf::make_control<ColorPlane>(
            gf::StableId("color-plane"), std::static_pointer_cast<EditorDialog>(shared_from_this()), false);
        hue_strip_ = gf::make_control<ColorPlane>(
            gf::StableId("hue-strip"), std::static_pointer_cast<EditorDialog>(shared_from_this()), true);
        put(plane_, {296, 91, 290, 224});
        put(hue_strip_, {602, 91, 24, 224});
        const char* names[] = {"Red", "Green", "Blue", "Alpha"};
        std::shared_ptr<gf::NumericUpDown>* fields[] = {&red_, &green_, &blue_, &alpha_};
        for (int i = 0; i < 4; ++i) {
            label("channel-label-" + std::to_string(i), names[i], {296 + i * 84.0, 325, 78, 20});
            *fields[i] = number("color-" + std::string(names[i]), {296 + i * 84.0, 348, 77, 28}, 0, 255, 0);
            subscriptions_.push_back(
                (**fields[i])
                    .value_changed()
                    .subscribe(*this,
                               gf::Delegate<double>::bind<EditorDialog, &EditorDialog::rgb_changed>(*this)));
        }
        label("hex-label", "Hex", {296, 387, 42, 25});
        hex_ = gf::make_control<gf::TextBox>(gf::StableId("color-hex"));
        put(hex_, {340, 386, 122, 28});
        subscriptions_.push_back((*hex_).text_changed().subscribe(
            *this, gf::Delegate<const std::string&>::bind<EditorDialog, &EditorDialog::hex_changed>(*this)));
        label("oklab-label", "OKLab", {296, 424, 60, 23});
        light_ = number("color-lightness", {360, 422, 82, 28}, 0, 1, 0, 3);
        a_ = number("color-a", {450, 422, 82, 28}, -0.5, 0.5, 0, 3);
        b_ = number("color-b", {540, 422, 82, 28}, -0.5, 0.5, 0, 3);
        for (const std::shared_ptr<gf::NumericUpDown>& field : {light_, a_, b_}) {
            subscriptions_.push_back((*field).value_changed().subscribe(
                *this, gf::Delegate<double>::bind<EditorDialog, &EditorDialog::lab_changed>(*this)));
        }
        set_color(original_);
    } else if (kind_ == EditorDialogKind::properties) {
        panel_ = {0, 0, 460, 345};
        Document& document = (*editor).document;
        label("properties-info",
              "RGBA image · " + std::to_string(document.image.width) + " × " +
                  std::to_string(document.image.height) + " pixels",
              {24, 50, 410, 26});
        label("properties-width-label", "Canvas width", {24, 95, 200, 28});
        label("properties-height-label", "Canvas height", {24, 131, 200, 28});
        label("properties-quality-label", "JPEG quality", {24, 167, 200, 28});
        atlas_numbers_.push_back(
            number("properties-width", {242, 94, 186, 30}, 1, 32768, document.image.width));
        atlas_numbers_.push_back(
            number("properties-height", {242, 130, 186, 30}, 1, 32768, document.image.height));
        atlas_numbers_.push_back(
            number("properties-quality", {242, 166, 186, 30}, 1, 100, (*editor).jpeg_quality));
        scale_ = gf::make_control<gf::CheckBox>(gf::StableId("properties-monochrome"),
                                                "Convert to black and white");
        put(scale_, {24, 210, 410, 28});
    } else if (kind_ == EditorDialogKind::print_preview) {
        panel_ = {0, 0, 860, 650};
        label("print-preview-hint", "Fit preview. Use Page setup to choose the paper and orientation.",
              {22, 47, 810, 28});
        button("preview-print", "Print…", {22, 87, 124, 30});
        button("preview-page-setup", "Page setup…", {156, 87, 144, 30});
        std::shared_ptr<gf::RasterCanvas> preview =
            gf::make_control<gf::RasterCanvas>(gf::StableId("print-preview-image"));
        (*preview).set_canvas_background(gf::Color::rgba(255, 255, 255));
        (*preview).set_transparency_grid(false);
        Image image = (*editor).document.output_image();
        publish_image(image, *preview);
        double zoom = std::min(776.0 / image.width, 392.0 / image.height);
        (*preview).set_view(zoom, {-(816.0 / zoom - image.width) / 2, -(432.0 / zoom - image.height) / 2});
        put(preview, {22, 133, 816, 432});
    } else if (kind_ == EditorDialogKind::atlas_gallery) {
        panel_ = {0, 0, 1030, 630};
        std::shared_ptr<AtlasPanel> gallery =
            gf::make_control<AtlasPanel>(gf::StableId("atlas-gallery-panel"), editor_, true);
        put(gallery, {15, 48, 1000, 492});
        (*gallery).synchronize();
    } else if (kind_ == EditorDialogKind::atlas_grid) {
        panel_ = {0, 0, 490, 420};
        const AtlasGrid& grid = (*editor).document.atlas.grid;
        label("atlas-grid-hint", "Split the whole image into equal-sized frames.", {24, 49, 440, 26});
        const char* ids[] = {
            "Columns",         "Rows", "Horizontal margin", "Vertical margin", "Horizontal spacing",
            "Vertical spacing"};
        int values[] = {grid.columns,  grid.rows,      grid.margin_x,
                        grid.margin_y, grid.spacing_x, grid.spacing_y};
        for (int i = 0; i < 6; ++i) {
            label("atlas-label-" + std::to_string(i), ids[i], {24, 89 + i * 34.0, 218, 28});
            std::shared_ptr<gf::NumericUpDown> field =
                number("atlas-" + std::to_string(i), {252, 88 + i * 34.0, 206, 28}, i < 2 ? 1 : 0,
                       i < 2 ? 4096 : 32768, values[i]);
            (*field).set_accessible_name(ids[i]);
            atlas_numbers_.push_back(field);
            subscriptions_.push_back((*field).value_changed().subscribe(
                *this, gf::Delegate<double>::bind<EditorDialog, &EditorDialog::atlas_changed>(*this)));
        }
    } else if (kind_ == EditorDialogKind::icon_sizes || kind_ == EditorDialogKind::cursor_sizes) {
        panel_ = {0, 0, 460, 330};
        label("icon-hint", "Generate square frames from the current image (CONV*).", {22, 50, 420, 26});
        const int sizes[] = {16, 24, 32, 48, 64, 96, 128, 256};
        for (int i = 0; i < 8; ++i) {
            std::shared_ptr<gf::CheckBox> field =
                gf::make_control<gf::CheckBox>(gf::StableId("icon-size-" + std::to_string(sizes[i])),
                                               std::to_string(sizes[i]) + " × " + std::to_string(sizes[i]));
            (*field).set_checked(sizes[i] == 16 || sizes[i] == 32 || sizes[i] == 48 || sizes[i] == 256);
            put(field, {28 + (i % 2) * 216.0, 90 + (i / 2) * 35.0, 188, 28});
            icon_sizes_.push_back(field);
        }
    } else if (kind_ == EditorDialogKind::hotspot) {
        panel_ = {0, 0, 430, 286};
        const Document& document = (*editor).document;
        const IconFrame& frame = document.atlas.icons[document.atlas.active];
        label("hotspot-hint", "The hotspot is the pixel used as the cursor's click point.",
              {20, 47, 390, 26});
        label("hotspot-x-label", "X", {25, 94, 30, 28});
        label("hotspot-y-label", "Y", {222, 94, 30, 28});
        atlas_numbers_.push_back(
            number("hotspot-x", {59, 92, 134, 30}, 0, document.image.width - 1, frame.hotspot_x));
        atlas_numbers_.push_back(
            number("hotspot-y", {256, 92, 134, 30}, 0, document.image.height - 1, frame.hotspot_y));
        button("hotspot-pick", "Pick on canvas", {25, 143, 170, 30});
    } else {
        panel_ = {0, 0, 450, 500};
        Document& document = (*editor).document;
        original_width_ = document.selection.active ? document.selection.image.width : document.image.width;
        original_height_ =
            document.selection.active ? document.selection.image.height : document.image.height;
        label("resize-target", document.selection.active ? "Resize selected content" : "Resize the picture",
              {24, 53, 390, 25}, true);
        percent_tab_ = button("resize-percent", "Percentage", {24, 91, 116, 28});
        pixel_tab_ = button("resize-pixels", "Pixels", {146, 91, 86, 28});
        (*percent_tab_).set_theme_override(ribbon_theme());
        (*pixel_tab_).set_theme_override(ribbon_theme());
        (*pixel_tab_).set_selected(true);
        label("width-label", "Horizontal", {24, 136, 104, 28});
        label("height-label", "Vertical", {24, 174, 104, 28});
        width_ = number("resize-width", {148, 135, 176, 30}, 1, 32768, original_width_);
        height_ = number("resize-height", {148, 173, 176, 30}, 1, 32768, original_height_);
        subscriptions_.push_back((*width_).value_changed().subscribe(
            *this, gf::Delegate<double>::bind<EditorDialog, &EditorDialog::width_changed>(*this)));
        subscriptions_.push_back((*height_).value_changed().subscribe(
            *this, gf::Delegate<double>::bind<EditorDialog, &EditorDialog::height_changed>(*this)));
        lock_ = gf::make_control<gf::CheckBox>(gf::StableId("resize-lock"), "Maintain aspect ratio");
        (*lock_).set_checked(true);
        put(lock_, {24, 214, 360, 25});
        scale_ = gf::make_control<gf::CheckBox>(gf::StableId("resize-scale"), "Scale artwork (CONV*)");
        (*scale_).set_checked(true);
        put(scale_, {24, 242, 360, 25});
        label("resize-hint", "Turn scaling off to change the canvas boundary.", {24, 269, 394, 22});
        label("skew-label", "Skew (degrees)", {24, 308, 390, 24}, true);
        label("skew-horizontal-label", "Horizontal", {24, 341, 112, 28});
        label("skew-vertical-label", "Vertical", {24, 377, 112, 28});
        skew_horizontal_ = number("skew-horizontal", {148, 340, 176, 30}, -80, 80, 0, 1);
        skew_vertical_ = number("skew-vertical", {148, 376, 176, 30}, -80, 80, 0, 1);
    }
    error_ = gf::make_control<gf::Label>(gf::StableId("dialog-error"));
    (*error_).set_font({gf::FontRole::control, 12, 400, false});
    (*error_).set_foreground(gf::Color::rgba(161, 49, 39));
    (*error_).set_text_wrapping(gf::TextWrapping::word);
    put(error_, {20, panel_.height - (kind_ == EditorDialogKind::color ? 74 : 86), panel_.width - 40,
                 kind_ == EditorDialogKind::color ? 25.0 : 36.0});
    std::shared_ptr<gf::Button> ok = button(
        "dialog-ok",
        kind_ == EditorDialogKind::atlas_gallery || kind_ == EditorDialogKind::print_preview ? "Close" : "OK",
        {panel_.width - 212, panel_.height - 41, 90, 28});
    (*ok).set_default_button(true);
    if (kind_ == EditorDialogKind::atlas_grid) {
        atlas_changed(0);
    }
    if (kind_ != EditorDialogKind::atlas_gallery && kind_ != EditorDialogKind::print_preview) {
        button("dialog-cancel", "Cancel", {panel_.width - 110, panel_.height - 41, 90, 28});
    }
}
void EditorDialog::arrange(gf::Rect bounds) {
    if (attached_window()) {
        gf::Size client = (*attached_window()).client_size();
        bounds = {0, 0, client.width, client.height};
    }
    arrange_self(bounds);
    panel_.x = std::floor((bounds.width - panel_.width) / 2);
    panel_.y = std::floor((bounds.height - panel_.height) / 2);
    for (std::size_t i = 0; i < controls_.size(); ++i) {
        gf::Rect position = (*controls_[i]).requested_bounds();
        position.x += panel_.x;
        position.y += panel_.y;
        set_child_layout(controls_[i], position);
    }
}
void EditorDialog::on_paint(gf::Painter& painter, gf::Rect) {
    gf::Rect bounds = committed_arranged_bounds();
    painter.fill_rect({0, 0, bounds.width, bounds.height}, gf::Color::rgba(33, 51, 73, 48));
    painter.draw_box_shadow(panel_, 3, {0, 7}, 22, 0, gf::Color::rgba(20, 35, 53, 80));
    painter.fill_rounded_rect(panel_, 3, gf::Color::rgba(244, 247, 250));
    painter.fill_rect({panel_.x + 1, panel_.y + 1, panel_.width - 2, 35}, gf::Color::rgba(215, 230, 247));
    painter.stroke_rounded_rect(panel_, 3, gf::Color::rgba(126, 150, 178), 1);
    painter.draw_text_utf8({panel_.x + 16, panel_.y + 24}, title(),
                           {gf::FontRole::control, 15, 600, false, 0.08}, gf::Color::rgba(35, 60, 86));
    painter.draw_line({panel_.x + 1, panel_.y + panel_.height - 53},
                      {panel_.x + panel_.width - 1, panel_.y + panel_.height - 53},
                      gf::Color::rgba(205, 217, 231), 1);
}
void EditorDialog::on_pointer(gf::PointerEvent& event) {
    event.handled = true;
}
void EditorDialog::on_key_preview(gf::KeyEvent& event) {
    if (event.action != gf::KeyAction::down) {
        return;
    }
    if (kind_ == EditorDialogKind::atlas_gallery &&
        (event.physical_key == gf::PhysicalKey::left || event.physical_key == gf::PhysicalKey::right)) {
        std::shared_ptr<Editor> editor = editor_.lock();
        if (editor) {
            (*editor).execute(event.physical_key == gf::PhysicalKey::left ? "atlas-previous" : "atlas-next");
        }
        event.handled = true;
        return;
    }
    if (event.physical_key == gf::PhysicalKey::escape) {
        std::shared_ptr<Editor> editor = editor_.lock();
        if (editor) {
            (*editor).close_editor_dialog();
        }
        event.handled = true;
    } else if (event.physical_key == gf::PhysicalKey::enter) {
        accept();
        event.handled = true;
    }
}
gf::SemanticDescriptor EditorDialog::semantic_descriptor() const {
    gf::SemanticDescriptor result = Control::semantic_descriptor();
    result.role = gf::SemanticRole::group;
    result.name = title();
    return result;
}
std::string EditorDialog::title() const {
    switch (kind_) {
    case EditorDialogKind::color:
        return "Edit Colors";
    case EditorDialogKind::resize:
        return "Resize";
    case EditorDialogKind::atlas_grid:
        return "Sprite sheet grid";
    case EditorDialogKind::icon_sizes:
        return "Icon sizes";
    case EditorDialogKind::cursor_sizes:
        return "Cursor sizes";
    case EditorDialogKind::hotspot:
        return "Cursor hotspot";
    case EditorDialogKind::atlas_gallery:
        return "Atlas frames";
    case EditorDialogKind::properties:
        return "Image properties";
    case EditorDialogKind::print_preview:
        return "Print preview";
    }
    return "";
}
void EditorDialog::atlas_changed(double) {
    if (!error_ || atlas_numbers_.size() != 6) {
        return;
    }
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    try {
        AtlasGrid grid;
        grid.columns = static_cast<int>((*atlas_numbers_[0]).value());
        grid.rows = static_cast<int>((*atlas_numbers_[1]).value());
        grid.margin_x = static_cast<int>((*atlas_numbers_[2]).value());
        grid.margin_y = static_cast<int>((*atlas_numbers_[3]).value());
        grid.spacing_x = static_cast<int>((*atlas_numbers_[4]).value());
        grid.spacing_y = static_cast<int>((*atlas_numbers_[5]).value());
        const Document& document = (*editor).document;
        const Image& sheet = document.atlas.kind == AtlasKind::Sheet ? document.atlas.sheet : document.image;
        Rect frame = grid.frame(sheet, 0);
        int unused_x =
            sheet.width - 2 * grid.margin_x - (grid.columns - 1) * grid.spacing_x - grid.columns * frame.w;
        int unused_y =
            sheet.height - 2 * grid.margin_y - (grid.rows - 1) * grid.spacing_y - grid.rows * frame.h;
        (*error_).set_text(std::to_string(grid.columns * grid.rows) + " frames · " + std::to_string(frame.w) +
                           " × " + std::to_string(frame.h) + " pixels · unused right/bottom: " +
                           std::to_string(unused_x) + "/" + std::to_string(unused_y) + " px");
        (*error_).set_foreground(gf::Color::rgba(45, 66, 88));
    } catch (const std::exception& exception) {
        (*error_).set_text(exception.what());
        (*error_).set_foreground(gf::Color::rgba(161, 49, 39));
    }
}
void EditorDialog::choose_hsv(double h, double s, double v) {
    hue = h;
    saturation = s;
    value = v;
    Color color = hsv_color(h, s, v);
    color.a = color_.a;
    set_color(color, false);
}
void EditorDialog::set_color(Color color, bool update_hsv) {
    color_ = color;
    synchronizing_ = true;
    if (update_hsv) {
        double r = color.r / 255.0, g = color.g / 255.0, b = color.b / 255.0;
        double maximum = std::max({r, g, b}), minimum = std::min({r, g, b}), delta = maximum - minimum;
        value = maximum;
        saturation = maximum == 0 ? 0 : delta / maximum;
        if (delta > 0) {
            if (maximum == r) {
                hue = std::fmod((g - b) / delta + 6, 6) / 6;
            } else if (maximum == g) {
                hue = ((b - r) / delta + 2) / 6;
            } else {
                hue = ((r - g) / delta + 4) / 6;
            }
        }
    }
    (*red_).set_value(color.r);
    (*green_).set_value(color.g);
    (*blue_).set_value(color.b);
    (*alpha_).set_value(color.a);
    (*hex_).set_text(to_hex(color));
    Lab lab = to_oklab(color);
    (*light_).set_value(lab.l);
    (*a_).set_value(lab.a);
    (*b_).set_value(lab.b);
    (*preview_).set_color(color);
    (*old_).set_color(original_);
    (*plane_).invalidate(gf::Dirty::paint);
    (*hue_strip_).invalidate(gf::Dirty::paint);
    (*primary_tab_).set_selected(!secondary_);
    (*secondary_tab_).set_selected(secondary_);
    synchronizing_ = false;
}
void EditorDialog::rgb_changed(double) {
    if (synchronizing_) {
        return;
    }
    set_color({static_cast<std::uint8_t>((*red_).value()), static_cast<std::uint8_t>((*green_).value()),
               static_cast<std::uint8_t>((*blue_).value()), static_cast<std::uint8_t>((*alpha_).value())});
}
void EditorDialog::lab_changed(double) {
    if (synchronizing_) {
        return;
    }
    Color color = from_oklab({(*light_).value(), (*a_).value(), (*b_).value()});
    color.a = color_.a;
    set_color(color);
}
void EditorDialog::hex_changed(const std::string& text) {
    if (synchronizing_) {
        return;
    }
    Color color;
    if (from_hex(text, color)) {
        color.a = color_.a;
        set_color(color);
    }
}
void EditorDialog::width_changed(double value) {
    if (synchronizing_ || !(*lock_).checked()) {
        return;
    }
    synchronizing_ = true;
    double intended = percent_ ? value : std::round(value * original_height_ / original_width_);
    double bounded = std::clamp(intended, 1.0, 32768.0);
    (*height_).set_value(bounded);
    if (bounded != intended) {
        (*width_).set_value(percent_ ? bounded : std::round(bounded * original_width_ / original_height_));
    }
    synchronizing_ = false;
}
void EditorDialog::height_changed(double value) {
    if (synchronizing_ || !(*lock_).checked()) {
        return;
    }
    synchronizing_ = true;
    double intended = percent_ ? value : std::round(value * original_width_ / original_height_);
    double bounded = std::clamp(intended, 1.0, 32768.0);
    (*width_).set_value(bounded);
    if (bounded != intended) {
        (*height_).set_value(percent_ ? bounded : std::round(bounded * original_height_ / original_width_));
    }
    synchronizing_ = false;
}
void EditorDialog::clicked(gf::ButtonBase& control) {
    std::string id(control.stable_id().value());
    if (id == "preview-print" || id == "preview-page-setup") {
        std::shared_ptr<Editor> editor = editor_.lock();
        if (editor) {
            (*editor).execute(id == "preview-print" ? "print" : "page-setup");
        }
        return;
    }
    if (id == "hotspot-pick") {
        std::shared_ptr<Editor> editor = editor_.lock();
        if (editor) {
            (*editor).pick_hotspot = true;
            (*editor).close_editor_dialog();
        }
        return;
    }
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    try {
        if (id == "dialog-cancel") {
            (*editor).close_editor_dialog();
            return;
        }
        if (id == "dialog-ok") {
            accept();
            return;
        }
        if (id == "dialog-color1" || id == "dialog-color2") {
            secondary_ = id == "dialog-color2";
            original_ = secondary_ ? (*editor).document.ink.secondary : (*editor).document.ink.primary;
            set_color(original_);
        }
        if (id.starts_with("dialog-basic-")) {
            set_color(ribbon_color(std::stoi(id.substr(13))));
        }
        if (id.starts_with("dialog-custom-")) {
            custom_slot_ = std::stoi(id.substr(14));
            set_color((*editor).custom_colors.colors[static_cast<std::size_t>(custom_slot_)]);
        }
        if (id == "dialog-original") {
            set_color(original_);
        }
        if (id == "dialog-add-custom") {
            (*editor).custom_colors.store(custom_slot_, color_);
            (*custom_[static_cast<std::size_t>(custom_slot_)]).set_color(color_);
            custom_slot_ = (custom_slot_ + 1) % 16;
        }
        if (id == "resize-percent" || id == "resize-pixels") {
            percent_ = id == "resize-percent";
            synchronizing_ = true;
            (*width_).set_value(percent_ ? 100 : original_width_);
            (*height_).set_value(percent_ ? 100 : original_height_);
            (*percent_tab_).set_selected(percent_);
            (*pixel_tab_).set_selected(!percent_);
            synchronizing_ = false;
        }
    } catch (const std::exception& exception) {
        (*error_).set_text(exception.what());
    }
}
void EditorDialog::accept() {
    std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    try {
        if (kind_ == EditorDialogKind::color) {
            Color parsed;
            if (!from_hex(std::string((*hex_).text()), parsed)) {
                (*error_).set_text("Enter a six-digit hex color, such as #4F81BD.");
                return;
            }
            Color& target = secondary_ ? (*editor).document.ink.secondary : (*editor).document.ink.primary;
            target = color_;
            (*editor).document.sync_curve();
            (*editor).document.sync_path();
        } else if (kind_ == EditorDialogKind::properties) {
            Document& document = (*editor).document;
            int width = static_cast<int>((*atlas_numbers_[0]).value());
            int height = static_cast<int>((*atlas_numbers_[1]).value());
            if (width != document.image.width || height != document.image.height) {
                document.resize(width, height, false);
            } else if ((*scale_).checked()) {
                document.checkpoint();
            }
            if ((*scale_).checked()) {
                for (std::size_t i = 0; i < document.image.pixels.size(); ++i) {
                    Color& pixel = document.image.pixels[i];
                    unsigned brightness = 54U * pixel.r + 183U * pixel.g + 19U * pixel.b;
                    std::uint8_t value = brightness >= 128U * 256U ? 255 : 0;
                    pixel.r = pixel.g = pixel.b = value;
                }
            }
            (*editor).jpeg_quality = static_cast<int>((*atlas_numbers_[2]).value());
        } else if (kind_ == EditorDialogKind::print_preview) {
            // Preview is read-only.
        } else if (kind_ == EditorDialogKind::atlas_gallery) {
            // Frame selection is immediate; closing the gallery changes no pixels.
        } else if (kind_ == EditorDialogKind::atlas_grid) {
            AtlasGrid grid;
            grid.columns = static_cast<int>((*atlas_numbers_[0]).value());
            grid.rows = static_cast<int>((*atlas_numbers_[1]).value());
            grid.margin_x = static_cast<int>((*atlas_numbers_[2]).value());
            grid.margin_y = static_cast<int>((*atlas_numbers_[3]).value());
            grid.spacing_x = static_cast<int>((*atlas_numbers_[4]).value());
            grid.spacing_y = static_cast<int>((*atlas_numbers_[5]).value());
            (*editor).document.configure_atlas(grid);
        } else if (kind_ == EditorDialogKind::icon_sizes || kind_ == EditorDialogKind::cursor_sizes) {
            const int values[] = {16, 24, 32, 48, 64, 96, 128, 256};
            std::vector<int> sizes;
            for (std::size_t i = 0; i < icon_sizes_.size(); ++i) {
                if ((*icon_sizes_[i]).checked()) {
                    sizes.push_back(values[i]);
                }
            }
            (*editor).document.make_icon_sizes(sizes, kind_ == EditorDialogKind::cursor_sizes);
        } else if (kind_ == EditorDialogKind::hotspot) {
            (*editor).document.set_hotspot(static_cast<int>((*atlas_numbers_[0]).value()),
                                           static_cast<int>((*atlas_numbers_[1]).value()));
        } else {
            double width = (*width_).value() * (percent_ ? original_width_ / 100.0 : 1);
            double height = (*height_).value() * (percent_ ? original_height_ / 100.0 : 1);
            if (width < 1 || height < 1 || width > 32768 || height > 32768) {
                (*error_).set_text("Choose dimensions between 1 and 32768 pixels.");
                return;
            }
            if ((*skew_horizontal_).value() != 0 || (*skew_vertical_).value() != 0) {
                (*editor).request_skew(static_cast<int>(std::round(width)),
                                       static_cast<int>(std::round(height)), (*scale_).checked(),
                                       (*skew_horizontal_).value(), (*skew_vertical_).value());
            } else {
                (*editor).document.resize(static_cast<int>(std::round(width)),
                                          static_cast<int>(std::round(height)), (*scale_).checked());
            }
        }
        if (!(*editor).pending_save_path.empty()) {
            (*editor).save_path((*editor).pending_save_path);
        }
        (*editor).refresh();
        (*editor).complete_deferred_save();
    } catch (const std::exception& exception) {
        (*error_).set_text(exception.what());
    }
}
} // namespace paint::forms
