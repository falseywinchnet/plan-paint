#include "forms/display.hpp"
#include "forms/editor.hpp"
#include "localization.hpp"
#include <algorithm>
namespace paint::forms {
namespace gf = gui_forms;
namespace {
Image effect_preview(const IconFrame& frame, int zoom) {
    if (zoom <= 1) {
        return render_asset_preview(frame, 355, 192);
    }
    IconFrame enlarged;
    enlarged.image.reset(frame.image.width * zoom, frame.image.height * zoom, {0, 0, 0, 0});
    for (int y = 0; y < enlarged.image.height; ++y) {
        for (int x = 0; x < enlarged.image.width; ++x) {
            enlarged.image.set(x, y, frame.image.get(x / zoom, y / zoom));
        }
    }
    return render_asset_preview(enlarged, 355, 192);
}
} // namespace
void EditorDialog::initialize_icon_effects() {
    panel_ = {0, 0, 780, 510};
    const std::shared_ptr<Editor> editor = editor_.lock();
    Document& document = (*editor).document;
    document.sync_atlas();
    effect_source_ = document.image;
    effect_revision_ = document.revision;
    effect_epoch_ = document.atlas_epoch;
    effect_current_ = document.atlas.active;
    label("effect-scope",
          tr("Current image") + " · " + std::to_string(effect_source_.width) + " × " +
              std::to_string(effect_source_.height),
          {20, 48, 320, 28});
    effect_choice_ = gf::make_control<gf::ComboBox>(gf::StableId("effect-choice"));
    (*effect_choice_)
        .set_items({tr("Brightness / contrast"), tr("Hue / saturation"), tr("Grayscale"), tr("Opacity %"),
                    tr("Blur"), tr("Sharpen"), tr("Shadow"), tr("Fit artwork within image")});
    (*effect_choice_).set_selected_index(0);
    (*effect_choice_).set_accessible_name(tr("Effects"));
    put(effect_choice_, {350, 48, 410, 28});
    label("effect-before-label", tr("Original"), {20, 84, 355, 24});
    label("effect-after-label", tr("Preview"), {405, 84, 355, 24});
    effect_before_ = gf::make_control<gf::RasterCanvas>(gf::StableId("effect-before"));
    effect_after_ = gf::make_control<gf::RasterCanvas>(gf::StableId("effect-after"));
    (*effect_before_).set_accessible_name(tr("Original"));
    (*effect_after_).set_accessible_name(tr("Preview"));
    put(effect_before_, {20, 112, 355, 192});
    put(effect_after_, {405, 112, 355, 192});
    for (int index = 0; index < 4; ++index) {
        const std::shared_ptr<gf::Label> caption =
            gf::make_control<gf::Label>(gf::StableId("effect-label-" + std::to_string(index)));
        const std::shared_ptr<gf::NumericUpDown> value =
            gf::make_control<gf::NumericUpDown>(gf::StableId("effect-value-" + std::to_string(index)));
        put(caption, {20 + index * 185.0, 312, 175, 24});
        put(value, {20 + index * 185.0, 340, 175, 28});
        effect_labels_.push_back(caption);
        effect_values_.push_back(value);
        subscriptions_.push_back((*value).value_changed().subscribe(
            *this, gf::Delegate<double>::bind<EditorDialog, &EditorDialog::effect_number_changed>(*this)));
    }
    label("effect-color-label", tr("Color"), {20, 378, 80, 28});
    effect_color_ = gf::make_control<gf::TextBox>(gf::StableId("effect-color"));
    (*effect_color_).set_text("#000000");
    (*effect_color_).set_accessible_name(tr("Color"));
    put(effect_color_, {110, 378, 140, 28});
    effect_status_ = gf::make_control<gf::TextBox>(gf::StableId("effect-status"));
    (*effect_status_).set_read_only(true);
    (*effect_status_).set_multiline(true);
    (*effect_status_).set_word_wrap(true);
    (*effect_status_).set_accessible_name(tr("Preview"));
    put(effect_status_, {270, 378, 490, 64});
    error_ = gf::make_control<gf::Label>(gf::StableId("dialog-error"));
    (*error_).set_text_wrapping(gf::TextWrapping::word);
    put(error_, {20, 444, 740, 26});
    const std::shared_ptr<gf::Button> reset = button("effect-reset", tr("Reset"), {20, 474, 125, 28});
    effect_apply_ = button("effect-apply", tr("Apply"), {550, 474, 100, 28});
    button("dialog-cancel", tr("Close"), {660, 474, 100, 28});
    for (const std::shared_ptr<gf::Button>& control : {reset, effect_apply_}) {
        subscriptions_.push_back((*control).clicked().subscribe(
            *this, gf::Delegate<gf::ButtonBase&>::bind<EditorDialog, &EditorDialog::effect_clicked>(*this)));
    }
    subscriptions_.push_back(
        (*effect_choice_)
            .selected_index_changed()
            .subscribe(*this, gf::Delegate<std::optional<std::size_t>>::bind<
                                  EditorDialog, &EditorDialog::effect_choice_changed>(*this)));
    subscriptions_.push_back(
        (*effect_color_)
            .text_changed()
            .subscribe(
                *this,
                gf::Delegate<const std::string&>::bind<EditorDialog, &EditorDialog::effect_text_changed>(
                    *this)));
    effect_choice_changed(0);
}
void EditorDialog::effect_choice_changed(std::optional<std::size_t>) {
    synchronizing_ = true;
    const std::size_t choice = (*effect_choice_).selected_index().value_or(0);
    const char* names[8][4] = {{"Brightness %", "Contrast %", "", ""},
                               {"Hue", "Saturation %", "", ""},
                               {"", "", "", ""},
                               {"Opacity %", "", "", ""},
                               {"Radius (px)", "", "", ""},
                               {"Radius (px)", "Strength %", "", ""},
                               {"Radius (px)", "Offset X", "Offset Y", "Opacity %"},
                               {"Padding", "", "", ""}};
    const double minimum[8][4] = {{-100, -100, 0, 0}, {-180, 0, 0, 0}, {0, 0, 0, 0},       {0, 0, 0, 0},
                                  {0, 0, 0, 0},       {0, 0, 0, 0},    {0, -128, -128, 0}, {0, 0, 0, 0}};
    const double maximum[8][4] = {{100, 100, 0, 0}, {180, 200, 0, 0}, {0, 0, 0, 0},        {100, 0, 0, 0},
                                  {32, 0, 0, 0},    {32, 300, 0, 0},  {32, 128, 128, 100}, {127, 0, 0, 0}};
    const double defaults[8][4] = {{0, 0, 0, 0}, {0, 100, 0, 0}, {0, 0, 0, 0},  {100, 0, 0, 0},
                                   {2, 0, 0, 0}, {2, 100, 0, 0}, {2, 2, 2, 55}, {2, 0, 0, 0}};
    for (int index = 0; index < 4; ++index) {
        const bool visible = names[choice][index][0] != 0;
        (*effect_labels_[index]).set_text(tr(names[choice][index]));
        (*effect_labels_[index]).set_visible(visible);
        (*effect_values_[index]).set_visible(visible);
        (*effect_values_[index]).set_accessible_name(tr(names[choice][index]));
        (*effect_values_[index]).set_range(minimum[choice][index], maximum[choice][index]);
        (*effect_values_[index]).set_value(defaults[choice][index]);
    }
    (*effect_color_).set_enabled(choice == 6);
    (*effect_color_).set_text("#000000");
    synchronizing_ = false;
    render_icon_effects();
}
void EditorDialog::effect_number_changed(double) {
    if (!synchronizing_) {
        render_icon_effects();
    }
}
void EditorDialog::effect_text_changed(const std::string&) {
    if (!synchronizing_) {
        render_icon_effects();
    }
}
void EditorDialog::render_icon_effects() {
    if (!attached_window()) {
        return;
    }
    (*effect_apply_).set_enabled(false);
    (*error_).set_text("");
    effect_result_ = {};
    try {
        IconEffectSettings settings;
        settings.effect = static_cast<IconEffect>((*effect_choice_).selected_index().value_or(0));
        const double first = (*effect_values_[0]).value(), second = (*effect_values_[1]).value();
        switch (settings.effect) {
        case IconEffect::BrightnessContrast:
            settings.brightness = first;
            settings.contrast = second;
            break;
        case IconEffect::HueSaturation:
            settings.hue = first;
            settings.saturation = second;
            break;
        case IconEffect::Opacity:
            settings.opacity = first;
            break;
        case IconEffect::Blur:
            settings.radius = static_cast<int>(first);
            break;
        case IconEffect::Sharpen:
            settings.radius = static_cast<int>(first);
            settings.strength = second;
            break;
        case IconEffect::Shadow:
            settings.radius = static_cast<int>(first);
            settings.offset_x = static_cast<int>(second);
            settings.offset_y = static_cast<int>((*effect_values_[2]).value());
            settings.opacity = (*effect_values_[3]).value();
            if (!from_hex(std::string((*effect_color_).text()), settings.shadow)) {
                throw std::runtime_error(tr("Invalid color"));
            }
            break;
        case IconEffect::Fit:
            settings.padding = static_cast<int>(first);
            break;
        case IconEffect::Grayscale:
            break;
        }
        effect_result_ = render_icon_effect(effect_source_, settings);
        (*effect_apply_)
            .set_enabled(!std::equal(effect_result_.image.pixels.begin(), effect_result_.image.pixels.end(),
                                     effect_source_.pixels.begin(), equal));
    } catch (const std::exception& exception) {
        (*error_).set_text(exception.what());
    }
    IconFrame before, after;
    before.image = effect_source_;
    after.image = effect_result_.image.pixels.empty() ? effect_source_ : effect_result_.image;
    if (!effect_result_.overflow.pixels.empty()) {
        after.image = effect_result_.overflow;
        const Rect bounds = effect_result_.bounds;
        for (int x = bounds.x; x < bounds.x + bounds.w; ++x) {
            after.image.set(x, bounds.y, {235, 40, 100, 255});
            after.image.set(x, bounds.y + bounds.h - 1, {235, 40, 100, 255});
        }
        for (int y = bounds.y; y < bounds.y + bounds.h; ++y) {
            after.image.set(bounds.x, y, {235, 40, 100, 255});
            after.image.set(bounds.x + bounds.w - 1, y, {235, 40, 100, 255});
        }
    }
    const int zoom = std::max(1, std::min(355 / std::max(before.image.width, after.image.width),
                                          192 / std::max(before.image.height, after.image.height)));
    publish_image(effect_preview(before, zoom), *effect_before_);
    publish_image(effect_preview(after, zoom), *effect_after_);
    (*effect_status_)
        .set_text(effect_result_.overflow.pixels.empty()
                      ? tr("Current image") + " · " + std::to_string(effect_source_.width) + " × " +
                            std::to_string(effect_source_.height)
                      : tr("Pink border: image bounds. Pixels outside are clipped.") + " " +
                            std::to_string(effect_result_.clipped_pixels));
}
void EditorDialog::effect_clicked(gf::ButtonBase& control) {
    const std::shared_ptr<EditorDialog> keep_alive =
        std::static_pointer_cast<EditorDialog>(shared_from_this());
    const std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    if (control.stable_id().value() == "effect-reset") {
        effect_choice_changed({});
        return;
    }
    if (!(*effect_apply_).enabled()) {
        return;
    }
    try {
        Document& document = (*editor).document;
        if (document.revision != effect_revision_ || document.atlas_epoch != effect_epoch_ ||
            document.atlas.active != effect_current_ || document.image.width != effect_source_.width ||
            document.image.height != effect_source_.height ||
            !std::equal(document.image.pixels.begin(), document.image.pixels.end(),
                        effect_source_.pixels.begin(), effect_source_.pixels.end(), equal)) {
            throw std::runtime_error(tr("Image changed. Reopen Effects."));
        }
        document.require_rgba_transform();
        document.checkpoint();
        document.image = std::move(effect_result_.image);
        (*editor).close_editor_dialog();
        (*editor).refresh();
    } catch (const std::exception& exception) {
        (*error_).set_text(exception.what());
    }
}
} // namespace paint::forms
