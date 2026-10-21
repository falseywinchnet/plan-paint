#pragma once
#include "asset_browser.hpp"
#include "carpet.hpp"
#include "color_tools.hpp"
#include "icon_export.hpp"
#include "icon_sheet.hpp"
#include "icon_states.hpp"
#include "image.hpp"
#include <gui_forms/basic_controls.hpp>
#include <gui_forms/canvas.hpp>
#include <gui_forms/controls/panel/combo_box/combo_box.hpp>
#include <gui_forms/controls/panel/list_box/list_box.hpp>
#include <gui_forms/controls/panel/numeric_up_down/numeric_up_down.hpp>
#include <gui_forms/controls/panel/text_box/text_box.hpp>
#include <gui_forms/controls/range_control/track_bar/track_bar.hpp>
#include <memory>
#include <thread>
namespace paint::forms {
class Editor;
class SwatchButton;
class EditorDialog;
struct CarpetRenderJob;
struct DitherRenderJob;
struct IconSheetJob;
struct IconStateJob;
struct IconExportJob;
struct AssetBrowserWorker;
struct AssetBrowserResult;
enum class EditorDialogKind {
    dither,
    tool_size,
    carpet,
    about,
    color,
    gradient_color,
    resize,
    atlas_grid,
    icon_sizes,
    cursor_sizes,
    hotspot,
    atlas_gallery,
    icon_sheet,
    icon_export,
    asset_browser,
    alpha_tools,
    icon_palette,
    icon_states,
    properties,
    settings,
    english_help,
    print_preview,
    linux_print,
    linux_page_setup
};
class ColorPlane final : public gui_forms::Control {
  public:
    ColorPlane(gui_forms::StableId id, std::weak_ptr<EditorDialog> dialog, bool hue_strip);
    void on_paint(gui_forms::Painter& painter, gui_forms::Rect damage) override;
    void on_pointer(gui_forms::PointerEvent& event) override;
    void on_detaching_from_window(gui_forms::Window& window) noexcept override;

  private:
    std::weak_ptr<EditorDialog> dialog_;
    bool hue_strip_;
    gui_forms::ImageId mosaic_image_;
    const ColorMosaic* cached_mosaic_ = nullptr;
    int mosaic_width_ = 0, mosaic_height_ = 0;
    std::vector<std::uint8_t> mosaic_cells_;
    std::size_t selected_cell_ = 96;
    std::vector<std::vector<Point>> selected_contours_;
    void paint_mosaic(gui_forms::Painter& painter, EditorDialog& dialog, gui_forms::Rect bounds);
};
class EditorDialog final : public gui_forms::Control {
  public:
    EditorDialog(gui_forms::StableId id, std::weak_ptr<Editor> editor, EditorDialogKind kind, bool secondary);
    ~EditorDialog() override;
    void on_attached_to_window() override;
    void on_detaching_from_window(gui_forms::Window& window) noexcept override;
    void deliver_carpet();
    void deliver_dither();
    void deliver_icon_sheet();
    void deliver_icon_states(const std::shared_ptr<IconStateJob>& job);
    void deliver_icon_export();
    void deliver_asset_browser(std::shared_ptr<const AssetBrowserResult> result);
    static constexpr bool initialize_tree_after_construction = true;
    void initialize_control_tree();
    void arrange(gui_forms::Rect bounds) override;
    void on_paint(gui_forms::Painter& painter, gui_forms::Rect damage) override;
    void on_pointer(gui_forms::PointerEvent& event) override;
    void on_key_preview(gui_forms::KeyEvent& event) override;
    gui_forms::SemanticDescriptor semantic_descriptor() const override;
    int interface_hue() const;
    void choose_hsv(double hue, double saturation, double value);
    double hue = 0, saturation = 0, value = 0;
    Color plane_color(double h, double s, double level) const;
    const ColorMosaic& color_mosaic(double hue) const;
    PickerSpace picker_space = PickerSpace::RGB;
    bool mosaic = false;

