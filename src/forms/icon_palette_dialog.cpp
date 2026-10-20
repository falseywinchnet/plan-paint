#include "forms/display.hpp"
#include "forms/editor.hpp"
#include "icon_palette.hpp"
#include "localization.hpp"
#include <algorithm>
#include <sstream>
namespace paint::forms {
namespace gf = gui_forms;
void EditorDialog::initialize_icon_palette() {
    panel_ = {0, 0, 780, 510};
    const std::shared_ptr<Editor> editor = editor_.lock();
    Document& document = (*editor).document;
    document.sync_atlas();
    palette_source_ = document.atlas.icons[document.atlas.active];
    palette_revision_ = document.revision;
    palette_epoch_ = document.atlas_epoch;
    palette_current_ = document.atlas.active;
    label("palette-depth-label", tr("Color depth"), {20, 48, 150, 28});
    palette_depth_ = gf::make_control<gf::ComboBox>(gf::StableId("palette-depth"));
    (*palette_depth_).set_accessible_name(tr("Color depth"));
    (*palette_depth_).set_items({"1 bpp · 2", "4 bpp · 16", "8 bpp · 256"});
    (*palette_depth_).set_selected_index(1);
    put(palette_depth_, {180, 48, 195, 28});
    label("palette-dither-label", tr("Dithering"), {405, 48, 150, 28});
    palette_dither_ = gf::make_control<gf::ComboBox>(gf::StableId("palette-dither"));
    (*palette_dither_).set_accessible_name(tr("Dithering"));
    (*palette_dither_).set_items({tr("None"), "Floyd–Steinberg"});
    (*palette_dither_).set_selected_index(0);
    put(palette_dither_, {565, 48, 195, 28});
    label("palette-threshold-label", tr("Alpha threshold"), {20, 88, 150, 28});
    palette_threshold_ = number("palette-threshold", {180, 88, 195, 28}, 1, 255, 128);
    label("palette-matte-label", tr("Matte RGB"), {405, 88, 150, 28});
    palette_matte_ = gf::make_control<gf::TextBox>(gf::StableId("palette-matte"));
    (*palette_matte_).set_accessible_name(tr("Matte RGB"));
    (*palette_matte_).set_text("#FFFFFF");
    put(palette_matte_, {565, 88, 195, 28});
    label("palette-before-label", tr("Original"), {20, 125, 355, 24});
    label("palette-after-label", tr("Indexed preview"), {405, 125, 355, 24});
    palette_before_ = gf::make_control<gf::RasterCanvas>(gf::StableId("palette-before"));
    palette_after_ = gf::make_control<gf::RasterCanvas>(gf::StableId("palette-after"));
    (*palette_before_).set_accessible_name(tr("Original"));
    (*palette_after_).set_accessible_name(tr("Indexed preview"));
    put(palette_before_, {20, 153, 355, 200});
    put(palette_after_, {405, 153, 355, 200});
    palette_status_ = gf::make_control<gf::TextBox>(gf::StableId("palette-status"));
    (*palette_status_).set_read_only(true);
    (*palette_status_).set_multiline(true);
    (*palette_status_).set_auto_scroll(true);
    (*palette_status_).set_accessible_name(tr("Palette conversion"));
    put(palette_status_, {20, 363, 740, 68});
    label("palette-contract", tr("Current image only. Further painting returns it to RGBA output."),
          {20, 432, 740, 20});
    error_ = gf::make_control<gf::Label>(gf::StableId("dialog-error"));
    put(error_, {20, 453, 740, 20});
    palette_apply_ = button("palette-apply", tr("Apply"), {550, 474, 100, 28});
    button("dialog-cancel", tr("Close"), {660, 474, 100, 28});
    subscriptions_.push_back(
        (*palette_apply_)
            .clicked()
            .subscribe(
                *this,
                gf::Delegate<gf::ButtonBase&>::bind<EditorDialog, &EditorDialog::palette_clicked>(*this)));
    for (const std::shared_ptr<gf::ComboBox>& control : {palette_depth_, palette_dither_}) {
        subscriptions_.push_back((*control).selected_index_changed().subscribe(
            *this,
            gf::Delegate<std::optional<std::size_t>>::bind<EditorDialog,
                                                           &EditorDialog::palette_choice_changed>(*this)));
    }
    subscriptions_.push_back(
        (*palette_threshold_)
            .value_changed()
            .subscribe(*this, gf::Delegate<double>::bind<EditorDialog, &EditorDialog::palette_number_changed>(
                                  *this)));
    subscriptions_.push_back(
        (*palette_matte_)
            .text_changed()
            .subscribe(
                *this,
                gf::Delegate<const std::string&>::bind<EditorDialog, &EditorDialog::palette_text_changed>(
                    *this)));
}
void EditorDialog::palette_choice_changed(std::optional<std::size_t>) {
    render_icon_palette();
}
void EditorDialog::palette_number_changed(double) {
    render_icon_palette();
}
void EditorDialog::palette_text_changed(const std::string&) {
    render_icon_palette();
}
void EditorDialog::render_icon_palette() {
    if (!attached_window()) {
        return;
    }
    palette_result_ = {};
    (*palette_apply_).set_enabled(false);
    (*error_).set_text("");
    (*palette_status_).set_text("");
    publish_image(render_asset_preview(palette_source_, 355, 200), *palette_before_);
    publish_image(render_asset_preview(palette_source_, 355, 200), *palette_after_);
    try {
        IconPaletteOptions options;
        const unsigned depths[] = {1, 4, 8};
        options.bits_per_pixel = depths[(*palette_depth_).selected_index().value_or(1)];
        options.alpha_threshold = static_cast<unsigned>((*palette_threshold_).value());
        options.dither = (*palette_dither_).selected_index().value_or(0) == 0
                             ? IconPaletteDither::None
                             : IconPaletteDither::FloydSteinberg;
        if (!from_hex(std::string((*palette_matte_).text()), options.matte)) {
            throw std::runtime_error(tr("Enter a matte color as #RRGGBB."));
        }
        IconPaletteResult converted = convert_icon_palette(palette_source_, options);
        std::ostringstream text;
        text << tr("Current image") << ": " << converted.frame.image.width << " × "
             << converted.frame.image.height << " · " << options.bits_per_pixel << " bpp DIB · "
             << tr("Palette entries") << ": " << converted.palette.size() << '/'
             << (1u << options.bits_per_pixel) << '\n'
             << tr("Transparent pixels") << ": " << converted.transparent_pixels << " · "
             << tr("Partial alpha converted") << ": " << converted.partial_pixels << '\n'
             << "Alpha < " << options.alpha_threshold << " → 0; Alpha ≥ " << options.alpha_threshold
             << " → 255 · " << tr("Matte RGB") << ": " << to_hex(options.matte);
        (*palette_status_).set_text(text.str());
        palette_result_ = std::move(converted.frame);
        publish_image(render_asset_preview(palette_result_, 355, 200), *palette_after_);
        const bool unchanged = icon_source_matches(palette_source_) &&
                               (*palette_source_.source).payload == (*palette_result_.source).payload;
        (*palette_apply_).set_enabled(!unchanged);
    } catch (const std::exception& exception) {
        (*error_).set_text(tr(exception.what()));
    }
}
void EditorDialog::palette_clicked(gf::ButtonBase&) {
    const std::shared_ptr<EditorDialog> keep_alive =
        std::static_pointer_cast<EditorDialog>(shared_from_this());
    const std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor || !(*palette_apply_).enabled() || !palette_result_.source) {
        return;
    }
    try {
        Document& document = (*editor).document;
        if (document.revision != palette_revision_ || document.atlas_epoch != palette_epoch_ ||
            document.atlas.active != palette_current_ ||
            document.image.width != palette_source_.image.width ||
            document.image.height != palette_source_.image.height ||
            !std::equal(document.image.pixels.begin(), document.image.pixels.end(),
                        palette_source_.image.pixels.begin(), palette_source_.image.pixels.end(), equal)) {
            throw std::runtime_error(tr("Image changed. Reopen Palette conversion."));
        }
        document.require_rgba_transform();
        Image prepared = palette_result_.image;
        document.checkpoint();
        document.image = std::move(prepared);
        document.atlas.icons[palette_current_] = std::move(palette_result_);
        ++document.atlas_epoch;
        (*editor).close_editor_dialog();
        (*editor).refresh();
    } catch (const std::exception& exception) {
        (*error_).set_text(exception.what());
    }
}
} // namespace paint::forms