  private:
    void initialize_icon_states();
    void stop_icon_states();
    void render_icon_states();
    void show_icon_state_image(std::optional<std::size_t>);
    void state_choice_changed(std::optional<std::size_t>);
    void state_preset_changed(std::optional<std::size_t>);
    void state_number_changed(double);
    void state_clicked(gui_forms::ButtonBase&);
    std::shared_ptr<const ImageContainer> state_source_;
    std::vector<int> state_selected_;
    int state_current_ = -1;
    std::string state_source_path_;
    bool state_updating_ = false;
    std::shared_ptr<IconStateJob> state_job_;
    std::thread state_worker_;
    std::shared_ptr<gui_forms::ComboBox> state_scope_, state_preset_, state_policy_, state_image_;
    std::shared_ptr<gui_forms::NumericUpDown> state_saturation_, state_brightness_, state_opacity_;
    std::shared_ptr<gui_forms::RasterCanvas> state_before_, state_after_;
    std::shared_ptr<gui_forms::TextBox> state_report_;
    std::shared_ptr<gui_forms::Button> state_export_, state_save_report_;
    void initialize_asset_browser();
    void stop_asset_browser();
    void request_assets(bool scan);
    void asset_clicked(gui_forms::ButtonBase&);
    void asset_filter_changed(std::optional<std::size_t>);
    void asset_input_changed(const std::string&);
    void asset_selected(const gui_forms::ListSelectionChange&);
    void asset_activated(std::size_t index);
    void asset_representation_changed(std::optional<std::size_t>);
    void asset_bookmark_changed(std::optional<std::size_t>);
    void clear_asset_preview();
    void show_asset_representation();
    std::shared_ptr<AssetBrowserWorker> asset_worker_;
    std::shared_ptr<const AssetPreview> asset_preview_;
    AssetListing asset_listing_;
    std::uint64_t asset_generation_ = 0;
    std::vector<std::string> asset_bookmarks_;
    bool asset_bookmarks_valid_ = true;
    bool asset_open_after_preview_ = false;
    std::shared_ptr<gui_forms::TextBox> asset_location_, asset_search_, asset_details_;
    std::shared_ptr<gui_forms::ListBox> asset_list_;
    std::shared_ptr<gui_forms::ComboBox> asset_filter_, asset_representations_, asset_bookmark_;
    std::shared_ptr<gui_forms::RasterCanvas> asset_canvas_;
    std::shared_ptr<gui_forms::Label> asset_status_;
    std::shared_ptr<gui_forms::Button> asset_open_, asset_stamp_, asset_copy_;
    void initialize_icon_export();
    void review_icon_export();
    void stop_icon_export();
    void icon_export_clicked(gui_forms::ButtonBase&);
    void icon_export_choice_changed(std::optional<std::size_t>);
    void icon_export_text_changed(const std::string&);
    void icon_export_destination_changed(const std::string&);
    void start_icon_export(bool execute);
    std::shared_ptr<const ImageContainer> export_source_;
    std::vector<int> export_selected_;
    int export_current_ = -1;
    std::string export_directory_;
    std::shared_ptr<IconExportJob> export_job_;
    std::thread export_worker_;
    std::shared_ptr<gui_forms::ComboBox> export_scope_, export_collision_;
    std::shared_ptr<gui_forms::TextBox> export_prefix_, export_manifest_, export_destination_;
    std::shared_ptr<gui_forms::Label> export_status_;
    std::shared_ptr<gui_forms::Button> export_folder_, export_review_, export_run_, export_cancel_,
        export_report_;
    void initialize_icon_palette();
    void render_icon_palette();
    void palette_clicked(gui_forms::ButtonBase&);
    void palette_choice_changed(std::optional<std::size_t>);
    void palette_number_changed(double);
    void palette_text_changed(const std::string&);
    IconFrame palette_source_, palette_result_;
    std::uint64_t palette_revision_ = 0, palette_epoch_ = 0;
    int palette_current_ = -1;
    std::shared_ptr<gui_forms::ComboBox> palette_depth_, palette_dither_;
    std::shared_ptr<gui_forms::NumericUpDown> palette_threshold_;
    std::shared_ptr<gui_forms::TextBox> palette_matte_, palette_status_;
    std::shared_ptr<gui_forms::RasterCanvas> palette_before_, palette_after_;
    std::shared_ptr<gui_forms::Button> palette_apply_;
    void initialize_alpha_tools();
    void render_alpha_tools();
    void alpha_clicked(gui_forms::ButtonBase&);
    void alpha_view_changed(std::optional<std::size_t>);
    Image alpha_source_, alpha_mask_, alpha_result_;
    std::uint64_t alpha_revision_ = 0, alpha_epoch_ = 0;
    int alpha_current_ = -1;
    std::shared_ptr<gui_forms::RasterCanvas> alpha_before_, alpha_after_;
    std::shared_ptr<gui_forms::ComboBox> alpha_view_;
    std::shared_ptr<gui_forms::CheckBox> alpha_resize_;
    std::shared_ptr<gui_forms::TextBox> alpha_status_;
    std::shared_ptr<gui_forms::Button> alpha_apply_;
    void initialize_icon_sheet();
    void render_icon_sheet_preview();
    void stop_icon_sheet();
    void icon_sheet_number_changed(double);
    void icon_sheet_choice_changed(std::optional<std::size_t>);
    void icon_sheet_zoom_changed(std::optional<std::size_t>);
    void icon_sheet_clicked(gui_forms::ButtonBase&);
    std::shared_ptr<const ImageContainer> sheet_source_;
    std::vector<int> sheet_selected_;
    int sheet_current_ = -1;
    Color sheet_custom_;
    std::shared_ptr<IconSheetJob> sheet_job_;
    std::thread sheet_worker_;
    std::shared_ptr<gui_forms::ComboBox> sheet_scope_, sheet_background_, sheet_zoom_;
    std::shared_ptr<gui_forms::NumericUpDown> sheet_columns_, sheet_padding_, sheet_gap_, sheet_font_;
    std::shared_ptr<gui_forms::CheckBox> sheet_labels_;
    std::shared_ptr<gui_forms::RasterCanvas> sheet_preview_;
    std::shared_ptr<gui_forms::Control> sheet_viewport_;
    std::shared_ptr<gui_forms::Label> sheet_status_;
    std::shared_ptr<gui_forms::Button> sheet_export_;
    void theme_changed(double hue);
    std::shared_ptr<gui_forms::TrackBar> theme_hue_;
    std::shared_ptr<gui_forms::CheckBox> recovery_enabled_;
    std::shared_ptr<gui_forms::NumericUpDown> recovery_seconds_;
    void initialize_dither();
    void render_dither();
    void stop_dither();
    void accept_dither();
    void dither_count_changed(double);
    void dither_pattern_changed(std::optional<std::size_t>);
    std::shared_ptr<DitherRenderJob> dither_job_;
    std::thread dither_worker_;
    std::shared_ptr<gui_forms::NumericUpDown> dither_count_;
    std::shared_ptr<gui_forms::ComboBox> dither_pattern_;
    std::shared_ptr<gui_forms::RasterCanvas> dither_preview_;
    std::shared_ptr<gui_forms::Label> dither_status_;
    std::shared_ptr<gui_forms::ComboBox> shape_gesture_, language_;
    std::shared_ptr<gui_forms::CheckBox> canvas_controls_;
    CarpetParameters carpet_;
    Image carpet_image_;
    std::shared_ptr<CarpetRenderJob> carpet_job_;
    std::thread carpet_worker_;
    std::shared_ptr<gui_forms::RasterCanvas> carpet_preview_;
    std::shared_ptr<gui_forms::ComboBox> carpet_presets_;
    std::shared_ptr<gui_forms::CheckBox> carpet_hillshade_;
    std::shared_ptr<gui_forms::TextBox> carpet_color_;
    std::vector<std::shared_ptr<gui_forms::TrackBar>> carpet_sliders_;
    std::vector<std::shared_ptr<gui_forms::Label>> carpet_values_;
    void initialize_carpet();
    void sync_carpet();
    void carpet_changed(double value);
    void carpet_color_changed(const std::string& value);
    void carpet_preset_changed(std::optional<std::size_t> index);
    void render_carpet_preview();
    void stop_carpet();
    std::weak_ptr<Editor> editor_;
    mutable std::array<std::unique_ptr<ColorMosaic>, 48> mosaics_;
    std::shared_ptr<gui_forms::ComboBox> background_, alpha_background_;
    std::shared_ptr<gui_forms::CheckBox> mosaic_;
    void color_space_changed(PickerSpace space);
    EditorDialogKind kind_;
    bool secondary_ = false, synchronizing_ = false, percent_ = false;
    Color original_, color_;
    int original_width_ = 1, original_height_ = 1, custom_slot_ = 0;
    gui_forms::Rect panel_;
    std::vector<gui_forms::Control::Ptr> controls_;
    std::vector<gui_forms::SubscriptionToken> subscriptions_;
    std::vector<std::shared_ptr<SwatchButton>> custom_;
    std::shared_ptr<SwatchButton> old_, preview_;
    std::shared_ptr<ColorPlane> plane_, hue_strip_;
    std::shared_ptr<gui_forms::NumericUpDown> red_, green_, blue_, alpha_, light_, a_, b_, width_, height_,
        skew_horizontal_, skew_vertical_;
    std::shared_ptr<gui_forms::TextBox> hex_, alpha_background_color_;
    std::shared_ptr<gui_forms::ComboBox> printer_;
    std::shared_ptr<gui_forms::CheckBox> lock_, scale_, rotate_view_;
    std::shared_ptr<gui_forms::Label> error_;
    std::shared_ptr<gui_forms::Button> primary_tab_, secondary_tab_, pixel_tab_, percent_tab_, rgb_tab_,
        okhsl_tab_;
    void put(gui_forms::Control::Ptr control, gui_forms::Rect bounds);
    void label(const std::string& id, const std::string& text, gui_forms::Rect bounds, bool heading = false);
    std::shared_ptr<gui_forms::Button> button(const std::string& id, const std::string& text,
                                              gui_forms::Rect bounds);
    std::shared_ptr<gui_forms::NumericUpDown> number(const std::string& id, gui_forms::Rect bounds,
                                                     double minimum, double maximum, double value,
                                                     int decimals = 0);
    void clicked(gui_forms::ButtonBase& control);
    void rgb_changed(double value);
    void lab_changed(double value);
    void hex_changed(const std::string& value);
    void width_changed(double value);
    void height_changed(double value);
    void set_color(Color color, bool update_hsv = true);
    std::vector<std::shared_ptr<gui_forms::NumericUpDown>> atlas_numbers_;
    std::vector<std::shared_ptr<gui_forms::CheckBox>> icon_sizes_;
    std::shared_ptr<gui_forms::ComboBox> icon_policy_;
    std::shared_ptr<gui_forms::Label> icon_plan_;
    std::shared_ptr<gui_forms::TextBox> icon_manifest_;
    std::vector<int> chosen_icon_sizes() const;
    void icon_size_clicked(gui_forms::ButtonBase& control);
    void icon_policy_changed(std::optional<std::size_t> index);
    void refresh_icon_plan();
    std::string title() const;
    void atlas_changed(double value);
    void accept();
};
} // namespace paint::forms
